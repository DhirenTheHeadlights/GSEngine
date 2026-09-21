export module gse.fs:layout_store;

import gse.concurrency;
import gse.core;
import gse.log;
import gse.math;
import gse.meta;
import gse.win32;
import std;

export namespace gse::layout_store {
	struct owner {
		std::vector<std::string> names;
		std::vector<std::string> prefixes;

		auto operator==(const owner&) const -> bool = default;
	};

	auto submit(
		const std::filesystem::path& path,
		owner sections,
		std::string block
	) -> void;

	auto read(
		const std::filesystem::path& path
	) -> std::string;

	auto flush() -> void;

	auto trimmed(
		std::string_view s
	) -> std::string_view;

	auto section_name(
		std::string_view line
	) -> std::string_view;

	struct section {
		std::string name;
		std::map<std::string, std::string> values;
	};

	auto parse_sections(
		std::string_view text
	) -> std::vector<section>;
}

namespace gse::layout_store {
	struct pending_block {
		owner sections;
		std::string block;
	};

	struct file_state {
		bool needs_write = false;
		std::vector<pending_block> blocks;
	};

	class store : non_copyable, non_movable {
	public:
		store() = default;

		~store();

		auto submit(
			const std::filesystem::path& path,
			owner sections,
			std::string block
		) -> void;

		auto read(
			const std::filesystem::path& path
		) -> std::string;

		auto flush() -> void;

	private:
		auto write_pending() -> void;

		auto next_dirty() const -> std::filesystem::path;

		std::mutex m_mutex;
		std::condition_variable m_idle;
		std::unordered_map<std::filesystem::path, file_state> m_files;
		bool m_scheduled = false;
	};

	auto instance() -> store&;

	auto owned(
		const owner& sections,
		std::string_view name
	) -> bool;

	auto apply(
		std::string& content,
		const owner& sections,
		std::string_view block
	) -> void;

	auto read_disk(
		const std::filesystem::path& path
	) -> std::string;

	auto write_disk(
		const std::filesystem::path& path,
		std::string_view content
	) -> void;

	auto merged(
		const std::filesystem::path& path,
		std::span<const pending_block> blocks
	) -> std::string;
}

auto gse::layout_store::submit(const std::filesystem::path& path, owner sections, std::string block) -> void {
	instance().submit(path, std::move(sections), std::move(block));
}

auto gse::layout_store::read(const std::filesystem::path& path) -> std::string {
	return instance().read(path);
}

auto gse::layout_store::flush() -> void {
	instance().flush();
}

gse::layout_store::store::~store() = default;

auto gse::layout_store::store::submit(const std::filesystem::path& path, owner sections, std::string block) -> void {
	std::lock_guard _(m_mutex);
	file_state& state = m_files[path];
	if (const auto existing = std::ranges::find(state.blocks, sections, &pending_block::sections); existing != state.blocks.end()) {
		existing->block = std::move(block);
	}
	else {
		state.blocks.push_back({
			.sections = std::move(sections),
			.block = std::move(block),
		});
	}
	state.needs_write = true;
	if (m_scheduled) {
		return;
	}
	m_scheduled = true;
	task::post_io(
		[this] {
			write_pending();
		},
		trace_id<"layout_store::write">()
	);
}

auto gse::layout_store::store::read(const std::filesystem::path& path) -> std::string {
	std::vector<pending_block> blocks;
	{
		std::lock_guard _(m_mutex);
		if (const auto it = m_files.find(path); it != m_files.end()) {
			blocks = it->second.blocks;
		}
	}
	return merged(path, blocks);
}

auto gse::layout_store::store::flush() -> void {
	const time budget = seconds(5.f);
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<std::int64_t>(budget.as<milliseconds>()));

	std::unique_lock lock(m_mutex);
	while (m_scheduled) {
		if (m_idle.wait_until(lock, deadline) == std::cv_status::timeout && m_scheduled) {
			log::println(
				log::level::error,
				log::category::general,
				"layout_store: flush abandoned after {::s} with writes still pending",
				budget
			);
			return;
		}
	}
}

auto gse::layout_store::store::write_pending() -> void {
	std::unique_lock lock(m_mutex);
	while (true) {
		const std::filesystem::path path = next_dirty();
		if (path.empty()) {
			m_scheduled = false;
			m_idle.notify_all();
			return;
		}

		file_state& state = m_files.at(path);
		state.needs_write = false;
		const std::vector<pending_block> blocks = state.blocks;

		lock.unlock();
		write_disk(path, merged(path, blocks));
		lock.lock();
	}
}

auto gse::layout_store::store::next_dirty() const -> std::filesystem::path {
	for (const auto& [path, state] : m_files) {
		if (state.needs_write) {
			return path;
		}
	}
	return {};
}

auto gse::layout_store::merged(const std::filesystem::path& path, const std::span<const pending_block> blocks) -> std::string {
	std::string content = read_disk(path);
	for (const auto& [sections, block] : blocks) {
		apply(content, sections, block);
	}
	return content;
}

auto gse::layout_store::instance() -> store& {
	static store s;
	return s;
}

auto gse::layout_store::trimmed(const std::string_view s) -> std::string_view {
	std::size_t start = 0;
	while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n')) {
		++start;
	}
	std::size_t end = s.size();
	while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n')) {
		--end;
	}
	return s.substr(start, end - start);
}

auto gse::layout_store::section_name(const std::string_view line) -> std::string_view {
	const std::string_view t = trimmed(line);
	if (t.size() < 2 || t.front() != '[' || t.back() != ']') {
		return {};
	}
	return t.substr(1, t.size() - 2);
}

auto gse::layout_store::parse_sections(const std::string_view text) -> std::vector<section> {
	std::vector<section> sections;
	section* current = nullptr;

	std::size_t position = 0;
	while (const std::optional<std::string_view> raw = next_line(text, position)) {
		const std::string_view line = trimmed(*raw);
		if (line.empty() || line.front() == '#') {
			continue;
		}

		if (const std::string_view name = section_name(line); !name.empty()) {
			sections.push_back({ .name = std::string(name) });
			current = &sections.back();
			continue;
		}

		if (!current) {
			continue;
		}

		const std::size_t separator = line.find('=');
		if (separator == std::string_view::npos) {
			continue;
		}

		current->values[std::string(trimmed(line.substr(0, separator)))] = std::string(trimmed(line.substr(separator + 1)));
	}

	return sections;
}

auto gse::layout_store::owned(const owner& sections, const std::string_view name) -> bool {
	if (std::ranges::find(sections.names, name) != sections.names.end()) {
		return true;
	}
	for (const std::string& prefix : sections.prefixes) {
		if (name.starts_with(prefix)) {
			return true;
		}
	}
	return false;
}

auto gse::layout_store::apply(std::string& content, const owner& sections, const std::string_view block) -> void {
	std::string kept;
	std::string section;
	bool keep = true;

	std::size_t pos = 0;
	while (pos < content.size()) {
		const std::size_t line_end = content.find('\n', pos);
		const std::size_t next = line_end == std::string::npos ? content.size() : line_end + 1;
		const std::string_view line(content.data() + pos, next - pos);
		const std::string_view name = section_name(line);

		if (!name.empty()) {
			if (keep) {
				kept.append(section);
			}
			section.clear();
			keep = !owned(sections, name);
		}

		section.append(line);
		pos = next;
	}
	if (keep) {
		kept.append(section);
	}

	if (!block.empty()) {
		while (!kept.empty() && (kept.back() == '\n' || kept.back() == '\r')) {
			kept.pop_back();
		}
		if (!kept.empty()) {
			kept.append("\n\n");
		}
		kept.append(block);
		if (kept.back() != '\n') {
			kept.push_back('\n');
		}
	}

	content = std::move(kept);
}

auto gse::layout_store::read_disk(const std::filesystem::path& path) -> std::string {
	std::error_code ec;
	if (!std::filesystem::exists(path, ec)) {
		return {};
	}

	std::ifstream file(path);
	if (!file) {
		return {};
	}

	std::ostringstream oss;
	oss << file.rdbuf();
	return oss.str();
}

auto gse::layout_store::write_disk(const std::filesystem::path& path, const std::string_view content) -> void {
	std::error_code ec;
	if (const auto parent = path.parent_path(); !parent.empty() && !std::filesystem::exists(parent, ec)) {
		std::filesystem::create_directories(parent, ec);
	}

	std::filesystem::path temp = path;
	temp += std::format(".{}.tmp", win32::GetCurrentProcessId());
	const auto _ = make_scope_exit([&temp] {
		std::error_code remove_ec;
		std::filesystem::remove(temp, remove_ec);
	});

	{
		std::ofstream file(temp, std::ios::trunc);
		if (!file) {
			log::println(log::level::error, "layout_store: failed to open {} for write", temp.generic_display_string());
			return;
		}
		file << content;
	}

	constexpr int rename_attempts = 5;
	for (int attempt = 0; attempt < rename_attempts; ++attempt) {
		std::filesystem::rename(temp, path, ec);
		if (!ec) {
			return;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	log::println(log::level::error, "layout_store: failed to replace {} after {} attempts: {}", path.generic_display_string(), rename_attempts, ec.message());
}
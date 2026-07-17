export module gse.fs:layout_store;

import std;

import gse.core;
import gse.log;

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
}

namespace gse::layout_store {
	struct pending_block {
		owner sections;
		std::string block;
	};

	struct file_state {
		bool loaded = false;
		bool needs_write = false;
		std::string content;
		std::vector<pending_block> blocks;
	};

	class store : non_copyable, non_movable {
	public:
		store();

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
		auto run_worker() -> void;

		auto next_dirty() const -> std::filesystem::path;

		auto idle() const -> bool;

		auto load_entry(
			std::unique_lock<std::mutex>& lock,
			const std::filesystem::path& path
		) -> void;

		std::mutex m_mutex;
		std::condition_variable m_work;
		std::condition_variable m_idle;
		std::unordered_map<std::filesystem::path, file_state> m_files;
		bool m_stopping = false;
		bool m_writing = false;
		std::jthread m_worker;
	};

	auto instance() -> store&;

	auto trimmed(
		std::string_view s
	) -> std::string_view;

	auto section_name(
		std::string_view line
	) -> std::string_view;

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

gse::layout_store::store::store() : m_worker(&store::run_worker, this) {}

gse::layout_store::store::~store() {
	{
		std::lock_guard lock(m_mutex);
		m_stopping = true;
	}
	m_work.notify_all();
}

auto gse::layout_store::store::submit(const std::filesystem::path& path, owner sections, std::string block) -> void {
	{
		std::lock_guard lock(m_mutex);
		file_state& state = m_files[path];
		if (state.loaded) {
			apply(state.content, sections, block);
		}
		else {
			bool coalesced = false;
			for (pending_block& p : state.blocks) {
				if (p.sections == sections) {
					p.block = std::move(block);
					coalesced = true;
					break;
				}
			}
			if (!coalesced) {
				state.blocks.push_back({
					.sections = std::move(sections),
					.block = std::move(block),
				});
			}
		}
		state.needs_write = true;
	}
	m_work.notify_one();
}

auto gse::layout_store::store::read(const std::filesystem::path& path) -> std::string {
	std::unique_lock lock(m_mutex);
	m_files.try_emplace(path);
	load_entry(lock, path);
	return m_files.at(path).content;
}

auto gse::layout_store::store::flush() -> void {
	std::unique_lock lock(m_mutex);
	while (!idle()) {
		m_idle.wait(lock);
	}
}

auto gse::layout_store::store::run_worker() -> void {
	std::unique_lock lock(m_mutex);
	while (true) {
		std::filesystem::path path = next_dirty();
		while (path.empty() && !m_stopping) {
			m_work.wait(lock);
			path = next_dirty();
		}
		if (path.empty()) {
			return;
		}

		load_entry(lock, path);

		file_state& state = m_files.at(path);
		state.needs_write = false;
		const std::string snapshot = state.content;

		m_writing = true;
		lock.unlock();
		write_disk(path, snapshot);
		lock.lock();
		m_writing = false;

		if (idle()) {
			m_idle.notify_all();
		}
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

auto gse::layout_store::store::idle() const -> bool {
	if (m_writing) {
		return false;
	}
	for (const auto& state : std::views::values(m_files)) {
		if (state.needs_write) {
			return false;
		}
	}
	return true;
}

auto gse::layout_store::store::load_entry(std::unique_lock<std::mutex>& lock, const std::filesystem::path& path) -> void {
	if (m_files.at(path).loaded) {
		return;
	}

	lock.unlock();
	std::string disk = read_disk(path);
	lock.lock();

	file_state& state = m_files.at(path);
	if (state.loaded) {
		return;
	}

	for (const pending_block& p : state.blocks) {
		apply(disk, p.sections, p.block);
	}
	state.blocks.clear();
	state.content = std::move(disk);
	state.loaded = true;
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
		if (!kept.empty() && kept.back() != '\n') {
			kept.push_back('\n');
		}
		if (!kept.empty()) {
			kept.push_back('\n');
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
	temp += ".tmp";

	{
		std::ofstream file(temp, std::ios::trunc);
		if (!file) {
			log::println(log::level::error, "layout_store: failed to open {} for write", temp.display_string());
			return;
		}
		file << content;
	}

	std::filesystem::rename(temp, path, ec);
	if (ec) {
		log::println(log::level::error, "layout_store: failed to replace {}: {}", path.display_string(), ec.message());
	}
}

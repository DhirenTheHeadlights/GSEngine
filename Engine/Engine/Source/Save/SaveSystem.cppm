export module gse.save:save_system;

import std;

import gse.log;
import gse.math;
import gse.meta;
import gse.core;
import gse.concurrency;
import gse.ecs;
import gse.assert;

export namespace gse::save {
	class registry : public non_copyable {
	public:
		explicit registry(
			std::filesystem::path auto_save_path = {}
		);

		~registry();

		auto set_auto_save(
			bool enabled,
			std::filesystem::path path = {}
		) -> void;

		auto set_on_restart(
			std::function<void()> fn
		) -> void;

		auto add(
			settings::register_settings_type entry
		) -> void;

		auto for_each_entry(
			auto&& fn
		) const -> void;

		auto entry_count() const -> std::size_t;

		auto save_now() const -> bool;

		auto trigger_restart() const -> void;

		template <typename T>
		[[nodiscard]]
		static auto read_one(
			const std::filesystem::path& path,
			std::string_view category,
			std::string_view name,
			T fallback = T{}
		) -> T;

	private:
		using doc = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;

		static auto read_file(
			const std::filesystem::path& path
		) -> std::expected<std::string, std::error_code>;

		static auto trim(
			std::string_view s
		) -> std::string_view;

		static auto parse(
			std::string_view text
		) -> doc;

		static auto emit(
			const doc& d
		) -> std::string;

		auto load_from_file(
			const std::filesystem::path& path
		) -> bool;

		auto save_to_file(
			const std::filesystem::path& path
		) const -> bool;

		std::vector<settings::register_settings_type> m_entries;
		mutable std::mutex m_entries_mutex;
		std::filesystem::path m_auto_save_path;
		bool m_auto_save = false;
		std::function<void()> m_on_restart;
		doc m_loaded;
	};
}

auto gse::save::registry::for_each_entry(auto&& fn) const -> void {
	std::lock_guard lock(m_entries_mutex);
	for (const auto& entry : m_entries) {
		fn(entry);
	}
}

gse::save::registry::registry(std::filesystem::path auto_save_path) : m_auto_save_path(std::move(auto_save_path)) {
	if (!m_auto_save_path.empty()) {
		load_from_file(m_auto_save_path);
	}
}

gse::save::registry::~registry() {
	if (m_auto_save && !m_auto_save_path.empty()) {
		if (!save_to_file(m_auto_save_path)) {
			log::println(
				log::level::warning,
				log::category::save_system,
				"Failed to save settings to {}",
				m_auto_save_path.string()
			);
		}
	}
}

auto gse::save::registry::set_auto_save(const bool enabled, std::filesystem::path path) -> void {
	m_auto_save = enabled;
	if (!path.empty() && path != m_auto_save_path) {
		m_auto_save_path = std::move(path);
		load_from_file(m_auto_save_path);
	}
}

auto gse::save::registry::set_on_restart(std::function<void()> fn) -> void {
	m_on_restart = std::move(fn);
}

auto gse::save::registry::add(settings::register_settings_type entry) -> void {
	std::lock_guard lock(m_entries_mutex);

	if (entry.read && entry.settings_ptr) {
		entry.read(m_loaded, entry.category, entry.settings_ptr);
	}

	const auto match = std::ranges::find_if(
		m_entries,
		[&](const settings::register_settings_type& existing) {
			return existing.category == entry.category && existing.settings_ptr == entry.settings_ptr;
		}
	);

	if (match != m_entries.end()) {
		*match = std::move(entry);
		return;
	}

	for (const auto& existing : m_entries) {
		if (existing.category != entry.category) {
			continue;
		}
		for (const auto& key : entry.keys) {
			assert(
				std::ranges::find(existing.keys, key) == existing.keys.end(),
				"settings field name collision: category=\"{}\" key=\"{}\" is declared by both {} and {}",
				entry.category,
				key,
				existing.type_id,
				entry.type_id
			);
		}
	}

	m_entries.push_back(std::move(entry));
}

auto gse::save::registry::entry_count() const -> std::size_t {
	std::lock_guard lock(m_entries_mutex);
	return m_entries.size();
}

auto gse::save::registry::save_now() const -> bool {
	if (m_auto_save_path.empty()) {
		return false;
	}
	return save_to_file(m_auto_save_path);
}

auto gse::save::registry::trigger_restart() const -> void {
	save_now();
	if (m_on_restart) {
		m_on_restart();
	}
}

auto gse::save::registry::read_file(const std::filesystem::path& path) -> std::expected<std::string, std::error_code> {
	std::error_code ec;
	const auto size = std::filesystem::file_size(path, ec);
	if (ec) {
		return std::unexpected(ec);
	}

	std::ifstream file(path, std::ios::binary);
	if (!file) {
		return std::unexpected(std::make_error_code(std::errc::io_error));
	}

	std::string content(static_cast<std::size_t>(size), '\0');
	if (size > 0) {
		file.read(content.data(), static_cast<std::streamsize>(size));
		content.resize(static_cast<std::size_t>(file.gcount()));
	}
	return content;
}

auto gse::save::registry::trim(const std::string_view s) -> std::string_view {
	std::size_t start = 0;
	while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r')) {
		++start;
	}
	std::size_t end = s.size();
	while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r')) {
		--end;
	}
	return s.substr(start, end - start);
}

auto gse::save::registry::parse(const std::string_view text) -> doc {
	doc result;
	std::string current_section;

	std::size_t pos = 0;
	while (pos < text.size()) {
		const std::size_t line_end = text.find('\n', pos);
		const std::string_view line_raw =
			text.substr(
				pos,
				line_end == std::string_view::npos ? text.size() - pos : line_end - pos
			);
		pos = line_end == std::string_view::npos ? text.size() : line_end + 1;

		const auto line = trim(line_raw);
		if (line.empty() || line.front() == '#') {
			continue;
		}

		if (line.front() == '[' && line.back() == ']') {
			current_section = std::string(trim(line.substr(1, line.size() - 2)));
			result.try_emplace(current_section);
			continue;
		}

		const std::size_t eq = line.find('=');
		if (eq == std::string_view::npos) {
			continue;
		}

		const auto key = std::string(trim(line.substr(0, eq)));
		const auto value = std::string(trim(line.substr(eq + 1)));
		if (current_section.empty()) {
			continue;
		}
		result[current_section][key] = value;
	}

	return result;
}

auto gse::save::registry::emit(const doc& d) -> std::string {
	std::string out;
	bool first = true;
	for (const auto& [section, entries] : d) {
		if (!first) {
			out.push_back('\n');
		}
		first = false;
		out.push_back('[');
		out.append(section);
		out.append("]\n");
		for (const auto& [key, value] : entries) {
			out.append(key);
			out.append(" = ");
			out.append(value);
			out.push_back('\n');
		}
	}
	return out;
}

auto gse::save::registry::load_from_file(const std::filesystem::path& path) -> bool {
	if (!std::filesystem::exists(path)) {
		log::println(log::level::warning, log::category::save_system, "Settings file does not exist: {}",
					 path.string());
		return false;
	}

	const auto content = read_file(path);
	if (!content) {
		log::println(
			log::level::warning,
			log::category::save_system,
			"Failed to read {}: {}",
			path.string(),
			content.error().message()
		);
		return false;
	}

	m_loaded = parse(*content);

	std::lock_guard lock(m_entries_mutex);
	for (const auto& entry : m_entries) {
		if (entry.read && entry.settings_ptr) {
			entry.read(m_loaded, entry.category, entry.settings_ptr);
		}
	}
	return true;
}

auto gse::save::registry::save_to_file(const std::filesystem::path& path) const -> bool {
	doc d;
	{
		std::lock_guard lock(m_entries_mutex);
		for (const auto& entry : m_entries) {
			if (entry.write && entry.settings_ptr) {
				entry.write(d, entry.category, entry.settings_ptr);
			}
		}
	}

	std::ofstream file(path);
	if (!file) {
		return false;
	}
	file << emit(d);
	return true;
}

template <typename T>
auto gse::save::registry::read_one(const std::filesystem::path& path, const std::string_view category, const std::string_view name, T fallback) -> T {
	if (!std::filesystem::exists(path)) {
		return fallback;
	}
	const auto content = read_file(path);
	if (!content) {
		return fallback;
	}
	const auto d = parse(*content);
	const auto cat_it = d.find(std::string(category));
	if (cat_it == d.end()) {
		return fallback;
	}
	const auto val_it = cat_it->second.find(std::string(name));
	if (val_it == cat_it->second.end()) {
		return fallback;
	}
	T result = fallback;
	if (!gse::parse(val_it->second, result)) {
		return fallback;
	}
	return result;
}

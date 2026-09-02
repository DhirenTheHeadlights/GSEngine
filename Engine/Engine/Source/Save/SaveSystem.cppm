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
	enum class value_provenance : std::uint8_t {
		code_default,
		project,
		user,
		app_pin,
		session
	};

	struct registry_paths {
		std::filesystem::path user;
		std::filesystem::path project;
	};

	class registry : public non_copyable {
	public:
		~registry();

		auto set_paths(
			registry_paths paths
		) -> void;

		auto load() -> void;

		auto set_auto_save(
			bool enabled
		) -> void;

		auto set_on_restart(
			std::function<void()> fn
		) -> void;

		auto set_overrides(
			std::span<const std::string> assignments
		) -> void;

		template <typename State, fixed_string Key>
		auto pin(
			const auto& value
		) -> void;

		auto set_pin(
			std::string_view category,
			std::string_view key,
			std::string_view value
		) -> void;

		auto set_override(
			std::string_view category,
			std::string_view key,
			std::string_view value
		) -> void;

		auto clear_override(
			std::string_view category,
			std::string_view key
		) -> void;

		auto release_override(
			std::string_view category,
			std::string_view key
		) -> void;

		auto stage_value(
			std::string_view category,
			std::string_view key,
			std::string_view value
		) -> void;

		auto clear_staged(
			std::string_view category,
			std::string_view key
		) -> void;

		auto override_of(
			std::string_view category,
			std::string_view key
		) const -> std::optional<std::string>;

		auto provenance_of(
			std::string_view category,
			std::string_view key
		) const -> value_provenance;

		auto add(
			settings::register_settings_type entry
		) -> void;

		auto for_each_entry(
			auto&& fn
		) const -> void;

		auto audit_overrides() const -> void;

		auto audit_files() const -> void;

		auto dump() const -> std::string;

		auto entry_count() const -> std::size_t;

		auto user_path() const -> const std::filesystem::path&;

		auto project_path() const -> const std::filesystem::path&;

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
			const std::filesystem::path& path,
			settings::scope_kind scope
		) -> bool;

		auto save_to_file(
			const std::filesystem::path& path,
			settings::scope_kind scope
		) const -> bool;

		auto save_all() const -> bool;

		auto find_key(
			std::string_view category,
			std::string_view key
		) const -> const settings::settings_key_info*;

		auto category_registered(
			std::string_view category
		) const -> bool;

		std::vector<settings::register_settings_type> m_entries;
		registry_paths m_paths;
		bool m_auto_save = false;
		std::function<void()> m_on_restart;
		auto apply_one_key(
			std::string_view category,
			std::string_view key,
			const std::string& value
		) -> void;

		doc m_loaded;
		doc m_loaded_project;
		doc m_pins;
		doc m_overrides;
		doc m_staged;
		doc m_defaults;
	};
}

export namespace gse::save::override_system {
	[[= system_run<>{}]]
	auto run(
		channel_read<settings::override_request> requests_in,
		registry& save_reg
	) -> async::task<>;
}

auto gse::save::registry::for_each_entry(auto&& fn) const -> void {
	for (const auto& entry : m_entries) {
		fn(entry);
	}
}

template <typename State, gse::fixed_string Key>
auto gse::save::registry::pin(const auto& value) -> void {
	static_assert(
		!settings::category_of<State>().empty(),
		"pinned state has no settings::category annotation, so its keys are unreachable"
	);
	static_assert(
		settings::settings_key_exists<State>(std::string_view(Key)),
		"pinned key does not name an annotated setting on that state"
	);
	set_pin(settings::category_of<State>(), std::string_view(Key), meta::write_field(value));
}

gse::save::registry::~registry() {
	save_all();
}

auto gse::save::registry::set_paths(registry_paths paths) -> void {
	m_paths = std::move(paths);
}

auto gse::save::registry::load() -> void {
	if (!m_paths.project.empty()) {
		load_from_file(m_paths.project, settings::scope_kind::project);
	}
	if (!m_paths.user.empty()) {
		load_from_file(m_paths.user, settings::scope_kind::user);
	}
}

auto gse::save::registry::set_auto_save(const bool enabled) -> void {
	m_auto_save = enabled;
}

auto gse::save::registry::set_on_restart(std::function<void()> fn) -> void {
	m_on_restart = std::move(fn);
}

auto gse::save::registry::set_overrides(const std::span<const std::string> assignments) -> void {
	for (const std::string& assignment : assignments) {
		const std::string_view text = assignment;
		const std::size_t equals = text.find('=');
		const std::size_t dot = text.find('.');
		if (equals == std::string_view::npos || dot == std::string_view::npos || dot > equals) {
			log::println(
				log::level::warning,
				log::category::general,
				"ignoring setting override '{}'; expected Section.key=value",
				text
			);
			continue;
		}
		const std::string category(trim(text.substr(0, dot)));
		const std::string key(trim(text.substr(dot + 1, equals - dot - 1)));
		m_overrides[category][key] = std::string(trim(text.substr(equals + 1)));
	}
}

auto gse::save::registry::set_pin(const std::string_view category, const std::string_view key, const std::string_view value) -> void {
	auto& stored = m_pins[std::string(category)][std::string(key)];
	stored = std::string(value);
	apply_one_key(category, key, stored);
}

auto gse::save::registry::set_override(const std::string_view category, const std::string_view key, const std::string_view value) -> void {
	auto& stored = m_overrides[std::string(category)][std::string(key)];
	stored = std::string(value);
	apply_one_key(category, key, stored);
}

auto gse::save::registry::clear_override(const std::string_view category, const std::string_view key) -> void {
	const auto cat_it = m_overrides.find(std::string(category));
	if (cat_it == m_overrides.end() || cat_it->second.erase(std::string(key)) == 0) {
		return;
	}
	for (const doc* source : { &m_pins, &m_loaded, &m_loaded_project, &m_defaults }) {
		const auto sc = source->find(std::string(category));
		if (sc == source->end()) {
			continue;
		}
		const auto kv = sc->second.find(std::string(key));
		if (kv == sc->second.end()) {
			continue;
		}
		apply_one_key(category, key, kv->second);
		return;
	}
}

auto gse::save::registry::release_override(const std::string_view category, const std::string_view key) -> void {
	for (doc* layer : { &m_overrides, &m_pins }) {
		const auto cat_it = layer->find(std::string(category));
		if (cat_it != layer->end()) {
			cat_it->second.erase(std::string(key));
		}
	}
}

auto gse::save::registry::stage_value(const std::string_view category, const std::string_view key, const std::string_view value) -> void {
	m_staged[std::string(category)][std::string(key)] = std::string(value);
}

auto gse::save::registry::clear_staged(const std::string_view category, const std::string_view key) -> void {
	const auto cat_it = m_staged.find(std::string(category));
	if (cat_it != m_staged.end()) {
		cat_it->second.erase(std::string(key));
	}
}

auto gse::save::registry::override_of(const std::string_view category, const std::string_view key) const -> std::optional<std::string> {
	for (const doc* layer : { &m_overrides, &m_pins }) {
		const auto cat_it = layer->find(std::string(category));
		if (cat_it == layer->end()) {
			continue;
		}
		if (const auto kv = cat_it->second.find(std::string(key)); kv != cat_it->second.end()) {
			return kv->second;
		}
	}
	return std::nullopt;
}

auto gse::save::registry::provenance_of(const std::string_view category, const std::string_view key) const -> value_provenance {
	const auto holds = [&](const doc& d) {
		const auto cat_it = d.find(std::string(category));
		return cat_it != d.end() && cat_it->second.contains(std::string(key));
	};
	if (holds(m_overrides)) {
		return value_provenance::session;
	}
	if (holds(m_pins)) {
		return value_provenance::app_pin;
	}
	if (holds(m_loaded)) {
		return value_provenance::user;
	}
	if (holds(m_loaded_project)) {
		return value_provenance::project;
	}
	return value_provenance::code_default;
}

auto gse::save::registry::apply_one_key(const std::string_view category, const std::string_view key, const std::string& value) -> void {
	doc one;
	one[std::string(category)][std::string(key)] = value;
	for (auto& entry : m_entries) {
		if (entry.category != category || !entry.read || !entry.settings_ptr) {
			continue;
		}
		entry.read(one, entry.category, entry.settings_ptr, settings::scope_kind::user);
		entry.read(one, entry.category, entry.settings_ptr, settings::scope_kind::project);
		entry.read(one, entry.category, entry.settings_ptr, settings::scope_kind::app);
	}
}

auto gse::save::registry::add(settings::register_settings_type entry) -> void {
	for (const auto& field : entry.fields) {
		assert(
			std::ranges::contains(entry.keys, field.key, &settings::settings_key_info::key),
			"settings field '{}' in category '{}' has no matching serialized key",
			field.key,
			entry.category
		);
	}

	if (entry.write && entry.settings_ptr) {
		doc snapshot;
		entry.write(snapshot, entry.category, entry.settings_ptr, settings::scope_kind::user);
		entry.write(snapshot, entry.category, entry.settings_ptr, settings::scope_kind::project);
		entry.write(snapshot, entry.category, entry.settings_ptr, settings::scope_kind::app);
		for (auto& [cat, values] : snapshot) {
			for (auto& [key, value] : values) {
				m_defaults[cat].try_emplace(key, std::move(value));
			}
		}
	}

	if (entry.read && entry.settings_ptr) {
		entry.read(m_loaded, entry.category, entry.settings_ptr, settings::scope_kind::user);
		entry.read(m_loaded_project, entry.category, entry.settings_ptr, settings::scope_kind::project);
		for (const doc* layer : { &m_pins, &m_overrides }) {
			entry.read(*layer, entry.category, entry.settings_ptr, settings::scope_kind::user);
			entry.read(*layer, entry.category, entry.settings_ptr, settings::scope_kind::project);
			entry.read(*layer, entry.category, entry.settings_ptr, settings::scope_kind::app);
		}
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
		for (const auto& info : entry.keys) {
			assert(
				!std::ranges::contains(existing.keys, info.key, &settings::settings_key_info::key),
				"settings field name collision: category=\"{}\" key=\"{}\" is declared by both {} and {}",
				entry.category,
				info.key,
				existing.type_id,
				entry.type_id
			);
		}
	}

	m_entries.push_back(std::move(entry));
}

auto gse::save::registry::find_key(const std::string_view category, const std::string_view key) const -> const settings::settings_key_info* {
	for (const auto& entry : m_entries) {
		if (entry.category != category) {
			continue;
		}
		const auto it = std::ranges::find(entry.keys, key, &settings::settings_key_info::key);
		if (it != entry.keys.end()) {
			return &*it;
		}
	}
	return nullptr;
}

auto gse::save::registry::category_registered(const std::string_view category) const -> bool {
	return std::ranges::any_of(
		m_entries,
		[category](const settings::register_settings_type& entry) {
			return entry.category == category;
		}
	);
}

auto gse::save::registry::audit_files() const -> void {
	const auto audit_file = [this](const doc& d, const std::filesystem::path& path) {
		if (path.empty()) {
			return;
		}
		for (const auto& [category, values] : d) {
			if (!category_registered(category)) {
				continue;
			}
			for (const auto& key : std::views::keys(values)) {
				const settings::settings_key_info* info = find_key(category, key);
				if (!info) {
					log::println(
						log::level::warning,
						log::category::save_system,
						"{}: '{}.{}' does not name a setting and was ignored",
						path.generic_display_string(),
						category,
						key
					);
				}
				else if (info->scope == settings::scope_kind::app) {
					log::println(
						log::level::warning,
						log::category::save_system,
						"{}: '{}.{}' is fixed by the application and cannot be changed from a settings file",
						path.generic_display_string(),
						category,
						key
					);
				}
			}
		}
	};

	audit_file(m_loaded, m_paths.user);
	audit_file(m_loaded_project, m_paths.project);
}

auto gse::save::registry::audit_overrides() const -> void {
	std::vector<std::string> unknown;

	for (const auto& [category, values] : m_overrides) {
		if (!category_registered(category)) {
			for (const auto& key : std::views::keys(values)) {
				log::println(
					log::level::warning,
					log::category::save_system,
					"setting override '{}.{}' was not applied: no system registered section '{}' in this run mode",
					category,
					key,
					category
				);
			}
			continue;
		}
		for (const auto& key : std::views::keys(values)) {
			if (!find_key(category, key)) {
				unknown.push_back(std::format("{}.{}", category, key));
			}
		}
	}

	if (unknown.empty()) {
		return;
	}

	for (const auto& name : unknown) {
		std::cerr << std::format("error: setting override '{}' does not name a setting in that section\n", name);
	}
	std::cerr.flush();
	std::_Exit(2);
}

auto gse::save::registry::dump() const -> std::string {
	std::string out = std::format(
		"settings files:\n  user    = {}\n  project = {}\nlayers, lowest first: code_default < project < user < app_pin < session\n",
		m_paths.user.empty() ? std::string("<none>") : m_paths.user.generic_display_string(),
		m_paths.project.empty() ? std::string("<none>") : m_paths.project.generic_display_string()
	);

	for (const auto& entry : m_entries) {
		out += std::format("\n[{}]\n", entry.category);

		doc live;
		if (entry.write && entry.settings_ptr) {
			entry.write(live, entry.category, entry.settings_ptr, settings::scope_kind::user);
			entry.write(live, entry.category, entry.settings_ptr, settings::scope_kind::project);
			entry.write(live, entry.category, entry.settings_ptr, settings::scope_kind::app);
		}
		const auto values = live.find(entry.category);

		for (const auto& info : entry.keys) {
			std::string value = "<unreadable>";
			if (values != live.end()) {
				if (const auto kv = values->second.find(info.key); kv != values->second.end()) {
					value = kv->second;
				}
			}
			out += std::format(
				"  {} = {}  [{}{}]\n",
				info.key,
				value,
				enum_to_string(provenance_of(entry.category, info.key)),
				info.scope == settings::scope_kind::app ? ", app-fixed" : ""
			);
		}
	}
	return out;
}

auto gse::save::registry::entry_count() const -> std::size_t {
	return m_entries.size();
}

auto gse::save::registry::user_path() const -> const std::filesystem::path& {
	return m_paths.user;
}

auto gse::save::registry::project_path() const -> const std::filesystem::path& {
	return m_paths.project;
}

auto gse::save::registry::save_now() const -> bool {
	return save_all();
}

auto gse::save::registry::save_all() const -> bool {
	if (!m_auto_save) {
		return false;
	}

	bool ok = false;
	if (!m_paths.user.empty()) {
		ok = save_to_file(m_paths.user, settings::scope_kind::user);
		if (!ok) {
			log::println(log::level::warning, log::category::save_system, "Failed to save settings to {}", m_paths.user.generic_display_string());
		}
	}
	if (!m_paths.project.empty()) {
		if (!save_to_file(m_paths.project, settings::scope_kind::project)) {
			log::println(log::level::warning, log::category::save_system, "Failed to save project settings to {}", m_paths.project.generic_display_string());
			return false;
		}
		ok = true;
	}
	return ok;
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

auto gse::save::registry::load_from_file(const std::filesystem::path& path, const settings::scope_kind scope) -> bool {
	if (!std::filesystem::exists(path)) {
		log::println(log::level::warning, log::category::save_system, "Settings file does not exist: {}",
					 path.generic_display_string());
		return false;
	}

	const auto content = read_file(path);
	if (!content) {
		log::println(
			log::level::warning,
			log::category::save_system,
			"Failed to read {}: {}",
			path.generic_display_string(),
			content.error().message()
		);
		return false;
	}

	doc& target = scope == settings::scope_kind::project ? m_loaded_project : m_loaded;
	target = parse(*content);

	for (const auto& entry : m_entries) {
		if (entry.read && entry.settings_ptr) {
			entry.read(target, entry.category, entry.settings_ptr, scope);
		}
	}
	return true;
}

auto gse::save::registry::save_to_file(const std::filesystem::path& path, const settings::scope_kind scope) const -> bool {
	const doc& source = scope == settings::scope_kind::project ? m_loaded_project : m_loaded;
	doc d = source;
	for (const auto& entry : m_entries) {
		if (entry.write && entry.settings_ptr) {
			entry.write(d, entry.category, entry.settings_ptr, scope);
		}
	}
	for (const doc* layer : { &m_pins, &m_overrides }) {
		for (const auto& [category, keys] : *layer) {
			for (const auto& key : std::views::keys(keys)) {
				const auto restored = source.find(category);
				if (restored != source.end() && restored->second.contains(key)) {
					d[category][key] = restored->second.at(key);
				}
				else if (const auto written = d.find(category); written != d.end()) {
					written->second.erase(key);
				}
			}
		}
	}

	if (scope == settings::scope_kind::user) {
		for (const auto& [category, keys] : m_staged) {
			for (const auto& [key, value] : keys) {
				d[category][key] = value;
			}
		}
	}

	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);

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

auto gse::save::override_system::run(const channel_read<settings::override_request> requests_in, registry& save_reg) -> async::task<> {
	for (const auto& req : requests_in.of<settings::override_request>()) {
		switch (req.op) {
			case settings::override_op::release_override:
				save_reg.release_override(req.category, req.key);
				break;
			case settings::override_op::stage_value:
				save_reg.stage_value(req.category, req.key, req.value);
				break;
			case settings::override_op::clear_staged:
				save_reg.clear_staged(req.category, req.key);
				break;
		}
	}
	return {};
}

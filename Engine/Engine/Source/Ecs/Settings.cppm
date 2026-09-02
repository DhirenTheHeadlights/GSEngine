export module gse.ecs:settings;

import std;

import gse.core;
import gse.meta;
import gse.math;
import gse.log;

import :registries;

export namespace gse::settings {
	struct change_request {
		id state_type;
		std::function<void(void*)> apply;
	};

	enum class override_op : std::uint8_t {
		release_override,
		stage_value,
		clear_staged
	};

	struct override_request {
		override_op op = override_op::release_override;
		std::string category;
		std::string key;
		std::string value;
	};

	using change_request_writer = channel_write<change_request, override_request>;

	using draw_page_thunk = void (
			*
	)(
		void* builder,
		void* panel_state,
		change_request_writer channels,
		const void* entry
	);

	using write_settings_thunk = void (
			*
	)(
		std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc,
		std::string_view category,
		const void* settings_ptr,
		scope_kind filter
	);

	using read_settings_thunk = void (
			*
	)(
		const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc,
		std::string_view category,
		void* settings_ptr,
		scope_kind filter
	);

	using reset_to_defaults_thunk = void (
			*
	)(
		change_request_writer channels
	);

	using format_settings_field_thunk = std::string (
			*
	)(
		const void* settings_ptr
	);

	struct settings_field_option {
		std::string value;
		std::string label;
	};

	using settings_field_options_thunk = std::vector<settings_field_option> (
			*
	)(
		const void* settings_ptr
	);

	using push_settings_field_change_thunk = bool (
			*
	)(
		change_request_writer channels,
		std::string_view key,
		std::string_view value
	);

	using convert_settings_field_unit_thunk = std::string (
			*
	)(
		std::string_view canonical,
		std::string_view unit
	);

	using normalize_settings_field_thunk = std::string (
			*
	)(
		std::string_view text
	);

	enum class settings_field_widget : std::uint8_t {
		unsupported,
		boolean,
		choice,
		enumeration,
		integer,
		floating,
		dimensioned,
		text,
	};

	struct settings_field_range {
		bool enabled = false;
		double min = 0.0;
		double max = 0.0;
	};

	struct settings_field {
		std::string key;
		std::string description;
		settings_field_widget widget = settings_field_widget::unsupported;
		settings_field_range range;
		std::vector<settings_field_option> options;
		std::span<const std::string_view> units;
		std::string_view default_unit;
		format_settings_field_thunk format = nullptr;
		settings_field_options_thunk runtime_options = nullptr;
		push_settings_field_change_thunk push_change = nullptr;
		convert_settings_field_unit_thunk convert_unit = nullptr;
		normalize_settings_field_thunk normalize = nullptr;
		bool hot_reloadable = false;
		bool restart_required = false;
	};

	struct settings_key_info {
		std::string key;
		scope_kind scope = scope_kind::user;
	};

	struct register_settings_type {
		std::string category;
		id type_id;
		void* settings_ptr = nullptr;
		std::vector<settings_key_info> keys;
		std::vector<settings_field> fields;
		write_settings_thunk write = nullptr;
		read_settings_thunk read = nullptr;
		reset_to_defaults_thunk reset_to_defaults = nullptr;
		draw_page_thunk draw_page = nullptr;
	};

	template <typename T>
	concept has_parser_specialization = requires(std::string_view raw, T v) {
		{ parser<T>::parse(raw, v) } -> std::same_as<bool>; };

	template <typename T>
	concept is_scalar_settings_field = std::is_arithmetic_v<T> || std::is_enum_v<T> || std::same_as<T, bool> ||
		std::same_as<T, std::string> || has_parser_specialization<T> || meta::is_optional_field<T> ||
		meta::is_list_field<T>;

	template <typename T, scope_kind Inherited = scope_kind::user>
	auto write_settings_with_prefix(
		std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc,
		std::string_view category,
		std::string_view prefix,
		const T& value,
		scope_kind filter
	) -> void;

	template <typename T, scope_kind Inherited = scope_kind::user>
	auto read_settings_with_prefix(
		const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc,
		std::string_view category,
		std::string_view prefix,
		T& value,
		scope_kind filter
	) -> void;

	template <typename T>
	auto write_settings_for(
		std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc,
		std::string_view category,
		const void* settings_ptr,
		scope_kind filter
	) -> void;

	template <typename T>
	auto read_settings_for(
		const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc,
		std::string_view category,
		void* settings_ptr,
		scope_kind filter
	) -> void;

	template <std::meta::info M, scope_kind Fallback>
	consteval auto field_scope_of() -> scope_kind;

	template <typename T, scope_kind Inherited>
	auto collect_settings_keys_with_prefix(
		std::vector<settings_key_info>& out,
		std::string_view prefix
	) -> void;

	template <typename T>
	auto collect_settings_keys() -> std::vector<settings_key_info>;

	template <typename T>
	consteval auto settings_key_exists(
		std::string_view key
	) -> bool;

	template <typename T>
	consteval auto category_of() -> std::string_view;

	template <typename T>
	consteval auto scope_of() -> scope_kind;

	template <typename F>
	consteval auto field_widget_of() -> settings_field_widget;

	consteval auto make_range_field_from_info(
		std::meta::info range_type
	) -> settings_field_range;

	template <typename F>
	consteval auto make_quantity_range_field(
		std::meta::info range_type
	) -> settings_field_range;

	template <typename F>
	constexpr bool is_dimensioned_field = internal::is_quantity<F>;

	template <typename F>
	auto field_unit_names() -> std::span<const std::string_view>;

	template <typename F>
	consteval auto field_default_unit() -> std::string_view;

	template <typename F>
	auto convert_field_unit(
		std::string_view canonical,
		std::string_view unit
	) -> std::string;

	template <typename F>
	auto normalize_field_value(
		std::string_view text
	) -> std::string;

	template <typename F>
	auto warn_unparsed_field(
		std::string_view category,
		std::string_view key,
		std::string_view text
	) -> void;

	template <typename F>
	auto warn_ordinal_field(
		std::string_view category,
		std::string_view key,
		std::string_view text,
		const F& parsed
	) -> void;

	auto pretty_label(
		std::string_view raw
	) -> std::string;
}

auto gse::settings::pretty_label(const std::string_view raw) -> std::string {
	std::string result;
	result.reserve(raw.size());
	bool capitalize_next = true;
	for (const char c : raw) {
		if (c == '_') {
			result.push_back(' ');
			capitalize_next = true;
		}
		else if (capitalize_next) {
			result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
			capitalize_next = false;
		}
		else {
			result.push_back(c);
		}
	}
	return result;
}

template <typename F>
auto gse::settings::warn_ordinal_field(const std::string_view category, const std::string_view key, const std::string_view text, const F& parsed) -> void {
	if constexpr (std::is_enum_v<F>) {
		F by_name{};
		if (enum_from_string(text, by_name)) {
			return;
		}
		log::println(
			log::level::warning,
			log::category::general,
			"setting '{}.{}' still uses the numeric form '{}'; it now reads and writes the name '{}'",
			category,
			key,
			text,
			enum_to_string(parsed)
		);
	}
}

template <typename F>
auto gse::settings::warn_unparsed_field(const std::string_view category, const std::string_view key, const std::string_view text) -> void {
	if constexpr (is_dimensioned_field<F>) {
		std::string units;
		for (const auto name : field_unit_names<F>()) {
			if (!units.empty()) {
				units += ", ";
			}
			units += name;
		}
		log::println(
			log::level::warning,
			log::category::general,
			"setting '{}.{}' ignored: could not parse '{}'. Dimensioned values require a unit suffix, one of: {}",
			category,
			key,
			text,
			units
		);
	}
	else {
		log::println(
			log::level::warning,
			log::category::general,
			"setting '{}.{}' ignored: could not parse '{}'",
			category,
			key,
			text
		);
	}
}

template <typename T, gse::settings::scope_kind Inherited>
auto gse::settings::write_settings_with_prefix(std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc, const std::string_view category, const std::string_view prefix, const T& value, const scope_kind filter) -> void {
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		if constexpr (meta::find_describe(m) != std::meta::info{}) {
			using F = [:std::meta::type_of(m):];
			constexpr std::string_view name = meta::member_name(m);
			constexpr scope_kind effective = field_scope_of<m, Inherited>();
			const std::string key = prefix.empty() ? std::string(name) : std::format("{}.{}", prefix, name);

			if constexpr (std::is_class_v<F> && !is_scalar_settings_field<F>) {
				write_settings_with_prefix<F, effective>(doc, category, key, value.[:m:], filter);
			}
			else if (effective == filter) {
				doc[std::string(category)][key] = meta::write_field(value.[:m:]);
			}
		}
	}
}

template <std::meta::info M, gse::settings::scope_kind Fallback>
consteval auto gse::settings::field_scope_of() -> scope_kind {
	constexpr auto found = meta::find_scope(M);
	if constexpr (found != std::meta::info{}) {
		using S = [:found:];
		return S::value;
	}
	else {
		return Fallback;
	}
}

template <typename T, gse::settings::scope_kind Inherited>
auto gse::settings::read_settings_with_prefix(const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc, const std::string_view category, const std::string_view prefix, T& value, const scope_kind filter) -> void {
	const auto cat_it = doc.find(std::string(category));
	if (cat_it == doc.end()) {
		return;
	}
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		if constexpr (meta::find_describe(m) != std::meta::info{}) {
			using F = [:std::meta::type_of(m):];
			constexpr std::string_view name = meta::member_name(m);
			constexpr scope_kind effective = field_scope_of<m, Inherited>();
			const std::string key = prefix.empty() ? std::string(name) : std::format("{}.{}", prefix, name);

			if constexpr (std::is_class_v<F> && !is_scalar_settings_field<F>) {
				read_settings_with_prefix<F, effective>(doc, category, key, value.[:m:], filter);
			}
			else if (effective == filter) {
				if (const auto it = cat_it->second.find(key); it != cat_it->second.end()) {
					if (meta::read_field(it->second, value.[:m:])) {
						warn_ordinal_field<F>(category, key, it->second, value.[:m:]);
					}
					else {
						warn_unparsed_field<F>(category, key, it->second);
					}
				}
			}
		}
	}
}

template <typename T>
auto gse::settings::write_settings_for(std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc, const std::string_view category, const void* settings_ptr, const scope_kind filter) -> void {
	write_settings_with_prefix<T, scope_of<T>()>(
		doc,
		category,
		{},
		*static_cast<const T*>(settings_ptr),
		filter
	);
}

template <typename T>
auto gse::settings::read_settings_for(const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc, const std::string_view category, void* settings_ptr, const scope_kind filter) -> void {
	read_settings_with_prefix<T, scope_of<T>()>(
		doc,
		category,
		{},
		*static_cast<T*>(settings_ptr),
		filter
	);
}

template <typename T, gse::settings::scope_kind Inherited>
auto gse::settings::collect_settings_keys_with_prefix(std::vector<settings_key_info>& out, const std::string_view prefix) -> void {
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		if constexpr (meta::find_describe(m) != std::meta::info{}) {
			using F = [:std::meta::type_of(m):];
			constexpr std::string_view name = meta::member_name(m);
			constexpr scope_kind effective = field_scope_of<m, Inherited>();
			std::string key = prefix.empty() ? std::string(name) : std::format("{}.{}", prefix, name);

			if constexpr (std::is_class_v<F> && !is_scalar_settings_field<F>) {
				collect_settings_keys_with_prefix<F, effective>(out, key);
			}
			else {
				out.push_back({ .key = std::move(key), .scope = effective });
			}
		}
	}
}

template <typename T>
auto gse::settings::collect_settings_keys() -> std::vector<settings_key_info> {
	std::vector<settings_key_info> out;
	collect_settings_keys_with_prefix<T, scope_of<T>()>(
		out,
		{}
	);
	return out;
}

template <typename T>
consteval auto gse::settings::settings_key_exists(const std::string_view key) -> bool {
	bool found = false;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		if constexpr (meta::find_describe(m) != std::meta::info{}) {
			using F = [:std::meta::type_of(m):];
			constexpr std::string_view name = meta::member_name(m);

			if constexpr (std::is_class_v<F> && !is_scalar_settings_field<F>) {
				if (key.size() > name.size() && key.starts_with(name) && key[name.size()] == '.' &&
					settings_key_exists<F>(key.substr(name.size() + 1))) {
					found = true;
				}
			}
			else if (key == name) {
				found = true;
			}
		}
	}
	return found;
}

template <typename T>
consteval auto gse::settings::category_of() -> std::string_view {
	constexpr auto cat = meta::find_category(^^T);
	if constexpr (cat != std::meta::info{}) {
		using C = [:cat:];
		return C::value;
	}
	else {
		return std::string_view{};
	}
}

template <typename T>
consteval auto gse::settings::scope_of() -> scope_kind {
	constexpr auto found = meta::find_scope(^^T);
	if constexpr (found != std::meta::info{}) {
		using S = [:found:];
		return S::value;
	}
	else {
		return scope_kind::user;
	}
}

template <typename F>
consteval auto gse::settings::field_widget_of() -> settings_field_widget {
	if constexpr (std::same_as<F, bool>) {
		return settings_field_widget::boolean;
	}
	else if constexpr (is_choice_v<F>) {
		return settings_field_widget::choice;
	}
	else if constexpr (std::is_enum_v<F>) {
		return settings_field_widget::enumeration;
	}
	else if constexpr (std::is_integral_v<F>) {
		return settings_field_widget::integer;
	}
	else if constexpr (std::is_floating_point_v<F>) {
		return settings_field_widget::floating;
	}
	else if constexpr (is_dimensioned_field<F>) {
		return settings_field_widget::dimensioned;
	}
	else if constexpr (std::same_as<F, std::string> || has_parser_specialization<F>) {
		return settings_field_widget::text;
	}
	else {
		return settings_field_widget::unsupported;
	}
}

template <typename F>
auto gse::settings::field_unit_names() -> std::span<const std::string_view> {
	if constexpr (is_dimensioned_field<F>) {
		return internal::unit_names<typename F::quantity_tag>();
	}
	else {
		return {};
	}
}

template <typename F>
consteval auto gse::settings::field_default_unit() -> std::string_view {
	if constexpr (is_dimensioned_field<F>) {
		return std::string_view(F::default_unit::unit_name);
	}
	else {
		return {};
	}
}

template <typename F>
auto gse::settings::convert_field_unit(const std::string_view canonical, const std::string_view unit) -> std::string {
	if constexpr (is_dimensioned_field<F>) {
		F parsed{};
		if (!parse(canonical, parsed)) {
			return {};
		}
		return std::format("{::{}!}", parsed, unit);
	}
	else {
		return {};
	}
}

template <typename F>
auto gse::settings::normalize_field_value(const std::string_view text) -> std::string {
	F parsed{};
	if (!parse(text, parsed)) {
		return {};
	}
	return std::format("{}", parsed);
}

consteval auto gse::settings::make_range_field_from_info(const std::meta::info range_type) -> settings_field_range {
	const auto targs = std::meta::template_arguments_of(range_type);
	if (targs.size() < 2) {
		return {};
	}
	const auto value_type = std::meta::dealias(std::meta::type_of(targs[0]));
	if (value_type == ^^int) {
		return {
			.enabled = true,
			.min = static_cast<double>(std::meta::extract<int>(targs[0])),
			.max = static_cast<double>(std::meta::extract<int>(targs[1])),
		};
	}
	if (value_type == ^^float) {
		return {
			.enabled = true,
			.min = static_cast<double>(std::meta::extract<float>(targs[0])),
			.max = static_cast<double>(std::meta::extract<float>(targs[1])),
		};
	}
	if (value_type == ^^double) {
		return {
			.enabled = true,
			.min = std::meta::extract<double>(targs[0]),
			.max = std::meta::extract<double>(targs[1]),
		};
	}
	return {};
}

template <typename F>
consteval auto gse::settings::make_quantity_range_field(const std::meta::info range_type) -> settings_field_range {
	const auto targs = std::meta::template_arguments_of(range_type);
	if (targs.size() < 2) {
		return {};
	}
	if (std::meta::dealias(std::meta::type_of(targs[0])) != ^^F) {
		return {};
	}
	return {
		.enabled = true,
		.min = static_cast<double>(static_cast<typename F::value_type>(std::meta::extract<F>(targs[0]))),
		.max = static_cast<double>(static_cast<typename F::value_type>(std::meta::extract<F>(targs[1]))),
	};
}

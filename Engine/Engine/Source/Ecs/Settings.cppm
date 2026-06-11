export module gse.ecs:settings;

import std;

import gse.core;
import gse.meta;

import :registries;

export namespace gse::settings {
	template <typename S>
	struct change_request {
		std::function<void(typename S::data&)> apply;
	};

	template <typename S>
	struct changed {
		typename S::data old_value;
		typename S::data new_value;
	};

	using write_settings_thunk = void (
			*
	)(
		std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc,
		std::string_view category,
		const void* settings_ptr
	);

	using read_settings_thunk = void (
			*
	)(
		const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc,
		std::string_view category,
		void* settings_ptr
	);

	using reset_to_defaults_thunk = void (
			*
	)(
		void* channel_writer
	);

	using format_settings_field_thunk = std::string (
			*
	)(
		const void* settings_ptr
	);

	using settings_field_options_thunk = std::vector<std::string> (
			*
	)(
		const void* settings_ptr
	);

	using push_settings_field_change_thunk = bool (
			*
	)(
		void* channel_writer,
		std::string_view value
	);

	enum class settings_field_widget : std::uint8_t {
		unsupported,
		boolean,
		choice,
		enumeration,
		integer,
		floating,
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
		std::vector<std::string> options;
		format_settings_field_thunk format = nullptr;
		settings_field_options_thunk runtime_options = nullptr;
		push_settings_field_change_thunk push_change = nullptr;
		bool hot_reloadable = false;
		bool restart_required = false;
	};

	struct register_settings_type {
		std::string category;
		id type_id;
		void* settings_ptr = nullptr;
		std::vector<std::string> keys;
		std::vector<settings_field> fields;
		write_settings_thunk write = nullptr;
		read_settings_thunk read = nullptr;
		reset_to_defaults_thunk reset_to_defaults = nullptr;
	};

	template <typename T>
	concept has_parser_specialization = requires(std::string_view raw, T v) {
		{ parser<T>::parse(raw, v) } -> std::same_as<bool>; };

	template <typename T>
	concept is_scalar_settings_field = std::is_arithmetic_v<T> || std::is_enum_v<T> || std::same_as<T, bool> ||
		std::same_as<T, std::string> || has_parser_specialization<T>;

	template <typename T>
	auto write_settings_with_prefix(
		std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc,
		std::string_view category,
		std::string_view prefix,
		const T& value
	) -> void;

	template <typename T>
	auto read_settings_with_prefix(
		const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc,
		std::string_view category,
		std::string_view prefix,
		T& value
	) -> void;

	template <typename T>
	auto write_settings_for(
		std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc,
		std::string_view category,
		const void* settings_ptr
	) -> void;

	template <typename T>
	auto read_settings_for(
		const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc,
		std::string_view category,
		void* settings_ptr
	) -> void;

	template <typename T>
	auto collect_settings_keys_with_prefix(
		std::vector<std::string>& out,
		std::string_view prefix
	) -> void;

	template <typename T>
	auto collect_settings_keys() -> std::vector<std::string>;

	template <typename T>
	consteval auto category_of() -> std::string_view;

	template <typename F>
	consteval auto field_widget_of() -> settings_field_widget;

	template <typename S, std::meta::info M>
	auto format_settings_field(
		const void* settings_ptr
	) -> std::string;

	template <typename S, std::meta::info M>
	auto settings_field_options(
		const void* settings_ptr
	) -> std::vector<std::string>;

	template <typename S, std::meta::info M>
	auto push_settings_field_change(
		void* channel_writer_ptr,
		std::string_view raw
	) -> bool;

	template <typename S, std::meta::info M>
	auto make_settings_field() -> settings_field;

	template <typename S>
	auto collect_settings_fields() -> std::vector<settings_field>;

	template <typename S>
	auto reset_to_defaults_for(
		void* channel_writer_ptr
	) -> void;

	template <typename S>
	auto build_settings_record(
		typename S::data& obj
	) -> register_settings_type;
}

template <typename T>
auto gse::settings::write_settings_with_prefix(std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc, const std::string_view category, const std::string_view prefix, const T& value) -> void {
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		if constexpr (meta::find_describe(m) != std::meta::info{}) {
			using F = [:std::meta::type_of(m):];
			constexpr std::string_view name = meta::member_name(m);
			const std::string key = prefix.empty() ? std::string(name) : std::format("{}.{}", prefix, name);

			if constexpr (std::is_class_v<F> && !is_scalar_settings_field<F>) {
				write_settings_with_prefix<F>(doc, category, key, value.[:m:]);
			}
			else {
				std::string formatted;
				std::format_to(std::back_inserter(formatted), "{}", value.[:m:]);
				doc[std::string(category)][key] = std::move(formatted);
			}
		}
	}
}

template <typename T>
auto gse::settings::read_settings_with_prefix(const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc, const std::string_view category, const std::string_view prefix, T& value) -> void {
	const auto cat_it = doc.find(std::string(category));
	if (cat_it == doc.end()) {
		return;
	}
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		if constexpr (meta::find_describe(m) != std::meta::info{}) {
			using F = [:std::meta::type_of(m):];
			constexpr std::string_view name = meta::member_name(m);
			const std::string key = prefix.empty() ? std::string(name) : std::format("{}.{}", prefix, name);

			if constexpr (std::is_class_v<F> && !is_scalar_settings_field<F>) {
				read_settings_with_prefix<F>(doc, category, key, value.[:m:]);
			}
			else if (const auto it = cat_it->second.find(key); it != cat_it->second.end()) {
				gse::parse(it->second, value.[:m:]);
			}
		}
	}
}

template <typename T>
auto gse::settings::write_settings_for(std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc, const std::string_view category, const void* settings_ptr) -> void {
	write_settings_with_prefix<T>(
		doc,
		category,
		{},
		*static_cast<const T*>(settings_ptr)
	);
}

template <typename T>
auto gse::settings::read_settings_for(const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& doc, const std::string_view category, void* settings_ptr) -> void {
	read_settings_with_prefix<T>(
		doc,
		category,
		{},
		*static_cast<T*>(settings_ptr)
	);
}

template <typename T>
auto gse::settings::collect_settings_keys_with_prefix(std::vector<std::string>& out, const std::string_view prefix) -> void {
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		if constexpr (meta::find_describe(m) != std::meta::info{}) {
			using F = [:std::meta::type_of(m):];
			constexpr std::string_view name = meta::member_name(m);
			std::string key = prefix.empty() ? std::string(name) : std::format("{}.{}", prefix, name);

			if constexpr (std::is_class_v<F> && !is_scalar_settings_field<F>) {
				collect_settings_keys_with_prefix<F>(out, key);
			}
			else {
				out.push_back(std::move(key));
			}
		}
	}
}

template <typename T>
auto gse::settings::collect_settings_keys() -> std::vector<std::string> {
	std::vector<std::string> out;
	collect_settings_keys_with_prefix<T>(
		out,
		{}
	);
	return out;
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
	else if constexpr (std::same_as<F, std::string> || has_parser_specialization<F>) {
		return settings_field_widget::text;
	}
	else {
		return settings_field_widget::unsupported;
	}
}

template <typename S, std::meta::info M>
auto gse::settings::format_settings_field(const void* settings_ptr) -> std::string {
	using data_t = typename S::data;
	using F = [:std::meta::type_of(M):];
	const auto& field = static_cast<const data_t*>(settings_ptr)->[:M:];
	if constexpr (is_choice_v<F>) {
		return std::format("{}", field.value);
	}
	else {
		return std::format("{}", field);
	}
}

template <typename S, std::meta::info M>
auto gse::settings::settings_field_options(const void* settings_ptr) -> std::vector<std::string> {
	using data_t = typename S::data;
	using F = [:std::meta::type_of(M):];
	if constexpr (is_choice_v<F>) {
		return static_cast<const data_t*>(settings_ptr)->[:M:].options;
	}
	else {
		return {};
	}
}

template <typename S, std::meta::info M>
auto gse::settings::push_settings_field_change(void* channel_writer_ptr, const std::string_view raw) -> bool {
	using data_t = typename S::data;
	using F = [:std::meta::type_of(M):];
	auto& channels = *static_cast<channel_writer*>(channel_writer_ptr);
	if constexpr (is_choice_v<F>) {
		typename F::value_type parsed{};
		if (!gse::parse(raw, parsed)) {
			return false;
		}
		channels.push<change_request<S>>({
			.apply = [parsed](data_t& d) {
				d.[:M:].value = parsed;
			},
		});
		return true;
	}
	else {
		F parsed{};
		if (!gse::parse(raw, parsed)) {
			return false;
		}
		channels.push<change_request<S>>({
			.apply = [parsed = std::move(parsed)](data_t& d) {
				d.[:M:] = parsed;
			},
		});
		return true;
	}
}

template <typename S, std::meta::info M>
auto gse::settings::make_settings_field() -> settings_field {
	using F = [:std::meta::type_of(M):];
	using describe_t = [:meta::find_describe(M):];
	settings_field field{
		.key = std::string(meta::member_name(M)),
		.description = std::string(describe_t::value),
		.widget = field_widget_of<F>(),
		.format = &format_settings_field<S, M>,
		.push_change = &push_settings_field_change<S, M>,
		.hot_reloadable = has_annotation<settings::hot_reloadable_tag>(M),
		.restart_required = has_annotation<settings::restart_required>(M),
	};
	if constexpr (is_choice_v<F>) {
		field.runtime_options = &settings_field_options<S, M>;
	}
	if constexpr (std::is_enum_v<F>) {
		template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^F))) {
			field.options.emplace_back(std::meta::identifier_of(e));
		}
	}
	if constexpr (constexpr auto range_t = meta::find_range(M); range_t != std::meta::info{}) {
		using R = [:range_t:];
		if constexpr (std::is_arithmetic_v<F>) {
			field.range = {
				.enabled = true,
				.min = static_cast<double>(R::min),
				.max = static_cast<double>(R::max),
			};
		}
	}
	return field;
}

template <typename S>
auto gse::settings::collect_settings_fields() -> std::vector<settings_field> {
	using data_t = typename S::data;
	std::vector<settings_field> out;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^data_t, std::meta::access_context::unchecked()))) {
		if constexpr (meta::find_describe(m) != std::meta::info{}) {
			using F = [:std::meta::type_of(m):];
			if constexpr (!(std::is_class_v<F> && !is_scalar_settings_field<F>) && field_widget_of<F>() != settings_field_widget::unsupported) {
				out.push_back(make_settings_field<S, m>());
			}
		}
	}
	return out;
}

template <typename S>
auto gse::settings::reset_to_defaults_for(void* channel_writer_ptr) -> void {
	using data_t = typename S::data;
	auto& channels = *static_cast<channel_writer*>(channel_writer_ptr);
	channels.push<change_request<S>>({
		.apply = [](data_t& d) {
			data_t defaults{};
			template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^data_t, std::meta::access_context::unchecked()))) {
				if constexpr (meta::find_describe(m) != std::meta::info{}) {
					using F = [:std::meta::type_of(m):];
					if constexpr (is_choice_v<F>) {
						d.[:m:].value = defaults.[:m:].value;
					}
					else {
						d.[:m:] = defaults.[:m:];
					}
				}
			}
		},
	});
}

template <typename S>
auto gse::settings::build_settings_record(typename S::data& obj) -> register_settings_type {
	using data_t = typename S::data;
	return {
		.category = std::string(category_of<data_t>()),
		.type_id = id_of<data_t>(),
		.settings_ptr = &obj,
		.keys = collect_settings_keys<data_t>(),
		.fields = collect_settings_fields<S>(),
		.write = &write_settings_for<data_t>,
		.read = &read_settings_for<data_t>,
		.reset_to_defaults = &reset_to_defaults_for<S>,
	};
}

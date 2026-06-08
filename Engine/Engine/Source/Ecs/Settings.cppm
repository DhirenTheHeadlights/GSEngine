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

	using draw_settings_thunk = void (
			*
	)(
		void* gui_builder,
		void* panel_state,
		std::string_view category,
		void* settings_ptr,
		void* channel_writer
	);

	using draw_page_thunk = void (
			*
	)(
		void* gui_builder,
		void* panel_state,
		void* settings_ptr,
		void* channel_writer
	);

	using draw_hot_fields_thunk = void (
			*
	)(
		void* gui_builder,
		void* panel_state,
		void* settings_ptr,
		void* channel_writer
	);

	using reset_to_defaults_thunk = void (
			*
	)(
		void* channel_writer
	);

	struct register_settings_type {
		std::string category;
		id type_id;
		void* settings_ptr = nullptr;
		std::vector<std::string> keys;
		write_settings_thunk write = nullptr;
		read_settings_thunk read = nullptr;
		draw_settings_thunk draw = nullptr;
		draw_page_thunk draw_page = nullptr;
		draw_hot_fields_thunk draw_hot_fields = nullptr;
		reset_to_defaults_thunk reset_to_defaults = nullptr;
		bool has_hot_fields = false;
	};

	template <typename S>
	struct gui_draw_provider {
		static constexpr draw_settings_thunk value = nullptr;
		static constexpr draw_page_thunk page_value = nullptr;
		static constexpr draw_hot_fields_thunk hot_value = nullptr;
		static constexpr bool any_hot = false;
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
		.write = &write_settings_for<data_t>,
		.read = &read_settings_for<data_t>,
		.draw = gui_draw_provider<S>::value,
		.draw_page = gui_draw_provider<S>::page_value,
		.draw_hot_fields = gui_draw_provider<S>::hot_value,
		.reset_to_defaults = &reset_to_defaults_for<S>,
		.has_hot_fields = gui_draw_provider<S>::any_hot,
	};
}

export module gse.ecs:system_anno;

import std;

import gse.meta;

export namespace gse {
	enum class system_hook : std::uint8_t {
		init,
		run,
		frame,
		shutdown,
	};

	template <fixed_string Name>
	struct system_state {
		static constexpr std::string_view value = Name;
	};

	struct system_init {};

	template <int Order = 0>
	struct system_run {
		static constexpr int order = Order;
	};

	template <std::meta::info State>
	struct runs_after {
		static constexpr std::meta::info state_type = State;
	};

	template <std::meta::info State>
	struct runs_after_optional {
		static constexpr std::meta::info state_type = State;
	};

	struct system_frame {};

	struct system_shutdown {};

	struct deferred_system {};
}

export namespace gse::settings {
	template <std::meta::info State>
	struct page_for {
		static constexpr std::meta::info state_type = State;
	};
}

export namespace gse::meta {
	consteval auto find_system_state_anno(
		std::meta::info type
	) -> std::meta::info;

	consteval auto find_system_hook_anno(
		std::meta::info fn
	) -> std::meta::info;

	consteval auto find_page_for_anno(
		std::meta::info fn
	) -> std::meta::info;

	consteval auto is_deferred_system(
		std::meta::info type
	) -> bool;

	consteval auto hook_kind_of(
		std::meta::info hook_anno_type
	) -> system_hook;

	consteval auto state_type_of_page_for(
		std::meta::info page_anno_type
	) -> std::meta::info;

	consteval auto run_order_of(
		std::meta::info hook_anno_type
	) -> int;

	consteval auto order_deps_of(
		std::meta::info fn,
		std::meta::info anno_template
	) -> std::vector<std::meta::info>;

	consteval auto required_order_deps_of(
		std::meta::info fn
	) -> std::vector<std::meta::info>;

	consteval auto optional_order_deps_of(
		std::meta::info fn
	) -> std::vector<std::meta::info>;

	template <typename State>
	consteval auto has_system_state() -> bool;

	template <typename State>
	consteval auto system_state_name() -> std::string_view;

	template <typename State>
	consteval auto system_qualified_name() -> std::string_view;
}

consteval auto gse::meta::find_system_hook_anno(const std::meta::info fn) -> std::meta::info {
	for (const auto ann : std::meta::annotations_of(fn)) {
		const auto t = std::meta::remove_cvref(std::meta::dealias(std::meta::type_of(ann)));
		if (t == ^^system_init || t == ^^system_frame || t == ^^system_shutdown) {
			return t;
		}
		if (std::meta::has_template_arguments(t) && std::meta::template_of(t) == ^^system_run) {
			return t;
		}
	}
	return std::meta::info{};
}

consteval auto gse::meta::find_system_state_anno(const std::meta::info type) -> std::meta::info {
	return find_class_template_annotation(type, ^^system_state);
}

consteval auto gse::meta::is_deferred_system(const std::meta::info type) -> bool {
	return gse::find_annotation<deferred_system>(type) != std::meta::info{};
}

consteval auto gse::meta::find_page_for_anno(const std::meta::info fn) -> std::meta::info {
	return find_class_template_annotation(fn, ^^settings::page_for);
}

consteval auto gse::meta::hook_kind_of(const std::meta::info hook_anno_type) -> system_hook {
	if (hook_anno_type == ^^system_init) {
		return system_hook::init;
	}
	if (hook_anno_type == ^^system_frame) {
		return system_hook::frame;
	}
	if (hook_anno_type == ^^system_shutdown) {
		return system_hook::shutdown;
	}
	return system_hook::run;
}

consteval auto gse::meta::state_type_of_page_for(const std::meta::info page_anno_type) -> std::meta::info {
	return std::meta::extract<std::meta::info>(std::meta::template_arguments_of(page_anno_type)[0]);
}

consteval auto gse::meta::run_order_of(const std::meta::info hook_anno_type) -> int {
	if (std::meta::has_template_arguments(hook_anno_type) && std::meta::template_of(hook_anno_type) == ^^system_run) {
		return std::meta::extract<int>(std::meta::template_arguments_of(hook_anno_type)[0]);
	}
	return 0;
}

consteval auto gse::meta::order_deps_of(const std::meta::info fn, const std::meta::info anno_template) -> std::vector<std::meta::info> {
	std::vector<std::meta::info> out;
	for (const auto ann : std::meta::annotations_of(fn)) {
		const auto t = std::meta::remove_cvref(std::meta::dealias(std::meta::type_of(ann)));
		if (!std::meta::has_template_arguments(t) || std::meta::template_of(t) != anno_template) {
			continue;
		}
		out.push_back(std::meta::extract<std::meta::info>(std::meta::template_arguments_of(t)[0]));
	}
	return out;
}

consteval auto gse::meta::required_order_deps_of(const std::meta::info fn) -> std::vector<std::meta::info> {
	return order_deps_of(fn, ^^runs_after);
}

consteval auto gse::meta::optional_order_deps_of(const std::meta::info fn) -> std::vector<std::meta::info> {
	return order_deps_of(fn, ^^runs_after_optional);
}

template <typename State>
consteval auto gse::meta::has_system_state() -> bool {
	return find_system_state_anno(^^State) != std::meta::info{};
}

template <typename State>
consteval auto gse::meta::system_state_name() -> std::string_view {
	constexpr auto anno = find_system_state_anno(^^State);
	if constexpr (anno != std::meta::info{}) {
		using anno_t = [:anno:];
		return anno_t::value;
	}
	else {
		return std::string_view{};
	}
}

template <typename State>
consteval auto gse::meta::system_qualified_name() -> std::string_view {
	const auto entity = std::meta::dealias(^^State);
	std::string self = std::meta::has_identifier(entity)
		? std::string(std::meta::identifier_of(entity))
		: std::string("data");
	std::string prefix;
	auto parent = std::meta::parent_of(entity);
	while (std::meta::has_identifier(parent)) {
		const std::string_view ident = std::meta::identifier_of(parent);
		if (ident == "std" || ident.starts_with("__")) {
			break;
		}
		prefix = std::string(ident) + "::" + prefix;
		parent = std::meta::parent_of(parent);
	}
	return std::define_static_string(prefix + self);
}

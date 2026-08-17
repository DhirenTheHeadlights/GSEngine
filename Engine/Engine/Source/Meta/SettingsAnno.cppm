export module gse.meta:settings_anno;

import std;

import :annotations;
import :fixed_string;
import :parse;

export namespace gse::settings {
	template <fixed_string V>
	struct describe {
		static constexpr std::string_view value = V;
	};

	template <fixed_string V>
	struct category {
		static constexpr std::string_view value = V;
	};

	enum class scope_kind : std::uint8_t {
		user,
		project,
		app
	};

	template <scope_kind V>
	struct scope {
		static constexpr scope_kind value = V;
	};

	using user_scope = scope<scope_kind::user>;
	using project_scope = scope<scope_kind::project>;
	using app_scope = scope<scope_kind::app>;

	struct restart_required {};
	struct skip {};
	struct hot_reloadable_tag {};
	constexpr hot_reloadable_tag hot_reloadable{};

	template <auto Min, auto Max>
	struct range {
		static constexpr auto min = Min;
		static constexpr auto max = Max;
	};

	template <typename T>
	struct choice {
		using value_type = T;
		T value{};
		std::vector<std::string> options;
	};

	template <typename T>
	struct is_choice : std::false_type {};

	template <typename T>
	struct is_choice<choice<T>> : std::true_type {};

	template <typename T>
	constexpr bool is_choice_v = is_choice<T>::value;

	template <typename T>
	auto choice_index(
		const choice<T>& c
	) -> std::size_t;

	template <typename T>
	auto set_choice_index(
		choice<T>& c,
		std::size_t index
	) -> void;
}

template <typename T>
auto gse::settings::choice_index(const choice<T>& c) -> std::size_t {
	if constexpr (std::same_as<T, std::string>) {
		const auto it = std::ranges::find(c.options, c.value);
		return it == c.options.end() ? 0 : static_cast<std::size_t>(std::ranges::distance(c.options.begin(), it));
	}
	else {
		return static_cast<std::size_t>(c.value);
	}
}

template <typename T>
auto gse::settings::set_choice_index(choice<T>& c, const std::size_t index) -> void {
	if constexpr (std::same_as<T, std::string>) {
		c.value = index < c.options.size() ? c.options[index] : std::string{};
	}
	else {
		c.value = static_cast<T>(index);
	}
}

export template <typename T>
struct std::formatter<gse::settings::choice<T>> : std::formatter<T> {
	auto format(const gse::settings::choice<T>& c, auto& ctx) const {
		return std::formatter<T>::format(c.value, ctx);
	}
};

export template <typename T>
struct gse::parser<gse::settings::choice<T>> {
	static auto parse(std::string_view raw, gse::settings::choice<T>& out) -> bool {
		return gse::parse(raw, out.value);
	}
};

export namespace gse {
	template <typename Anno>
	consteval auto find_annotation(
		std::meta::info member
	) -> std::meta::info;

	template <typename Anno, std::meta::info M>
	consteval auto annotation_of() -> Anno;
}

export namespace gse::meta {
	consteval auto member_name(
		std::meta::info m
	) -> std::string_view;

	consteval auto find_class_template_annotation(
		std::meta::info m,
		std::meta::info template_reflection
	) -> std::meta::info;

	consteval auto find_range(
		std::meta::info m
	) -> std::meta::info;

	consteval auto find_describe(
		std::meta::info m
	) -> std::meta::info;

	consteval auto find_category(
		std::meta::info m
	) -> std::meta::info;

	consteval auto find_scope(
		std::meta::info m
	) -> std::meta::info;
}

template <typename Anno>
consteval auto gse::find_annotation(const std::meta::info member) -> std::meta::info {
	constexpr auto target = std::meta::remove_cvref(std::meta::dealias(^^Anno));
	for (auto ann : std::meta::annotations_of(member)) {
		const auto t = std::meta::remove_cvref(std::meta::dealias(std::meta::type_of(ann)));
		if (std::meta::is_same_type(t, target)) {
			return ann;
		}
	}
	return std::meta::info{};
}

template <typename Anno, std::meta::info M>
consteval auto gse::annotation_of() -> Anno {
	constexpr auto ann = find_annotation<Anno>(M);
	return [:std::meta::constant_of(ann):];
}

consteval auto gse::meta::member_name(const std::meta::info m) -> std::string_view {
	const std::string_view ident = std::meta::identifier_of(m);
	if (ident.size() > 2 && ident[0] == 'm' && ident[1] == '_') {
		return ident.substr(2);
	}
	return ident;
}

consteval auto gse::meta::find_class_template_annotation(const std::meta::info m, const std::meta::info template_reflection) -> std::meta::info {
	const auto deal_m = std::meta::dealias(m);
	for (auto ann : std::meta::annotations_of(deal_m)) {
		const auto t = std::meta::dealias(std::meta::type_of(ann));
		if (std::meta::has_template_arguments(t) && std::meta::template_of(t) == template_reflection) {
			return t;
		}
	}
	return std::meta::info{};
}

consteval auto gse::meta::find_range(const std::meta::info m) -> std::meta::info {
	return find_class_template_annotation(m, ^^settings::range);
}

consteval auto gse::meta::find_describe(const std::meta::info m) -> std::meta::info {
	return find_class_template_annotation(m, ^^settings::describe);
}

consteval auto gse::meta::find_category(const std::meta::info m) -> std::meta::info {
	return find_class_template_annotation(m, ^^settings::category);
}

consteval auto gse::meta::find_scope(const std::meta::info m) -> std::meta::info {
	return find_class_template_annotation(m, ^^settings::scope);
}

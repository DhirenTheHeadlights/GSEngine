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

	struct option_label {
		char text[48];
	};

	struct choice {
		std::string value;
		std::vector<std::string> options;
	};

	template <typename T>
	constexpr bool is_choice_v = std::same_as<T, choice>;

	auto choice_index(
		const choice& c
	) -> std::size_t;

	auto set_choice_index(
		choice& c,
		std::size_t index
	) -> void;
}

auto gse::settings::choice_index(const choice& c) -> std::size_t {
	const auto it = std::ranges::find(c.options, c.value);
	return it == c.options.end() ? 0 : static_cast<std::size_t>(std::ranges::distance(c.options.begin(), it));
}

auto gse::settings::set_choice_index(choice& c, const std::size_t index) -> void {
	if (index < c.options.size()) {
		c.value = c.options[index];
	}
}

template <>
struct std::formatter<gse::settings::choice> : formatter<std::string> {
	auto format(const gse::settings::choice& c, auto& ctx) const {
		return formatter<std::string>::format(c.value, ctx);
	}
};

template <>
struct gse::parser<gse::settings::choice> {
	static auto parse(const std::string_view raw, settings::choice& out) -> bool {
		out.value = std::string(raw);
		return true;
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
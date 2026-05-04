export module gse.meta:settings_anno;

import std;
import gse.std_meta;

import :annotations;
import :parse;

export namespace gse::settings {
    struct describe {};
    struct restart_required {};
    struct skip {};

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
    consteval auto annotation_of(
    ) -> Anno;
}

export namespace gse::meta {
    consteval auto member_name(
        std::meta::info m
    ) -> std::string_view;

    consteval auto find_range(
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
    return [: std::meta::constant_of(ann) :];
}

consteval auto gse::meta::member_name(const std::meta::info m) -> std::string_view {
    const std::string_view ident = std::meta::identifier_of(m);
    if (ident.size() > 2 && ident[0] == 'm' && ident[1] == '_') {
        return ident.substr(2);
    }
    return ident;
}

consteval auto gse::meta::find_range(const std::meta::info m) -> std::meta::info {
    constexpr auto target = ^^settings::range;
    for (auto ann : std::meta::annotations_of(m)) {
        const auto t = std::meta::dealias(std::meta::type_of(ann));
        if (std::meta::has_template_arguments(t) && std::meta::template_of(t) == target) {
            return t;
        }
    }
    return std::meta::info{};
}

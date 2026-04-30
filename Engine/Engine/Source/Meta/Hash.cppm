export module gse.meta:hash;

import std;
import gse.std_meta;

export namespace gse {
    template <typename T>
    concept reflectable = requires { typename T::gse_reflectable_t; };

    template <typename T>
    auto hash_combine(
        const T& value
    ) -> std::size_t;
}

template <typename T>
auto gse::hash_combine(const T& value) -> std::size_t {
    std::size_t h = 0;
    template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
        using member_type = std::remove_cvref_t<decltype(value.[:m:])>;
        std::size_t sub;
        if constexpr (std::is_class_v<member_type>) {
            sub = hash_combine(value.[:m:]);
        }
        else {
            sub = std::hash<member_type>{}(value.[:m:]);
        }
        h ^= sub + 0x9e3779b9u + (h << 6) + (h >> 2);
    }
    return h;
}

export template <typename T>
    requires (gse::reflectable<T>)
struct std::hash<T> {
    auto operator()(const T& value) const noexcept -> std::size_t {
        return gse::hash_combine(value);
    }
};

export template <typename T>
    requires (gse::reflectable<T>)
struct std::formatter<T> {
    constexpr auto parse(auto& ctx) {
        return ctx.begin();
    }

    auto format(const T& value, auto& ctx) const {
        auto out = std::format_to(ctx.out(), "{{");
        bool first = true;
        template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
            if (!first) {
                out = std::format_to(out, ", ");
            }
            first = false;
            out = std::format_to(out, "{}={}", std::meta::identifier_of(m), value.[:m:]);
        }
        return std::format_to(out, "}}");
    }
};

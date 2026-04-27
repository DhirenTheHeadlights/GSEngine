export module gse.meta:hash;

import std;

export namespace gse {
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

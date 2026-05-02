export module gse.meta:enums;

import std;
import gse.std_meta;

export namespace gse {

    template <typename E>
        requires std::is_enum_v<E>
    constexpr auto enum_to_string(
        E value
    ) -> std::string_view;
}

template <typename E>
    requires std::is_enum_v<E>
constexpr auto gse::enum_to_string(const E value) -> std::string_view {
    template for (constexpr auto v : std::define_static_array(std::meta::enumerators_of(^^E))) {
        if ([:v:] == value) {
            return std::meta::identifier_of(v);
        }
    }
    return "<unknown>";
}

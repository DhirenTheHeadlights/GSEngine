export module gse.math:primitive_math_shared;

import std;

export namespace gse {
    template <typename T>
    concept is_vec2 = requires(T t) {
        { t.x() } -> std::convertible_to<typename T::value_type>;
        { t.y() } -> std::convertible_to<typename T::value_type>;
    };
}
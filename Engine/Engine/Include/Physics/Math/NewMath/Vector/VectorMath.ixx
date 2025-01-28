export module gse.physics.math.vec.vec_math;

import std;

import gse.physics.math.unit_vec;
import gse.physics.math.mat;

export namespace gse {
    template <typename T, int N>
    constexpr auto dot(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> T;

    template <typename T>
    constexpr auto cross(const vec3<T>& lhs, const vec3<T>& rhs) -> vec3<T>;

    template <typename T, int N>
    constexpr auto magnitude(const vec_t<T, N>& v) -> T;

    template <typename T, int N>
    constexpr auto normalize(const vec_t<T, N>& v) -> vec_t<unitless, N>;

    template <typename T, int N>
    constexpr auto distance(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> T;

    template <typename T, int N>
    constexpr auto project(const vec_t<T, N>& a, const vec_t<T, N>& b) -> vec_t<unitless, N>;

    template <typename T, int N>
    constexpr auto reflect(const vec_t<T, N>& v, const vec_t<T, N>& normal) -> vec_t<unitless, N>;

    template <typename T, int N>
    constexpr auto angle_between(const vec_t<T, N>& a, const vec_t<T, N>& b) -> T;

    template <typename T, int N>
    constexpr auto lerp(const vec_t<T, N>& a, const vec_t<T, N>& b, T t) -> vec_t<unitless, N>;

    template <typename T, int N>
    constexpr auto min(const vec_t<T, N>& a, const vec_t<T, N>& b) -> vec_t<T, N>;

    template <typename T, int N>
    constexpr auto max(const vec_t<T, N>& a, const vec_t<T, N>& b) -> vec_t<T, N>;

    template <typename T, int N>
    constexpr auto clamp(const vec_t<T, N>& v, const vec_t<T, N>& min_val, const vec_t<T, N>& max_val) -> vec_t<T, N>;

    template <typename T, int N>
    constexpr auto component_multiply(const vec_t<T, N>& a, const vec_t<T, N>& b) -> vec_t<T, N>;

    template <typename T, int N>
    constexpr auto component_divide(const vec_t<T, N>& a, const vec_t<T, N>& b) -> vec_t<T, N>;

    template <typename T>
    constexpr auto projection_matrix(const vec3<T>& normal) -> mat_t<typename vec3<T>::value_type, 3, 3>;

    template <typename T>
    constexpr auto reflection_matrix(const vec3<T>& normal) -> mat_t<typename vec3<T>::value_type, 3, 3>;

    template <typename T, typename U, int N>
    constexpr auto convert(const vec_t<T, N>& v) -> vec_t<U, N>;

    template <typename T, int N>
    constexpr auto less_than(const vec_t<T, N>& a, const vec_t<T, N>& b) -> std::array<bool, N>;

    template <typename T, int N>
    constexpr auto greater_than(const vec_t<T, N>& a, const vec_t<T, N>& b) -> std::array<bool, N>;

    template <typename T, int N>
    constexpr auto min(const vec_t<T, N>& v, T scalar) -> vec_t<T, N>;

    template <typename T, int N>
    constexpr auto max(const vec_t<T, N>& v, T scalar) -> vec_t<T, N>;

    template <typename T, int N, int... Indices>
    constexpr auto swizzle(const vec_t<T, N>& v, std::integer_sequence<int, Indices...>) -> vec_t<T, sizeof...(Indices)>;

	template <typename T, int N>
	constexpr auto is_zero(const vec_t<T, N>& v, typename vec_t<T, N>::value_type epsilon = std::numeric_limits<typename vec_t<T, N>::value_type>::epsilon()) -> bool;

    template <typename T, int N>
    constexpr auto random_vector(T min = T(0), T max = T(1)) -> vec_t<T, N>;

    template <typename T, int N>
    constexpr auto is_normalized(const vec_t<T, N>& v, T epsilon = std::numeric_limits<T>::epsilon()) -> bool;

    template <typename T>
    constexpr auto perpendicular(const vec2<T>& v) -> vec2<T>;

    template <typename T>
    constexpr auto barycentric(const vec3<T>& p, const vec3<T>& a, const vec3<T>& b, const vec3<T>& c) -> std::tuple<T, T, T>;

    template <typename T, int N, int M>
    constexpr auto multiply(const mat_t<T, M, N>& matrix, const vec_t<T, N>& vector) -> vec_t<T, M>;

    template <typename T, int N>
    constexpr auto exp(const vec_t<T, N>& v) -> vec_t<T, N>;

    template <typename T, int N>
    constexpr auto log(const vec_t<T, N>& v) -> vec_t<T, N>;

    template <typename T, int N>
    constexpr auto min_norm_vector(const vec_t<T, N>& a, const vec_t<T, N>& b) -> vec_t<T, N>;

    template <typename T, int N>
    constexpr auto max_norm_vector(const vec_t<T, N>& a, const vec_t<T, N>& b) -> vec_t<T, N>;

    template <typename T, int N>
    constexpr auto approx_equal(const vec_t<T, N>& a, const vec_t<T, N>& b, typename vec_t<T, N>::value_type epsilon = std::numeric_limits<T>::epsilon()) -> bool;

    template <typename T, int N>
    constexpr auto reflect_across(const vec_t<T, N>& a, const vec_t<T, N>& b) -> vec_t<unitless, N>;

    template <typename T, int N>
    constexpr auto hadamard_product(const vec_t<T, N>& a, const vec_t<T, N>& b) -> vec_t<T, N>;

    template <typename T, int N>
    constexpr auto sin(const vec_t<T, N>& v) -> vec_t<T, N>;

    template <typename T, int N>
    constexpr auto cos(const vec_t<T, N>& v) -> vec_t<T, N>;

    template <typename T, int N>
    constexpr auto ceil(const vec_t<T, N>& v) -> vec_t<T, N>;

    template <typename T, int N>
    constexpr auto floor(const vec_t<T, N>& v) -> vec_t<T, N>;

    template <typename T, int N>
    constexpr auto pow(const vec_t<T, N>& v, T exponent) -> vec_t<T, N>;

    template <typename T, int N>
    constexpr auto to_string(const vec_t<T, N>& v) -> std::string;

    template <typename T, int N>
    constexpr auto swizzle(const vec_t<T, N>& v) -> vec_t<T, N>;

    template <typename T, int N>
    constexpr auto serialize(const vec_t<T, N>& v, std::ostream& os) -> std::ostream&;

    template <typename T, int N>
    constexpr auto deserialize(vec_t<T, N>& v, std::istream& is) -> std::istream&;
}

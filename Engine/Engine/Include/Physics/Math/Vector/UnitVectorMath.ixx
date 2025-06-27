export module gse.physics.math:unit_vec_math;

import std;

import :unit_vec;
import :vec_math;
import :unitless_vec;
import :quant;

export namespace gse {
	template <internal::is_quantity T, int N> constexpr auto magnitude(const vec_t<T, N>& v) -> T;
	template <internal::is_quantity T, int N> constexpr auto is_zero(const vec_t<T, N>& v) -> bool;
	template <internal::is_quantity T, int N> constexpr auto normalize(const vec_t<T, N>& v) -> unitless::vec_t<typename T::value_type, N>;
	template <internal::is_quantity T, int N> constexpr auto dot(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> T;
	template <internal::is_quantity T, internal::is_quantity U, int N> constexpr auto cross(const vec_t<T, N>& lhs, const vec_t<U, N>& rhs);
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::magnitude(const vec_t<T, N>& v) -> T {
	return T(magnitude(v.template as<T::default_unit>()));
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::is_zero(const vec_t<T, N>& v) -> bool {
	return is_zero(v.template as<T::default_unit>());
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::normalize(const vec_t<T, N>& v) -> unitless::vec_t<typename T::value_type, N> {
	return normalize(v.template as<T::default_unit>());
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::dot(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> T {
	return T(dot(lhs.template as<T::default_unit>(), rhs.template as<T::default_unit>()));
}

template <gse::internal::is_quantity T, gse::internal::is_quantity U, int N>
constexpr auto gse::cross(const vec_t<T, N>& lhs, const vec_t<U, N>& rhs) {
	using result_q = decltype(T() * U());
	return vec_t<result_q, N>(cross(lhs.template as<typename T::default_unit>(), rhs.template as<typename U::default_unit>()));
}


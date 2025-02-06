export module gse.physics.math.unit_vec_math;

import std;

import gse.physics.math.unit_vec;
import gse.physics.math.vec_math;
import gse.physics.math.unitless_vec;
import gse.physics.math.units.quant;

export namespace gse {
	template <internal::is_quantity T, int N> constexpr auto magnitude(const vec_t<T, N>& v) -> T;
	template <internal::is_quantity T, int N> constexpr auto is_zero(const vec_t<T, N>& v) -> bool;
	template <internal::is_quantity T, int N> constexpr auto normalize(const vec_t<T, N>& v) -> unitless::vec_t<typename T::value_type, N>;
	template <internal::is_quantity T, int N> constexpr auto dot(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> T;
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::magnitude(const vec_t<T, N>& v) -> T {
	return T(magnitude(v.template as_test<T::default_unit>()));
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::is_zero(const vec_t<T, N>& v) -> bool {
	return is_zero(v.template as_test<T::default_unit>());
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::normalize(const vec_t<T, N>& v) -> unitless::vec_t<typename T::value_type, N> {
	return normalize(v.template as_test<T::default_unit>());
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::dot(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> T {
	return T(dot(lhs.template as_test<T::default_unit>(), rhs.template as_test<T::default_unit>()));
}
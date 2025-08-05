export module gse.physics.math:unit_vec_math;

import std;

import :unit_vec;
import :vec_math;
import :unitless_vec;
import :quant;

export namespace gse::unit {
	template <internal::is_quantity T, int N>
	constexpr auto operator+(
		const vec_t<T, N>& lhs,
		const vec_t<T, N>& rhs
		) -> vec_t<T, N>;

	template <internal::is_quantity T, int N>
	constexpr auto operator-(
		const vec_t<T, N>& lhs,
		const vec_t<T, N>& rhs
		) -> vec_t<T, N>;

	template <internal::is_quantity T, internal::is_arithmetic U, int N>
	constexpr auto operator*(
		const vec_t<T, N>& lhs,
		const U& rhs
		) -> vec_t<T, N>;

	template <internal::is_arithmetic U, internal::is_quantity T, int N>
	constexpr auto operator*(
		const U& lhs,
		const vec_t<T, N>& rhs
		) -> vec_t<T, N>;

	template <internal::is_quantity T, internal::is_arithmetic U, int N>
	constexpr auto operator/(
		const vec_t<T, N>& lhs,
		const U& rhs
		) -> vec_t<T, N>;

	template <internal::is_arithmetic U, internal::is_quantity T, int N>
	constexpr auto operator/(
		const U& lhs,
		const vec_t<T, N>& rhs
		) -> vec_t<T, N>;

	template <internal::is_quantity T, internal::is_quantity U, int N>
	constexpr auto operator*(
		const vec_t<T, N>& lhs,
		const vec_t<U, N>& rhs
		);

	template <internal::is_quantity T, internal::is_quantity U, int N>
	constexpr auto operator/(
		const vec_t<T, N>& lhs,
		const vec_t<U, N>& rhs
		);

	template <internal::is_quantity T, int N>
	constexpr auto operator/(
		const vec_t<T, N>& lhs,
		const vec_t<T, N>& rhs
		) -> unitless::vec_t<typename T::value_type, N>;

	template <internal::is_quantity T, internal::is_quantity U, int N>
	constexpr auto operator*(
		const vec_t<T, N>& lhs,
		const U& rhs
		);

	template <internal::is_quantity T, internal::is_quantity U, int N>
	constexpr auto operator/(
		const vec_t<T, N>& lhs,
		const U& rhs
		);

	template <internal::is_quantity T, internal::is_quantity U, int N>
	constexpr auto operator*(
		const U& lhs,
		const vec_t<T, N>& rhs
		);

	template <internal::is_quantity T, internal::is_quantity U, int N>
	constexpr auto operator/(
		const U& lhs,
		const vec_t<T, N>& rhs
		);

	template <internal::is_quantity T, int N>
	constexpr auto operator/(
		const vec_t<T, N>& lhs,
		const T& rhs
		) -> unitless::vec_t<typename T::value_type, N>;

	template <internal::is_quantity T, int N>
	constexpr auto operator/(
		const T& lhs,
		const vec_t<T, N>& rhs
		) -> unitless::vec_t<typename T::value_type, N>;

	template <internal::is_quantity T, int N>
	constexpr auto operator+=(
		vec_t<T, N>& lhs,
		const vec_t<T, N>& rhs
		) -> vec_t<T, N>&;

	template <internal::is_quantity T, int N>
	constexpr auto operator-=(
		vec_t<T, N>& lhs,
		const vec_t<T, N>& rhs
		) -> vec_t<T, N>&;

	template <internal::is_quantity T, internal::is_arithmetic U, int N>
	constexpr auto operator*=(
		vec_t<T, N>& lhs,
		const U& rhs
		) -> vec_t<T, N>&;

	template <internal::is_quantity T, internal::is_arithmetic U, int N>
	constexpr auto operator/=(
		vec_t<T, N>& lhs,
		const U& rhs
		) -> vec_t<T, N>&;

	template <internal::is_quantity T, internal::is_arithmetic U, int N>
	constexpr auto operator+(
		const vec_t<T, N>& lhs,
		const unitless::vec_t<U, N>& rhs
		) -> vec_t<T, N>;

	template <internal::is_arithmetic T, internal::is_quantity U, int N>
	constexpr auto operator+(
		const unitless::vec_t<T, N>& lhs,
		const vec_t<U, N>& rhs
		) -> vec_t<T, N>;

	template <internal::is_quantity T, internal::is_arithmetic U, int N>
	constexpr auto operator-(
		const vec_t<T, N>& lhs,
		const unitless::vec_t<U, N>& rhs
		) -> vec_t<T, N>;

	template <internal::is_arithmetic T, internal::is_quantity U, int N>
	constexpr auto operator-(
		const unitless::vec_t<T, N>& lhs,
		const vec_t<U, N>& rhs
		) -> vec_t<T, N>;

	template <internal::is_quantity T, internal::is_arithmetic U, int N>
	constexpr auto operator*(
		const vec_t<T, N>& lhs,
		const unitless::vec_t<U, N>& rhs
		) -> vec_t<T, N>;

	template <internal::is_arithmetic T, internal::is_quantity U, int N>
	constexpr auto operator*(
		const unitless::vec_t<T, N>& lhs,
		const vec_t<U, N>& rhs
		) -> vec_t<T, N>;

	template <internal::is_quantity T, internal::is_arithmetic U, int N>
	constexpr auto operator/(
		const vec_t<T, N>& lhs,
		const unitless::vec_t<U, N>& rhs
		) -> vec_t<T, N>;

	template <internal::is_arithmetic T, internal::is_quantity U, int N>
	constexpr auto operator/(
		const unitless::vec_t<T, N>& lhs,
		const vec_t<U, N>& rhs
		) -> vec_t<T, N>;

	template <internal::is_arithmetic T, int N, internal::is_quantity U>
	constexpr auto operator*(
		const vec_t<T, N>& lhs,
		const U& rhs
		) -> vec_t<U, N>;

	template <internal::is_arithmetic T, int N, internal::is_quantity U>
	constexpr auto operator*(
		const U& lhs,
		const vec_t<T, N>& rhs
		) -> vec_t<U, N>;

	template <internal::is_arithmetic T, int N, internal::is_quantity U>
	constexpr auto operator/(
		const vec_t<T, N>& lhs,
		const U& rhs
		) -> vec_t<U, N>;

	template <internal::is_arithmetic T, int N, internal::is_quantity U>
	constexpr auto operator/(
		const U& lhs,
		const vec_t<T, N>& rhs
		) -> vec_t<U, N>;

	template <internal::is_quantity T, int N>
	constexpr auto operator-(
		const vec_t<T, N>& value
		) -> vec_t<T, N>;
}

export namespace gse {
	template <typename T, int N>
	constexpr auto magnitude(
		const vec_t<T, N>& v
	) -> T;

	template <typename T, int N>
	constexpr auto is_zero(
		const vec_t<T, N>& v
	) -> bool;

	template <typename T, int N>
	constexpr auto normalize(
		const vec_t<T, N>& v
	) -> unitless::vec_t<typename T::value_type, N>;

	template <typename T, int N>
	constexpr auto dot(
		const vec_t<T, N>& lhs, 
		const vec_t<T, N>& rhs
	);

	template <typename T, int N>
	constexpr auto dot(
		const vec_t<T, N>& lhs, 
		const unitless::vec_t<typename T::value_type, N>& rhs
	) -> T;

	template <typename T, int N>
	constexpr auto dot(
		const unitless::vec_t<typename T::value_type, N>& lhs, 
		const vec_t<T, N>& rhs
	) -> T;

	template <typename T, typename U, int N>
	constexpr auto cross(
		const vec_t<T, N>& lhs,
		const vec_t<U, N>& rhs
	);
}

export namespace gse::unitless {
	template <typename T, int N, internal::is_quantity U>
	constexpr auto operator*(
		const vec_t<T, N>& lhs,
		const U& rhs
	) -> unit::vec_t<U, N>;

	template <typename T, int N, internal::is_quantity U>
	constexpr auto operator*(
		const U& lhs, 
		const vec_t<T, N>& rhs
	) -> unit::vec_t<U, N>;

	template <typename T, int N, internal::is_quantity U>
	constexpr auto operator/(
		const vec_t<T, N>& lhs, 
		const U& rhs
	) -> unit::vec_t<U, N>;

	template <typename T, int N, internal::is_quantity U>
	constexpr auto operator/(
		const U& lhs,
		const vec_t<T, N>& rhs
	) -> unit::vec_t<U, N>;
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::unit::operator+(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>().storage + rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::unit::operator-(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>().storage - rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_quantity T, gse::internal::is_arithmetic U, int N>
constexpr auto gse::unit::operator*(const vec_t<T, N>& lhs, const U& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>().storage * rhs);
}

template <gse::internal::is_arithmetic U, gse::internal::is_quantity T, int N>
constexpr auto gse::unit::operator*(const U& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs * rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_quantity T, gse::internal::is_arithmetic U, int N>
constexpr auto gse::unit::operator/(const vec_t<T, N>& lhs, const U& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>().storage / rhs);
}

template <gse::internal::is_arithmetic U, gse::internal::is_quantity T, int N>
constexpr auto gse::unit::operator/(const U& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs / rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_quantity T, gse::internal::is_quantity U, int N>
constexpr auto gse::unit::operator*(const vec_t<T, N>& lhs, const vec_t<U, N>& rhs) {
	using result_q = decltype(T()* U());
	return unit::vec_t<result_q, N>(lhs.template as<typename T::default_unit>().storage * rhs.template as<typename U::default_unit>().storage);
}

template <gse::internal::is_quantity T, gse::internal::is_quantity U, int N>
constexpr auto gse::unit::operator/(const vec_t<T, N>& lhs, const vec_t<U, N>& rhs) {
	using result_q = decltype(T() / U());
	return unit::vec_t<result_q, N>(lhs.template as<typename T::default_unit>().storage / rhs.template as<typename U::default_unit>().storage);
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::unit::operator/(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> unitless::vec_t<typename T::value_type, N> {
	return unitless::vec_t<typename T::value_type, N>(lhs.template as<typename T::default_unit>().storage / rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_quantity T, gse::internal::is_quantity U, int N>
constexpr auto gse::unit::operator*(const vec_t<T, N>& lhs, const U& rhs) {
	using result_q = decltype(T()* U());
	return unit::vec_t<result_q, N>(lhs.template as<typename T::default_unit>().storage * rhs.template as<typename U::default_unit>());
}

template <gse::internal::is_quantity T, gse::internal::is_quantity U, int N>
constexpr auto gse::unit::operator/(const vec_t<T, N>& lhs, const U& rhs) {
	using result_q = decltype(T() / U());
	return unit::vec_t<result_q, N>(lhs.template as<typename T::default_unit>().storage / rhs.template as<typename U::default_unit>());
}

template <gse::internal::is_quantity T, gse::internal::is_quantity U, int N>
constexpr auto gse::unit::operator*(const U& lhs, const vec_t<T, N>& rhs) {
	using result_q = decltype(U()* T());
	return unit::vec_t<result_q, N>(lhs.template as<typename U::default_unit>() * rhs.template as<typename T::default_unit>());
}

template <gse::internal::is_quantity T, gse::internal::is_quantity U, int N>
constexpr auto gse::unit::operator/(const U& lhs, const vec_t<T, N>& rhs) {
	using result_q = decltype(U() / T());
	return unit::vec_t<result_q, N>(lhs.template as<typename U::default_unit>() / rhs.template as<typename T::default_unit>());
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::unit::operator/(const vec_t<T, N>& lhs, const T& rhs) -> unitless::vec_t<typename T::value_type, N> {
	return unitless::vec_t<typename T::value_type, N>(lhs.template as<typename T::default_unit>().storage / rhs);
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::unit::operator/(const T& lhs, const vec_t<T, N>& rhs) -> unitless::vec_t<typename T::value_type, N> {
	return unitless::vec_t<typename T::value_type, N>(lhs / rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::unit::operator+=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>& {
	auto v1 = lhs.template as<typename T::default_unit>();
	auto v2 = rhs.template as<typename T::default_unit>();

	v1.storage += v2.storage;
	lhs = unit::vec_t<T, N>(v1);
	return lhs;
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::unit::operator-=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>& {
	auto v1 = lhs.template as<typename T::default_unit>();
	auto v2 = rhs.template as<typename T::default_unit>();

	v1.storage -= v2.storage;
	lhs = unit::vec_t<T, N>(v1);
	return lhs;
}

template <gse::internal::is_quantity T, gse::internal::is_arithmetic U, int N>
constexpr auto gse::unit::operator*=(vec_t<T, N>& lhs, const U& rhs) -> vec_t<T, N>& {
	auto v1 = lhs.template as<typename T::default_unit>();

	v1.storage *= rhs;
	lhs = unit::vec_t<T, N>(v1);
	return lhs;
}

template <gse::internal::is_quantity T, gse::internal::is_arithmetic U, int N>
constexpr auto gse::unit::operator/=(vec_t<T, N>& lhs, const U& rhs) -> vec_t<T, N>& {
	auto v1 = lhs.template as<typename T::default_unit>();

	v1.storage /= rhs;
	lhs = unit::vec_t<T, N>(v1);
	return lhs;
}

template <gse::internal::is_quantity T, gse::internal::is_arithmetic U, int N>
constexpr auto gse::unit::operator+(const vec_t<T, N>& lhs, const unitless::vec_t<U, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>().storage + rhs.storage);
}

template <gse::internal::is_arithmetic T, gse::internal::is_quantity U, int N>
constexpr auto gse::unit::operator+(const unitless::vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.storage + rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_quantity T, gse::internal::is_arithmetic U, int N>
constexpr auto gse::unit::operator-(const vec_t<T, N>& lhs, const unitless::vec_t<U, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>().storage - rhs.storage);
}

template <gse::internal::is_arithmetic T, gse::internal::is_quantity U, int N>
constexpr auto gse::unit::operator-(const unitless::vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.storage - rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_quantity T, gse::internal::is_arithmetic U, int N>
constexpr auto gse::unit::operator*(const vec_t<T, N>& lhs, const unitless::vec_t<U, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>().storage * rhs.storage);
}

template <gse::internal::is_arithmetic T, gse::internal::is_quantity U, int N>
constexpr auto gse::unit::operator*(const unitless::vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.storage * rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_quantity T, gse::internal::is_arithmetic U, int N>
constexpr auto gse::unit::operator/(const vec_t<T, N>& lhs, const unitless::vec_t<U, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>().storage / rhs.storage);
}

template <gse::internal::is_arithmetic T, gse::internal::is_quantity U, int N>
constexpr auto gse::unit::operator/(const unitless::vec_t<T, N>& lhs, const vec_t<U, N>& rhs) -> vec_t<T, N> {
	return unit::vec_t<T, N>(lhs.storage / rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_arithmetic T, int N, gse::internal::is_quantity U>
constexpr auto gse::unit::operator*(const vec_t<T, N>& lhs, const U& rhs) -> vec_t<U, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>().storage * rhs.template as<typename T::default_unit>());
}

template <gse::internal::is_arithmetic T, int N, gse::internal::is_quantity U>
constexpr auto gse::unit::operator*(const U& lhs, const vec_t<T, N>& rhs) -> vec_t<U, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>() * rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_arithmetic T, int N, gse::internal::is_quantity U>
constexpr auto gse::unit::operator/(const vec_t<T, N>& lhs, const U& rhs) -> vec_t<U, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>().storage / rhs.template as<typename T::default_unit>());
}

template <gse::internal::is_arithmetic T, int N, gse::internal::is_quantity U>
constexpr auto gse::unit::operator/(const U& lhs, const vec_t<T, N>& rhs) -> vec_t<U, N> {
	return unit::vec_t<T, N>(lhs.template as<typename T::default_unit>() / rhs.template as<typename T::default_unit>().storage);
}

template <gse::internal::is_quantity T, int N>
constexpr auto gse::unit::operator-(const vec_t<T, N>& value) -> vec_t<T, N> {
	return unit::vec_t<T, N>(-value.template as<typename T::default_unit>().storage);
}

template <typename T, int N, gse::internal::is_quantity U>
constexpr auto gse::unitless::operator*(const vec_t<T, N>& lhs, const U& rhs) -> unit::vec_t<U, N> {
	return unit::vec_t<U, N>(lhs.storage * rhs.template as<typename U::default_unit>());
}

template <typename T, int N, gse::internal::is_quantity U>
constexpr auto gse::unitless::operator*(const U& lhs, const vec_t<T, N>& rhs) -> unit::vec_t<U, N> {
	return unit::vec_t<U, N>(lhs.template as<typename U::default_unit>() * rhs.storage);
}

template <typename T, int N, gse::internal::is_quantity U>
constexpr auto gse::unitless::operator/(const vec_t<T, N>& lhs, const U& rhs) -> unit::vec_t<U, N> {
	return unit::vec_t<U, N>(lhs.storage / rhs.template as<typename U::default_unit>());
}

template <typename T, int N, gse::internal::is_quantity U>
constexpr auto gse::unitless::operator/(const U& lhs, const vec_t<T, N>& rhs) -> unit::vec_t<U, N> {
	return unit::vec_t<U, N>(lhs.template as<typename U::default_unit>() / rhs.storage);
}

template <typename T, int N>
constexpr auto gse::magnitude(const vec_t<T, N>& v) -> T {
	return T(magnitude(v.template as<T::default_unit>()));
}

template <typename T, int N>
constexpr auto gse::is_zero(const vec_t<T, N>& v) -> bool {
	return is_zero(v.template as<T::default_unit>());
}

template <typename T, int N>
constexpr auto gse::normalize(const vec_t<T, N>& v) -> unitless::vec_t<typename T::value_type, N> {
	return normalize(v.template as<T::default_unit>());
}

template <typename T, int N>
constexpr auto gse::dot(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) {
	using result_q = decltype(T()* T());
	return unit::vec_t<result_q, N>(dot(lhs.template as<T::default_unit>(), rhs.template as<T::default_unit>()));
}

template <typename T, int N>
constexpr auto gse::dot(const vec_t<T, N>& lhs, const unitless::vec_t<typename T::value_type, N>& rhs) -> T {
	return dot(lhs.template as<T::default_unit>(), rhs);
}

template <typename T, int N>
constexpr auto gse::dot(const unitless::vec_t<typename T::value_type, N>& lhs, const vec_t<T, N>& rhs) -> T {
	return dot(lhs, rhs.template as<T::default_unit>());
}

template <typename T, typename U, int N>
constexpr auto gse::cross(const vec_t<T, N>& lhs, const vec_t<U, N>& rhs) {
	using result_q = decltype(T() * U());
	return vec_t<result_q, N>(cross(lhs.template as<typename T::default_unit>(), rhs.template as<typename U::default_unit>()));
}


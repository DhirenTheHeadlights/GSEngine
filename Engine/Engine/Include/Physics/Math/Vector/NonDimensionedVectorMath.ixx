export module gse.physics.math:vec_math;

import std;

import :angle;
import :base_vec;
import :unit_vec;
import :unitless_vec;

export namespace gse::unitless {
	template <typename T, int N>
	constexpr auto operator+(
		const vec_t<T, N>& lhs,
		const vec_t<T, N>& rhs
		) -> vec_t<T, N>;

	template <typename T, int N>
	constexpr auto operator-(
		const vec_t<T, N>& lhs,
		const vec_t<T, N>& rhs
		) -> vec_t<T, N>;

	template <typename T, int N>
	constexpr auto operator*(
		const vec_t<T, N>& lhs,
		const vec_t<T, N>& rhs
		) -> vec_t<T, N>;

	template <typename T, int N>
	constexpr auto operator/(
		const vec_t<T, N>& lhs,
		const vec_t<T, N>& rhs
		) -> vec_t<T, N>;

	template <typename T, int N>
	constexpr auto operator+=(
		vec_t<T, N>& lhs,
		const vec_t<T, N>& rhs
		) -> vec_t<T, N>&;

	template <typename T, int N>
	constexpr auto operator-=(
		vec_t<T, N>& lhs,
		const vec_t<T, N>& rhs
		) -> vec_t<T, N>&;

	template <typename T, int N>
	constexpr auto operator*=(
		vec_t<T, N>& lhs,
		const vec_t<T, N>& rhs
		) -> vec_t<T, N>&;

	template <typename T, int N>
	constexpr auto operator/=(
		vec_t<T, N>& lhs,
		const vec_t<T, N>& rhs
		) -> vec_t<T, N>&;

	template <typename T, int N>
	constexpr auto operator*(
		const vec_t<T, N>& lhs,
		const T& rhs
		) -> vec_t<T, N>;

	template <typename T, int N>
	constexpr auto operator*(
		const T& lhs,
		const vec_t<T, N>& rhs
		) -> vec_t<T, N>;

	template <typename T, int N>
	constexpr auto operator/(
		const vec_t<T, N>& lhs,
		const T& rhs
		) -> vec_t<T, N>;

	template <typename T, int N>
	constexpr auto operator/(
		const T& lhs,
		const vec_t<T, N>& rhs
		) -> vec_t<T, N>;

	template <typename T, int N>
	constexpr auto operator*=(
		vec_t<T, N>& lhs,
		const T& rhs
		) -> vec_t<T, N>&;

	template <typename T, int N>
	constexpr auto operator/=(
		vec_t<T, N>& lhs,
		const T& rhs
		) -> vec_t<T, N>&;

	template <typename T, int N>
	constexpr auto operator+(
		const vec_t<T, N>& value
		) -> vec_t<T, N>;

	template <typename T, int N>
	constexpr auto operator-(
		const vec_t<T, N>& value
		) -> vec_t<T, N>;
}

export namespace gse {
	template <typename T, int N>
	constexpr auto dot(
		const unitless::vec_t<T, N>& lhs,
		const unitless::vec_t<T, N>& rhs
	) -> T;

	template <typename T, int N>
	constexpr auto abs(
		const unitless::vec_t<T, N>& v
	) -> unitless::vec_t<T, N>;

	template <typename T>
	constexpr auto cross(
		const unitless::vec3_t<T>& lhs,
		const unitless::vec3_t<T>& rhs
	) -> unitless::vec3_t<T>;

	template <typename T, int N>
	constexpr auto magnitude(
		const unitless::vec_t<T, N>& v
	) -> T;

	template <typename T, int N>
	constexpr auto normalize(
		const unitless::vec_t<T, N>& v
	) -> unitless::vec_t<T, N>;

	template <typename T, int N>
	constexpr auto distance(
		const unitless::vec_t<T, N>& lhs,
		const unitless::vec_t<T, N>& rhs
	) -> T;

	template <typename T, int N>
	constexpr auto project(
		const unitless::vec_t<T, N>& a,
		const unitless::vec_t<T, N>& b
	) -> unitless::vec_t<T, N>;

	template <typename T, int N>
	constexpr auto reflect(
		const unitless::vec_t<T, N>& v,
		const unitless::vec_t<T, N>& normal
	) -> unitless::vec_t<T, N>;

	template <typename T, int N>
	constexpr auto angle_between(
		const unitless::vec_t<T, N>& a,
		const unitless::vec_t<T, N>& b
	) -> T;

	template <typename T, int N>
	constexpr auto lerp(
		const unitless::vec_t<T, N>& a,
		const unitless::vec_t<T, N>& b,
		T t
	) -> unitless::vec_t<T, N>;

	template <typename T, int N>
	constexpr auto min(
		const unitless::vec_t<T, N>& a,
		const unitless::vec_t<T, N>& b
	) -> unitless::vec_t<T, N>;

	template <typename T, int N>
	constexpr auto max(
		const unitless::vec_t<T, N>& a,
		const unitless::vec_t<T, N>& b
	) -> unitless::vec_t<T, N>;

	template <typename T, int N>
	constexpr auto clamp(
		const unitless::vec_t<T, N>& v,
		const unitless::vec_t<T, N>& min_val,
		const unitless::vec_t<T, N>& max_val
	) -> unitless::vec_t<T, N>;

	template <typename T, int N>
	constexpr auto min(
		const unitless::vec_t<T, N>& v,
		T scalar
	) -> unitless::vec_t<T, N>;

	template <typename T, int N>
	constexpr auto max(
		const unitless::vec_t<T, N>& v,
		T scalar
	) -> unitless::vec_t<T, N>;

	template <typename T, int N, int... Indices>
	constexpr auto swizzle(
		const unitless::vec_t<T, N>& v,
		std::integer_sequence<int, Indices...>
	) -> vec_t<T, sizeof...(Indices)>;

	template <typename T, int N>
	constexpr auto is_zero(
		const unitless::vec_t<T, N>& v,
		T epsilon = std::numeric_limits<T>::epsilon()
	) -> bool;

	template <typename T, int N>
	constexpr auto random_vector(
		T min = T(0),
		T max = T(1)
	) -> unitless::vec_t<T, N>;

	template <typename T, int N>
	constexpr auto is_normalized(
		const unitless::vec_t<T, N>& v,
		T epsilon = std::numeric_limits<T>::epsilon()
	) -> bool;

	template <typename T, int N>
	constexpr auto perpendicular(
		const vec2<T>& v
	) -> vec2<T>;

	template <typename T, int N>
	constexpr auto barycentric(
		const unitless::vec3_t<T>& p,
		const unitless::vec3_t<T>& a,
		const unitless::vec3_t<T>& b,
		const unitless::vec3_t<T>& c
	) -> std::tuple<T, T, T>;

	template <typename T, int N>
	constexpr auto exp(
		const unitless::vec_t<T, N>& v
	) -> unitless::vec_t<T, N>;

	template <typename T, int N>
	constexpr auto log(
		const unitless::vec_t<T, N>& v
	) -> unitless::vec_t<T, N>;

	template <typename T, int N>
	constexpr auto min_norm_vector(
		const unitless::vec_t<T, N>& a,
		const unitless::vec_t<T, N>& b
	) -> unitless::vec_t<T, N>;

	template <typename T, int N>
	constexpr auto max_norm_vector(
		const unitless::vec_t<T, N>& a,
		const unitless::vec_t<T, N>& b
	) -> unitless::vec_t<T, N>;

	template <typename T, int N>
	constexpr auto approx_equal(
		const unitless::vec_t<T, N>& a,
		const unitless::vec_t<T, N>& b,
		T epsilon = std::numeric_limits<T>::epsilon()
	) -> bool;

	template <typename T, int N>
	constexpr auto reflect_across(
		const unitless::vec_t<T, N>& a,
		const unitless::vec_t<T, N>& b
	) -> unitless::vec_t<T, N>;

	template <typename T, int N>
	constexpr auto hadamard_product(
		const unitless::vec_t<T, N>& a,
		const unitless::vec_t<T, N>& b
	) -> unitless::vec_t<T, N>;

	template <typename T, int N>
	constexpr auto sin(
		const unitless::vec_t<T, N>& v
	) -> unitless::vec_t<T, N>;

	template <typename T, int N>
	constexpr auto cos(
		const unitless::vec_t<T, N>& v
	) -> unitless::vec_t<T, N>;

	template <typename T, int N>
	constexpr auto ceil(
		const unitless::vec_t<T, N>& v
	) -> unitless::vec_t<T, N>;

	template <typename T, int N>
	constexpr auto floor(
		const unitless::vec_t<T, N>& v
	) -> unitless::vec_t<T, N>;

	template <typename T, int N>
	constexpr auto pow(
		const unitless::vec_t<T, N>& v,
		T exponent
	) -> unitless::vec_t<T, N>;

	template <typename T, int N>
	constexpr auto swizzle(
		const unitless::vec_t<T, N>& v
	) -> unitless::vec_t<T, N>;

	template <typename T, int N>
	constexpr auto rotate(
		const unitless::vec_t<T, N>& v,
		angle_t<T> angle,
		std::size_t i = 0,
		std::size_t j = 1
	) -> unitless::vec_t<T, N>;

	template <typename T, int N>
	constexpr auto epsilon_equal_index(
		const unitless::vec_t<T, N>& a,
		const unitless::vec_t<T, N>& b,
		int index,
		T epsilon = std::numeric_limits<T>::epsilon()
	) -> bool;
}

template <typename T, int N>
constexpr auto gse::unitless::operator+(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	return unitless::vec_t<T, N>(lhs.storage + rhs.storage);
}

template <typename T, int N>
constexpr auto gse::unitless::operator-(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	return unitless::vec_t<T, N>(lhs.storage - rhs.storage);
}

template <typename T, int N>
constexpr auto gse::unitless::operator*(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	return unitless::vec_t<T, N>(lhs.storage * rhs.storage);
}

template <typename T, int N>
constexpr auto gse::unitless::operator/(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	return unitless::vec_t<T, N>(lhs.storage / rhs.storage);
}

template <typename T, int N>
constexpr auto gse::unitless::operator+=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>& {
	lhs.storage += rhs.storage;
	return lhs;
}

template <typename T, int N>
constexpr auto gse::unitless::operator-=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>& {
	lhs.storage -= rhs.storage;
	return lhs;
}

template <typename T, int N>
constexpr auto gse::unitless::operator*=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>& {
	lhs.storage *= rhs.storage;
	return lhs;
}

template <typename T, int N>
constexpr auto gse::unitless::operator/=(vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N>& {
	lhs.storage /= rhs.storage;
	return lhs;
}

template <typename T, int N>
constexpr auto gse::unitless::operator*(const vec_t<T, N>& lhs, const T& rhs) -> vec_t<T, N> {
	return unitless::vec_t<T, N>(lhs.storage * rhs);
}

template <typename T, int N>
constexpr auto gse::unitless::operator*(const T& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	return unitless::vec_t<T, N>(lhs * rhs.storage);
}

template <typename T, int N>
constexpr auto gse::unitless::operator/(const vec_t<T, N>& lhs, const T& rhs) -> vec_t<T, N> {
	return unitless::vec_t<T, N>(lhs.storage / rhs);
}

template <typename T, int N>
constexpr auto gse::unitless::operator/(const T& lhs, const vec_t<T, N>& rhs) -> vec_t<T, N> {
	return unitless::vec_t<T, N>(lhs / rhs.storage);
}

template <typename T, int N>
constexpr auto gse::unitless::operator*=(vec_t<T, N>& lhs, const T& rhs) -> vec_t<T, N>& {
	lhs.storage *= rhs;
	return lhs;
}

template <typename T, int N>
constexpr auto gse::unitless::operator/=(vec_t<T, N>& lhs, const T& rhs) -> vec_t<T, N>& {
	lhs.storage /= rhs;
	return lhs;
}

template <typename T, int N>
constexpr auto gse::unitless::operator+(const vec_t<T, N>& value) -> vec_t<T, N> {
	return value;
}

template <typename T, int N>
constexpr auto gse::unitless::operator-(const vec_t<T, N>& value) -> vec_t<T, N> {
	return unitless::vec_t<T, N>(-value.storage);
}

template <typename T, int N> 
constexpr auto gse::dot(const unitless::vec_t<T, N>& lhs, const unitless::vec_t<T, N>& rhs) -> T {
	return vec::dot(lhs.storage, rhs.storage);
}

template <typename T, int N> 
constexpr auto gse::abs(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::abs(v[i]);
	}
	return result;
}

template <typename T> 
constexpr auto gse::cross(const unitless::vec3_t<T>& lhs, const unitless::vec3_t<T>& rhs) -> unitless::vec3_t<T> {
	return unitless::vec3_t<T>(
		lhs.y * rhs.z - lhs.z * rhs.y,
		lhs.z * rhs.x - lhs.x * rhs.z,
		lhs.x * rhs.y - lhs.y * rhs.x
	);
}

template <typename T, int N> 
constexpr auto gse::magnitude(const unitless::vec_t<T, N>& v) -> T {
	T sum = 0;
	for (int i = 0; i < N; ++i) {
		sum += v[i] * v[i];
	}
	return std::sqrt(sum);
}

template <typename T, int N> 
constexpr auto gse::normalize(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	auto mag = magnitude(v);
	if (mag == 0) {
		return {};
	}
	return v / mag;
}

template <typename T, int N> 
constexpr auto gse::distance(const unitless::vec_t<T, N>& lhs, const unitless::vec_t<T, N>& rhs) -> T {
	return magnitude(lhs - rhs);
}

template <typename T, int N> 
constexpr auto gse::project(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N> {
	auto b_normalized = normalize(b);
	return dot(a, b_normalized) * b_normalized;
}

template <typename T, int N> 
constexpr auto gse::reflect(const unitless::vec_t<T, N>& v, const unitless::vec_t<T, N>& normal) -> unitless::vec_t<T, N> {
	return v - 2 * dot(v, normal) * normal;
}

template <typename T, int N> 
constexpr auto gse::angle_between(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> T {
	auto dot_product = dot(a, b);
	auto mag_a = magnitude(a);
	auto mag_b = magnitude(b);
	if (mag_a == 0 || mag_b == 0) {
		return 0;
	}
	return std::acos(dot_product / (mag_a * mag_b));
}

template <typename T, int N> 
constexpr auto gse::lerp(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b, T t) -> unitless::vec_t<T, N> {
	return a + t * (b - a);
}

template <typename T, int N> 
constexpr auto gse::min(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::min(a[i], b[i]);
	}
	return result;
}

template <typename T, int N> 
constexpr auto gse::max(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::max(a[i], b[i]);
	}
	return result;
}

template <typename T, int N> 
constexpr auto gse::clamp(const unitless::vec_t<T, N>& v, const unitless::vec_t<T, N>& min_val, const unitless::vec_t<T, N>& max_val) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::clamp(v[i], min_val[i], max_val[i]);
	}
	return result;
}

template <typename T, int N> 
constexpr auto gse::min(const unitless::vec_t<T, N>& v, T scalar) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::min(v[i], scalar);
	}
	return result;
}

template <typename T, int N> 
constexpr auto gse::max(const unitless::vec_t<T, N>& v, T scalar) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::max(v[i], scalar);
	}
	return result;
}

template <typename T, int N, int... Indices> 
constexpr auto gse::swizzle(const unitless::vec_t<T, N>& v, std::integer_sequence<int, Indices...>) -> vec_t<T, sizeof...(Indices)> {
	return { v[Indices]... };
}

template <typename T, int N> 
constexpr auto gse::is_zero(const unitless::vec_t<T, N>& v, T epsilon) -> bool {
	return approx_equal(v, unitless::vec_t<T, N>{}, epsilon);
}

template <typename T, int N> 
constexpr auto gse::random_vector(T min, T max) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		
	}
	return result;
}

template <typename T, int N> 
constexpr auto gse::is_normalized(const unitless::vec_t<T, N>& v, T epsilon) -> bool {
	return std::abs(magnitude(v) - 1) < epsilon;
}

template <typename T, int N> 
constexpr auto gse::perpendicular(const vec2<T>& v) -> vec2<T> {
	return { -v.y, v.x };
}

template <typename T, int N> 
constexpr auto gse::barycentric(const unitless::vec3_t<T>& p, const unitless::vec3_t<T>& a, const unitless::vec3_t<T>& b, const unitless::vec3_t<T>& c) -> std::tuple<T, T, T> {
	vec_t<T, 3> v0 = b - a, v1 = c - a, v2 = p - a;
	T d00 = dot(v0, v0);
	T d01 = dot(v0, v1);
	T d11 = dot(v1, v1);
	T d20 = dot(v2, v0);
	T d21 = dot(v2, v1);
	T denom = d00 * d11 - d01 * d01;
	if (denom == T(0)) {
		return { T(0), T(0), T(0) };
	}
	T v = (d11 * d20 - d01 * d21) / denom;
	T w = (d00 * d21 - d01 * d20) / denom;
	T u = T(1) - v - w;
	return { u, v, w };
}

template <typename T, int N> 
constexpr auto gse::exp(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::exp(v[i]);
	}
	return result;
}

template <typename T, int N> 
constexpr auto gse::log(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::log(v[i]);
	}
	return result;
}

template <typename T, int N> 
constexpr auto gse::min_norm_vector(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::min(std::abs(a[i]), std::abs(b[i]));
	}
	return result;
}

template <typename T, int N> 
constexpr auto gse::max_norm_vector(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::max(std::abs(a[i]), std::abs(b[i]));
	}
	return result;
}

template <typename T, int N> 
constexpr auto gse::approx_equal(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b, T epsilon) -> bool {
	for (int i = 0; i < N; ++i) {
		if (std::abs(a[i] - b[i]) > epsilon) {
			return false;
		}
	}
	return true;
}

template <typename T, int N> 
constexpr auto gse::reflect_across(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N> {
	return a - 2 * dot(a, b) * b;
}

template <typename T, int N> 
constexpr auto gse::hadamard_product(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = a[i] * b[i];
	}
	return result;
}

template <typename T, int N> 
constexpr auto gse::sin(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::sin(v[i]);
	}
	return result;
}

template <typename T, int N> 
constexpr auto gse::cos(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::cos(v[i]);
	}
	return result;
}

template <typename T, int N> 
constexpr auto gse::ceil(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::ceil(v[i]);
	}
	return result;
}

template <typename T, int N> 
constexpr auto gse::floor(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::floor(v[i]);
	}
	return result;
}

template <typename T, int N> 
constexpr auto gse::pow(const unitless::vec_t<T, N>& v, T exponent) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::pow(v[i], exponent);
	}
	return result;
}

template <typename T, int N> 
constexpr auto gse::swizzle(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	return v;
}

template <typename T, int N>
constexpr auto gse::rotate(const unitless::vec_t<T, N>& v, angle_t<T> angle, std::size_t i, std::size_t j) -> unitless::vec_t<T, N> {
	if (i >= N || j >= N || i == j) {
		return v;
	}

	unitless::vec_t<T, N> result = v;
	const T rad = angle.template as<units::radians>();

	const T cos_theta = std::cos(rad);
	const T sin_theta = std::sin(rad);

	const T vi = v[i];
	const T vj = v[j];

	result[i] = vi * cos_theta - vj * sin_theta;
	result[j] = vi * sin_theta + vj * cos_theta;

	return result;
}

template <typename T, int N> 
constexpr auto gse::epsilon_equal_index(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b, const int index, T epsilon) -> bool {
	return std::abs(a[index] - b[index]) < epsilon;
}


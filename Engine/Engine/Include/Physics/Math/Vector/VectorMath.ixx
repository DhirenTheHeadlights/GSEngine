export module gse.physics.math.vec_math;

import std;

import gse.physics.math.base_vec;
import gse.physics.math.unit_vec;
import gse.physics.math.unitless_vec;

export namespace gse {
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto dot(const unitless::vec_t<T, N>& lhs, const unitless::vec_t<T, N>& rhs) -> T;
	template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto abs(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N>;
    template <typename T> requires std::is_arithmetic_v<T>						constexpr auto cross(const unitless::vec3_t<T>& lhs, const unitless::vec3_t<T>& rhs) -> unitless::vec3_t<T>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto magnitude(const unitless::vec_t<T, N>& v) -> T;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto normalize(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto distance(const unitless::vec_t<T, N>& lhs, const unitless::vec_t<T, N>& rhs) -> T;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto project(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto reflect(const unitless::vec_t<T, N>& v, const unitless::vec_t<T, N>& normal) -> unitless::vec_t<T, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto angle_between(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> T;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto lerp(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b, T t) -> unitless::vec_t<T, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto min(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto max(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto clamp(const unitless::vec_t<T, N>& v, const unitless::vec_t<T, N>& min_val, const unitless::vec_t<T, N>& max_val) -> unitless::vec_t<T, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto component_multiply(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto component_divide(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N>;
    template <typename T, typename U, int N> requires std::is_arithmetic_v<T>	constexpr auto convert(const unitless::vec_t<T, N>& v) -> vec_t<U, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto less_than(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> std::array<bool, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto greater_than(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> std::array<bool, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto min(const unitless::vec_t<T, N>& v, T scalar) -> unitless::vec_t<T, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto max(const unitless::vec_t<T, N>& v, T scalar) -> unitless::vec_t<T, N>;
    template <typename T, int N, int...Indices> requires std::is_arithmetic_v<T> constexpr auto swizzle(const unitless::vec_t<T, N>& v, std::integer_sequence<int, Indices...>) -> vec_t<T, sizeof...(Indices)>;
	template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto is_zero(const unitless::vec_t<T, N>& v, T epsilon = std::numeric_limits<T>::epsilon()) -> bool;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto random_vector(T min = T(0), T max = T(1)) -> unitless::vec_t<T, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto is_normalized(const unitless::vec_t<T, N>& v, T epsilon = std::numeric_limits<T>::epsilon()) -> bool;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto perpendicular(const vec2<T>& v) -> vec2<T>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto barycentric(const unitless::vec3_t<T>& p, const unitless::vec3_t<T>& a, const unitless::vec3_t<T>& b, const unitless::vec3_t<T>& c) -> std::tuple<T, T, T>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto exp(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto log(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto min_norm_vector(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto max_norm_vector(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto approx_equal(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b, T epsilon = std::numeric_limits<T>::epsilon()) -> bool;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto reflect_across(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto hadamard_product(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto sin(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto cos(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto ceil(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto floor(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto pow(const unitless::vec_t<T, N>& v, T exponent) -> unitless::vec_t<T, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto to_string(const unitless::vec_t<T, N>& v) -> std::string;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto swizzle(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N>;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto serialize(const unitless::vec_t<T, N>& v, std::ostream& os) -> std::ostream&;
    template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto deserialize(unitless::vec_t<T, N>& v, std::istream& is) -> std::istream&;
	template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto value_ptr(unitless::vec_t<T, N>& v) -> T*;
	template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto value_ptr(const unitless::vec_t<T, N>& v) -> const T*;
	template <typename T, int N> requires std::is_arithmetic_v<T>				constexpr auto epsilon_equal_index(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b, const int index, T epsilon = std::numeric_limits<T>::epsilon()) -> bool;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::dot(const unitless::vec_t<T, N>& lhs, const unitless::vec_t<T, N>& rhs) -> T {
	T result = 0;
	for (int i = 0; i < N; ++i) {
		result += lhs[i] * rhs[i];
	}
	return result;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::abs(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::abs(v[i]);
	}
	return result;
}

template <typename T> requires std::is_arithmetic_v<T>
constexpr auto gse::cross(const unitless::vec3_t<T>& lhs, const unitless::vec3_t<T>& rhs) -> unitless::vec3_t<T> {
	return unitless::vec3_t<T>(
		lhs.y * rhs.z - lhs.z * rhs.y,
		lhs.z * rhs.x - lhs.x * rhs.z,
		lhs.x * rhs.y - lhs.y * rhs.x
	);
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::magnitude(const unitless::vec_t<T, N>& v) -> T {
	T sum = 0;
	for (int i = 0; i < N; ++i) {
		sum += v[i] * v[i];
	}
	return std::sqrt(sum);
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::normalize(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	auto mag = magnitude(v);
	if (mag == 0) {
		return {};
	}
	return v / mag;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::distance(const unitless::vec_t<T, N>& lhs, const unitless::vec_t<T, N>& rhs) -> T {
	return magnitude(lhs - rhs);
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::project(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N> {
	auto b_normalized = normalize(b);
	return dot(a, b_normalized) * b_normalized;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::reflect(const unitless::vec_t<T, N>& v, const unitless::vec_t<T, N>& normal) -> unitless::vec_t<T, N> {
	return v - 2 * dot(v, normal) * normal;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::angle_between(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> T {
	auto dot_product = dot(a, b);
	auto mag_a = magnitude(a);
	auto mag_b = magnitude(b);
	if (mag_a == 0 || mag_b == 0) {
		return 0;
	}
	return std::acos(dot_product / (mag_a * mag_b));
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::lerp(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b, T t) -> unitless::vec_t<T, N> {
	return a + t * (b - a);
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::min(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::min(a[i], b[i]);
	}
	return result;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::max(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::max(a[i], b[i]);
	}
	return result;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::clamp(const unitless::vec_t<T, N>& v, const unitless::vec_t<T, N>& min_val, const unitless::vec_t<T, N>& max_val) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::clamp(v[i], min_val[i], max_val[i]);
	}
	return result;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::component_multiply(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = a[i] * b[i];
	}
	return result;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::component_divide(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = a[i] / b[i];
	}
	return result;
}

template <typename T, typename U, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::convert(const unitless::vec_t<T, N>& v) -> vec_t<U, N> {
	vec_t<U, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = static_cast<U>(v[i]);
	}
	return result;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::less_than(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> std::array<bool, N> {
	std::array<bool, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = a[i] < b[i];
	}
	return result;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::greater_than(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> std::array<bool, N> {
	std::array<bool, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = a[i] > b[i];
	}
	return result;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::min(const unitless::vec_t<T, N>& v, T scalar) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::min(v[i], scalar);
	}
	return result;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::max(const unitless::vec_t<T, N>& v, T scalar) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::max(v[i], scalar);
	}
	return result;
}

template <typename T, int N, int... Indices> requires std::is_arithmetic_v<T>
constexpr auto gse::swizzle(const unitless::vec_t<T, N>& v, std::integer_sequence<int, Indices...>) -> vec_t<T, sizeof...(Indices)> {
	return { v[Indices]... };
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::is_zero(const unitless::vec_t<T, N>& v, T epsilon) -> bool {
	return approx_equal(v, unitless::vec_t<T, N>{}, epsilon);
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::random_vector(T min, T max) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {

	}
	return result;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::is_normalized(const unitless::vec_t<T, N>& v, T epsilon) -> bool {
	return std::abs(magnitude(v) - 1) < epsilon;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::perpendicular(const vec2<T>& v) -> vec2<T> {
	return { -v.y, v.x };
}

template <typename T, int N> requires std::is_arithmetic_v<T>
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

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::exp(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::exp(v[i]);
	}
	return result;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::log(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::log(v[i]);
	}
	return result;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::min_norm_vector(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::min(std::abs(a[i]), std::abs(b[i]));
	}
	return result;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::max_norm_vector(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::max(std::abs(a[i]), std::abs(b[i]));
	}
	return result;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::approx_equal(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b, T epsilon) -> bool {
	for (int i = 0; i < N; ++i) {
		if (std::abs(a[i] - b[i]) > epsilon) {
			return false;
		}
	}
	return true;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::reflect_across(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N> {
	return a - 2 * dot(a, b) * b;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::hadamard_product(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = a[i] * b[i];
	}
	return result;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::sin(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::sin(v[i]);
	}
	return result;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::cos(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::cos(v[i]);
	}
	return result;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::ceil(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::ceil(v[i]);
	}
	return result;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::floor(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::floor(v[i]);
	}
	return result;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::pow(const unitless::vec_t<T, N>& v, T exponent) -> unitless::vec_t<T, N> {
	unitless::vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::pow(v[i], exponent);
	}
	return result;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::to_string(const unitless::vec_t<T, N>& v) -> std::string {
	std::ostringstream oss;
	oss << "(";
	for (int i = 0; i < N; ++i) {
		oss << v[i];
		if (i < N - 1) {
			oss << ", ";
		}
	}
	oss << ")";
	return oss.str();
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::swizzle(const unitless::vec_t<T, N>& v) -> unitless::vec_t<T, N> {
	return v;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::serialize(const unitless::vec_t<T, N>& v, std::ostream& os) -> std::ostream& {
	os << "{";
	for (int i = 0; i < N; ++i) {
		os << v[i];
		if (i < N - 1) {
			os << ", ";
		}
	}
	os << "}";
	return os;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::deserialize(unitless::vec_t<T, N>& v, std::istream& is) -> std::istream& {
	char ch;
	is >> ch;						 // Read the opening brace
	for (int i = 0; i < N; ++i) {
		is >> v[i];
		if (i < N - 1) {
			is >> ch;				// Read the comma
		}
	}
	is >> ch;						// Read the closing brace
	return is;
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::value_ptr(unitless::vec_t<T, N>& v) -> T* {
	return value_ptr(v.storage);
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::value_ptr(const unitless::vec_t<T, N>& v) -> const T* {
	return value_ptr(v.storage);
}

template <typename T, int N> requires std::is_arithmetic_v<T>
constexpr auto gse::epsilon_equal_index(const unitless::vec_t<T, N>& a, const unitless::vec_t<T, N>& b, const int index, T epsilon) -> bool {
	return std::abs(a[index] - b[index]) < epsilon;
}


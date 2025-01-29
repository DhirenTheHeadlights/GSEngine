export module gse.physics.math.vec_math;

import std;

import gse.physics.math.unit_vec;
import gse.physics.math.matrix;

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

template <typename T, int N>
constexpr auto gse::dot(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> T {
	T result = 0;
	for (int i = 0; i < N; ++i) {
		result += lhs[i] * rhs[i];
	}
	return result;
}

template <typename T>
constexpr auto gse::cross(const vec3<T>& lhs, const vec3<T>& rhs) -> vec3<T> {
	return vec3_t<T>(
		lhs.y * rhs.z - lhs.z * rhs.y,
		lhs.z * rhs.x - lhs.x * rhs.z,
		lhs.x * rhs.y - lhs.y * rhs.x
	);
}

template <typename T, int N>
constexpr auto gse::magnitude(const vec_t<T, N>& v) -> T {
	T sum = 0;
	for (int i = 0; i < N; ++i) {
		sum += v[i] * v[i];
	}
	return std::sqrt(sum);
}

template <typename T, int N>
constexpr auto gse::normalize(const vec_t<T, N>& v) -> vec_t<unitless, N> {
	auto mag = magnitude(v);
	if (mag == 0) {
		return vec_t<unitless, N>{};
	}
	return v / mag;
}

template <typename T, int N>
constexpr auto gse::distance(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) -> T {
	return magnitude(lhs - rhs);
}

template <typename T, int N>
constexpr auto gse::project(const vec_t<T, N>& a, const vec_t<T, N>& b) -> vec_t<unitless, N> {
	auto b_normalized = normalize(b);
	return dot(a, b_normalized) * b_normalized;
}

template <typename T, int N>
constexpr auto gse::reflect(const vec_t<T, N>& v, const vec_t<T, N>& normal) -> vec_t<unitless, N> {
	return v - 2 * dot(v, normal) * normal;
}

template <typename T, int N>
constexpr auto gse::angle_between(const vec_t<T, N>& a, const vec_t<T, N>& b) -> T {
	auto dot_product = dot(a, b);
	auto mag_a = magnitude(a);
	auto mag_b = magnitude(b);
	if (mag_a == 0 || mag_b == 0) {
		return 0;
	}
	return std::acos(dot_product / (mag_a * mag_b));
}

template <typename T, int N>
constexpr auto gse::lerp(const vec_t<T, N>& a, const vec_t<T, N>& b, T t) -> vec_t<unitless, N> {
	return a + t * (b - a);
}

template <typename T, int N>
constexpr auto gse::min(const vec_t<T, N>& a, const vec_t<T, N>& b) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::min(a[i], b[i]);
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::max(const vec_t<T, N>& a, const vec_t<T, N>& b) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::max(a[i], b[i]);
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::clamp(const vec_t<T, N>& v, const vec_t<T, N>& min_val, const vec_t<T, N>& max_val) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::clamp(v[i], min_val[i], max_val[i]);
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::component_multiply(const vec_t<T, N>& a, const vec_t<T, N>& b) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = a[i] * b[i];
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::component_divide(const vec_t<T, N>& a, const vec_t<T, N>& b) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = a[i] / b[i];
	}
	return result;
}

template <typename T>
constexpr auto gse::projection_matrix(const vec3<T>& normal) -> mat_t<typename vec3<T>::value_type, 3, 3> {
	return gse::mat_t<T, 3, 3>{
		1 - normal.x * normal.x, -normal.x * normal.y, -normal.x * normal.z,
			-normal.y * normal.x, 1 - normal.y * normal.y, -normal.y * normal.z,
			-normal.z * normal.x, -normal.z * normal.y, 1 - normal.z * normal.z
	};
}

template <typename T>
constexpr auto gse::reflection_matrix(const vec3<T>& normal) -> mat_t<typename vec3<T>::value_type, 3, 3> {
	return gse::mat_t<T, 3, 3>{
		1 - 2 * normal.x * normal.x, -2 * normal.x * normal.y, -2 * normal.x * normal.z,
			-2 * normal.y * normal.x, 1 - 2 * normal.y * normal.y, -2 * normal.y * normal.z,
			-2 * normal.z * normal.x, -2 * normal.z * normal.y, 1 - 2 * normal.z * normal.z
	};
}

template <typename T, typename U, int N>
constexpr auto gse::convert(const vec_t<T, N>& v) -> vec_t<U, N> {
	vec_t<U, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = static_cast<U>(v[i]);
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::less_than(const vec_t<T, N>& a, const vec_t<T, N>& b) -> std::array<bool, N> {
	std::array<bool, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = a[i] < b[i];
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::greater_than(const vec_t<T, N>& a, const vec_t<T, N>& b) -> std::array<bool, N> {
	std::array<bool, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = a[i] > b[i];
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::min(const vec_t<T, N>& v, T scalar) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::min(v[i], scalar);
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::max(const vec_t<T, N>& v, T scalar) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::max(v[i], scalar);
	}
	return result;
}

template <typename T, int N, int... Indices>
constexpr auto gse::swizzle(const vec_t<T, N>& v, std::integer_sequence<int, Indices...>) -> vec_t<T, sizeof...(Indices)> {
	return { v[Indices]... };
}

template <typename T, int N>
constexpr auto gse::is_zero(const vec_t<T, N>& v, typename vec_t<T, N>::value_type epsilon) -> bool {
	return approx_equal(v, vec_t<T, N>{}, epsilon);
}

template <typename T, int N>
constexpr auto gse::random_vector(T min, T max) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {

	}
	return result;
}

template <typename T, int N>
constexpr auto gse::is_normalized(const vec_t<T, N>& v, T epsilon) -> bool {
	return std::abs(magnitude(v) - 1) < epsilon;
}

template <typename T>
constexpr auto gse::perpendicular(const vec2<T>& v) -> vec2<T> {
	return { -v.y, v.x };
}

template <typename T>
constexpr auto gse::barycentric(const vec3<T>& p, const vec3<T>& a, const vec3<T>& b, const vec3<T>& c) -> std::tuple<T, T, T> {
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

template <typename T, int N, int M>
constexpr auto gse::multiply(const mat_t<T, M, N>& matrix, const vec_t<T, N>& vector) -> vec_t<T, M> {
	vec_t<T, M> result;
	for (int i = 0; i < M; ++i) {
		result[i] = 0;
		for (int j = 0; j < N; ++j) {
			result[i] += matrix[i][j] * vector[j];
		}
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::exp(const vec_t<T, N>& v) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::exp(v[i]);
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::log(const vec_t<T, N>& v) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::log(v[i]);
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::min_norm_vector(const vec_t<T, N>& a, const vec_t<T, N>& b) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::min(std::abs(a[i]), std::abs(b[i]));
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::max_norm_vector(const vec_t<T, N>& a, const vec_t<T, N>& b) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::max(std::abs(a[i]), std::abs(b[i]));
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::approx_equal(const vec_t<T, N>& a, const vec_t<T, N>& b, typename vec_t<T, N>::value_type epsilon) -> bool {
	for (int i = 0; i < N; ++i) {
		if (std::abs(a[i] - b[i]) > epsilon) {
			return false;
		}
	}
	return true;
}

template <typename T, int N>
constexpr auto gse::reflect_across(const vec_t<T, N>& a, const vec_t<T, N>& b) -> vec_t<unitless, N> {
	return a - 2 * dot(a, b) * b;
}

template <typename T, int N>
constexpr auto gse::hadamard_product(const vec_t<T, N>& a, const vec_t<T, N>& b) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = a[i] * b[i];
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::sin(const vec_t<T, N>& v) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::sin(v[i]);
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::cos(const vec_t<T, N>& v) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::cos(v[i]);
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::ceil(const vec_t<T, N>& v) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::ceil(v[i]);
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::floor(const vec_t<T, N>& v) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::floor(v[i]);
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::pow(const vec_t<T, N>& v, T exponent) -> vec_t<T, N> {
	vec_t<T, N> result;
	for (int i = 0; i < N; ++i) {
		result[i] = std::pow(v[i], exponent);
	}
	return result;
}

template <typename T, int N>
constexpr auto gse::to_string(const vec_t<T, N>& v) -> std::string {
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

template <typename T, int N>
constexpr auto gse::swizzle(const vec_t<T, N>& v) -> vec_t<T, N> {
	return v;
}

template <typename T, int N>
constexpr auto gse::serialize(const vec_t<T, N>& v, std::ostream& os) -> std::ostream& {
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

template <typename T, int N>
constexpr auto gse::deserialize(vec_t<T, N>& v, std::istream& is) -> std::istream& {
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


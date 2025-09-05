export module gse.physics.math:quat_math;

import std;

import :angle;
import :vector;
import :vector_math;
import :quat;
import :matrix;

export namespace gse {
	template <typename T>
	constexpr auto operator+(
		const quat_t<T>& lhs,
		const quat_t<T>& rhs
	) -> quat_t<T>;

	template <typename T>
	constexpr auto operator-(
		const quat_t<T>& lhs,
		const quat_t<T>& rhs
	) -> quat_t<T>;

	template <typename T>
	constexpr auto operator*(
		const quat_t<T>& lhs,
		const quat_t<T>& rhs
	) -> quat_t<T>;

	template <typename T>
	constexpr auto operator/(
		const quat_t<T>& lhs,
		const quat_t<T>& rhs
	) -> quat_t<T>;

	template <typename T>
	constexpr auto operator+=(
		quat_t<T>& lhs,
		const quat_t<T>& rhs
	) -> quat_t<T>&;

	template <typename T>
	constexpr auto operator-=(
		quat_t<T>& lhs,
		const quat_t<T>& rhs
	) -> quat_t<T>&;

	template <typename T>
	constexpr auto operator*=(
		quat_t<T>& lhs,
		const quat_t<T>& rhs
	) -> quat_t<T>&;

	template <typename T>
	constexpr auto operator/=(
		quat_t<T>& lhs,
		const quat_t<T>& rhs
	) -> quat_t<T>&;

	template <typename T>
	constexpr auto operator*(
		const quat_t<T>& lhs,
		const T& rhs
	) -> quat_t<T>;

	template <typename T>
	constexpr auto operator*(
		const T& lhs,
		const quat_t<T>& rhs
	) -> quat_t<T>;

	template <typename T>
	constexpr auto operator/(
		const quat_t<T>& lhs,
		const T& rhs
	) -> quat_t<T>;

	template <typename T>
	constexpr auto operator/(
		const T& lhs,
		const quat_t<T>& rhs
	) -> quat_t<T>;

	template <typename T>
	constexpr auto operator*(
		const quaternion<T>& lhs,
		const unitless::vec3_t<T>& rhs
	) -> unitless::vec3_t<T>;

	template <typename T>
	constexpr auto operator*=(
		quat_t<T>& lhs,
		const T& rhs
	) -> quat_t<T>&;

	template <typename T>
	constexpr auto operator/=(
		quat_t<T>& lhs,
		const T& rhs
	) -> quat_t<T>&;

	template <typename T>
	constexpr auto normalize(
		const quat_t<T>& q
	) -> quat_t<T>;

	template <typename T>
	constexpr auto conjugate(
		const quat_t<T>& q
	) -> quat_t<T>;

	template <typename T>
	constexpr auto norm_squared(
		const quat_t<T>& q
	) -> T;

	template <typename T>
	constexpr auto inverse(
		const quat_t<T>& q
	) -> quat_t<T>;

	template <typename T>
	constexpr auto dot(
		const quat_t<T>& lhs,
		const quat_t<T>& rhs
	) -> T;

	template <typename T>
	constexpr auto mat3_cast(
		const quat_t<T>& q
	) -> mat3_t<T>;

	template <typename T>
	constexpr auto from_mat4(
		const mat4_t<T>& m
	) -> quat_t<T>;

	template <typename T>
	constexpr auto identity(
	) -> quat_t<T>;

	template <typename T>
	constexpr auto from_axis_angle(
		const unitless::vec3_t<T>& axis,
		angle_t<T> angle
	) -> quat_t<T>;
}

template <typename T>
constexpr auto gse::operator+(const quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T> {
	return quat_t<T>(lhs.v4() + rhs.v4());
}

template <typename T>
constexpr auto gse::operator-(const quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T> {
	return quat_t<T>(lhs.v4() - rhs.v4());
}

template <typename T>
constexpr auto gse::operator*(const quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T> {
	const T s1 = lhs[0];
	const T s2 = rhs[0];
	const unitless::vec3_t<T> v1{ lhs[1], lhs[2], lhs[3] };
	const unitless::vec3_t<T> v2{ rhs[1], rhs[2], rhs[3] };
	const unitless::vec3_t<T> vec_part = s1 * v2 + s2 * v1 + cross(v1, v2);
	const T scalar_part = s1 * s2 - dot(v1, v2);
	return quat_t<T>(vec_part, scalar_part);
}

template <typename T>
constexpr auto gse::operator/(const quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T> {
	return lhs * inverse(rhs);
}

template <typename T>
constexpr auto gse::operator+=(quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T>& {
	lhs = lhs + rhs;
	return lhs;
}

template <typename T>
constexpr auto gse::operator-=(quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T>& {
	lhs = lhs - rhs;
	return lhs;
}

template <typename T>
constexpr auto gse::operator*=(quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T>& {
	lhs = lhs * rhs;
	return lhs;
}

template <typename T>
constexpr auto gse::operator/=(quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T>& {
	lhs = lhs / rhs;
	return lhs;
}

template <typename T>
constexpr auto gse::operator*(const quat_t<T>& lhs, const T& rhs) -> quat_t<T> {
	return quat_t<T>(lhs.v4() * rhs);
}

template <typename T>
constexpr auto gse::operator*(const T& lhs, const quat_t<T>& rhs) -> quat_t<T> {
	return rhs * lhs;
}

template <typename T>
constexpr auto gse::operator/(const quat_t<T>& lhs, const T& rhs) -> quat_t<T> {
	return quat_t<T>(lhs.v4() / rhs);
}

template <typename T>
constexpr auto gse::operator/(const T& lhs, const quat_t<T>& rhs) -> quat_t<T> {
	const quat_t<T> s_quat{ lhs, T(0), T(0), T(0) };
	return s_quat / rhs;
}

template <typename T>
constexpr auto gse::operator*(const quaternion<T>& lhs, const unitless::vec3_t<T>& rhs) -> unitless::vec3_t<T> {
	const unitless::vec3_t<T> u{ lhs[1], lhs[2], lhs[3] };
	const T s = lhs[0];
	const unitless::vec3_t<T> t = T(2) * cross(u, rhs);
	return rhs + s * t + cross(u, t);
}

template <typename T>
constexpr auto gse::operator*=(quat_t<T>& lhs, const T& rhs) -> quat_t<T>& {
	lhs = lhs * rhs;
	return lhs;
}

template <typename T>
constexpr auto gse::operator/=(quat_t<T>& lhs, const T& rhs) -> quat_t<T>& {
	lhs = lhs / rhs;
	return lhs;
}

template <typename T>
constexpr auto gse::normalize(const quat_t<T>& q) -> quat_t<T> {
	const T n2 = dot(q.v4(), q.v4());
	if (n2 == T(0)) {
		return quat_t<T>{};
	}
	const T inv_n = T(1) / std::sqrt(n2);
	return quat_t<T>(q.v4() * inv_n);
}

template <typename T>
constexpr auto gse::conjugate(const quat_t<T>& q) -> quat_t<T> {
	const unitless::vec4_t<T> mask{ T(1), T(-1), T(-1), T(-1) };
	return quat_t<T>(q.v4() * mask);
}

template <typename T>
constexpr auto gse::norm_squared(const quat_t<T>& q) -> T {
	return dot(q.v4(), q.v4());
}

template <typename T>
constexpr auto gse::inverse(const quat_t<T>& q) -> quat_t<T> {
	return conjugate(q) / norm_squared(q);
}

template <typename T>
constexpr auto gse::dot(const quat_t<T>& lhs, const quat_t<T>& rhs) -> T {
	return dot(lhs.v4(), rhs.v4());
}

template <typename T>
constexpr auto gse::mat3_cast(const quat_t<T>& q) -> mat3_t<T> {
	const T x = q[1];
	const T y = q[2];
	const T z = q[3];
	const T w = q[0];
	const T x2 = x + x;
	const T y2 = y + y;
	const T z2 = z + z;
	const T xx = x * x2;
	const T xy = x * y2;
	const T xz = x * z2;
	const T yy = y * y2;
	const T yz = y * z2;
	const T zz = z * z2;
	const T wx = w * x2;
	const T wy = w * y2;
	const T wz = w * z2;
	return mat3_t<T>{
		unitless::vec3_t<T>{ T(1) - (yy + zz), xy + wz, xz - wy },
		unitless::vec3_t<T>{ xy - wz, T(1) - (xx + zz), yz + wx },
		unitless::vec3_t<T>{ xz + wy, yz - wx, T(1) - (xx + yy) }
	};
}

template <typename T>
constexpr auto gse::from_mat4(const mat4_t<T>& m) -> quat_t<T> {
	const T trace = m[0][0] + m[1][1] + m[2][2];
	quat_t<T> q{};
	if (trace > T(0)) {
		const T s = std::sqrt(trace + T(1)) * T(2);
		q[0] = T(0.25) * s;
		q[1] = (m[1][2] - m[2][1]) / s;
		q[2] = (m[2][0] - m[0][2]) / s;
		q[3] = (m[0][1] - m[1][0]) / s;
	}
	else if ((m[0][0] > m[1][1]) && (m[0][0] > m[2][2])) {
		const T s = std::sqrt(T(1) + m[0][0] - m[1][1] - m[2][2]) * T(2);
		q[0] = (m[1][2] - m[2][1]) / s;
		q[1] = T(0.25) * s;
		q[2] = (m[1][0] + m[0][1]) / s;
		q[3] = (m[2][0] + m[0][2]) / s;
	}
	else if (m[1][1] > m[2][2]) {
		const T s = std::sqrt(T(1) + m[1][1] - m[0][0] - m[2][2]) * T(2);
		q[0] = (m[2][0] - m[0][2]) / s;
		q[1] = (m[1][0] + m[0][1]) / s;
		q[2] = T(0.25) * s;
		q[3] = (m[2][1] + m[1][2]) / s;
	}
	else {
		const T s = std::sqrt(T(1) + m[2][2] - m[0][0] - m[1][1]) * T(2);
		q[0] = (m[0][1] - m[1][0]) / s;
		q[1] = (m[2][0] + m[0][2]) / s;
		q[2] = (m[2][1] + m[1][2]) / s;
		q[3] = T(0.25) * s;
	}
	return q;
}

template <typename T>
constexpr auto gse::identity() -> quat_t<T> {
	return quat_t<T>{ T(1), T(0), T(0), T(0) };
}

template <typename T>
constexpr auto gse::from_axis_angle(const unitless::vec3_t<T>& axis, angle_t<T> angle) -> quat_t<T> {
	const T half_angle = angle.template as<radians>() / T(2);
	const T s = std::sin(half_angle);
	const T c = std::cos(half_angle);
	return quat_t<T>{ c, axis[0] * s, axis[1] * s, axis[2] * s };
}

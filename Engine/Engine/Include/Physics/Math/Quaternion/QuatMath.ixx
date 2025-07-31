export module gse.physics.math:quat_math;

import std;

import :angle;
import :unitless_vec;
import :quat;
import :matrix;

export namespace gse {
	template <typename T> constexpr auto normalize(const quat_t<T>& q) -> quat_t<T>;
	template <typename T> constexpr auto conjugate(const quat_t<T>& q) -> quat_t<T>;
	template <typename T> constexpr auto norm_sqrd(const quat_t<T>& q) -> T;
	template <typename T> constexpr auto inverse(const quat_t<T>& q) -> quat_t<T>;
	template <typename T> constexpr auto dot(const quat_t<T>& lhs, const quat_t<T>& rhs) -> T;
	template <typename T> constexpr auto mat3_cast(const quat_t<T>& q) -> mat3_t<T>;
	template <typename T> constexpr auto identity() -> quat_t<T>;
	template <typename T> constexpr auto from_axis_angle(const unitless::vec3_t<T>& axis, angle_t<T> angle) -> quat_t<T>;
}

template <typename T>
constexpr auto gse::normalize(const quat_t<T>& q) -> quat_t<T> {
	auto mag = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.s * q.s);
	if (mag == 0) {
		return quat_t<T>{};
	}
	return { q.s / mag, q.x / mag, q.y / mag, q.z / mag };
}

template <typename T>
constexpr auto gse::conjugate(const quat_t<T>& q) -> quat_t<T> {
	return { q.s, -q.x, -q.y, -q.z };
}

template <typename T>
constexpr auto gse::norm_sqrd(const quat_t<T>& q) -> T {
	return dot(q, q);
}

template <typename T>
constexpr auto gse::inverse(const quat_t<T>& q) -> quat_t<T> {
	return conjugate(q) / norm_sqrd(q);
}

template <typename T>
constexpr auto gse::dot(const quat_t<T>& lhs, const quat_t<T>& rhs) -> T {
	T result = 0;
	for (int i = 0; i < 4; ++i) {
		result += lhs[i] * rhs[i];
	}
	return result;
}

template <typename T>
constexpr auto gse::mat3_cast(const quat_t<T>& q) -> mat3_t<T> {
	auto x = q.x;
	auto y = q.y;
	auto z = q.z;
	auto w = q.s;
	auto x2 = x + x;
	auto y2 = y + y;
	auto z2 = z + z;
	auto xx = x * x2;
	auto xy = x * y2;
	auto xz = x * z2;
	auto yy = y * y2;
	auto yz = y * z2;
	auto zz = z * z2;
	auto wx = w * x2;
	auto wy = w * y2;
	auto wz = w * z2;
	return mat3_t<T>{
			unitless::vec3_t<T>{1 - (yy + zz), xy + wz, xz - wy},
			unitless::vec3_t<T>{xy - wz, 1 - (xx + zz), yz + wx},
			unitless::vec3_t<T>{xz + wy, yz - wx, 1 - (xx + yy)}
	};
}

template <typename T>
constexpr auto gse::identity() -> gse::quat_t<T> {
	return quat_t<T>{ 1, 0, 0, 0 };
}

template <typename T>
constexpr auto gse::from_axis_angle(const unitless::vec3_t<T>& axis, angle_t<T> angle) -> quat_t<T> {
	T half_angle = angle.template as<units::radians>() / 2;
	T s = std::sin(half_angle);
	T c = std::cos(half_angle);
	return quat_t<T>{ c, axis.x * s, axis.y * s, axis.z * s };
}
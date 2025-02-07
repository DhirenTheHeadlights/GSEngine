export module gse.physics.math.quat;

import std;
import gse.physics.math.unitless_vec;
import gse.physics.math.base_vec;
import gse.physics.math.vec_math;
import gse.physics.math.units;

namespace gse {
	template <typename T>
		requires std::is_arithmetic_v<T>
	struct quaternion {
		union {
			internal::vec_storage<T, 4> v;
			struct {
				T s, x, y, z;
			};
		};

		constexpr quaternion() : v{ 1, 0, 0, 0 } {}
		constexpr quaternion(const unitless::vec_t<T, 4>& vec) : v(vec) {}
		constexpr quaternion(const unitless::vec_t<T, 3>& vec, const T scalar) : v{ scalar, vec.x, vec.y, vec.z } {}
		constexpr quaternion(const T scalar, const unitless::vec_t<T, 3>& vec) : v{ scalar, vec.x, vec.y, vec.z } {}
		constexpr quaternion(T s, T x, T y, T z) : v{ s, x, y, z } {}
		constexpr quaternion(const unitless::vec_t<T, 3>& axis, angle_t<T> angle);
		constexpr quaternion(angle_t<T> angle, const unitless::vec_t<T, 3>& axis);
		constexpr auto operator[](size_t index) -> T&;
		constexpr auto operator[](size_t index) const -> T;
		constexpr auto imaginary_part() const -> unitless::vec_t<T, 3> { return { x, y, z }; }
	};

	template <typename T>
		requires std::is_arithmetic_v<T>
	constexpr quaternion<T>::quaternion(const unitless::vec_t<T, 3>& axis, angle_t<T> angle) {
		auto half_angle = angle.template as<units::radians>() / T(2);
		auto sin_half_angle = std::sin(half_angle);
		*this = quaternion(axis * sin_half_angle, std::cos(half_angle));
	}

	template <typename T>
		requires std::is_arithmetic_v<T>
	constexpr quaternion<T>::quaternion(angle_t<T> angle, const unitless::vec_t<T, 3>& axis)
		: quaternion(axis, angle) {}

	template <typename T>
		requires std::is_arithmetic_v<T>
	constexpr auto quaternion<T>::operator[](size_t index) -> T& {
		return v.data[index < 4 ? index : 0];
	}

	template <typename T>
		requires std::is_arithmetic_v<T>
	constexpr auto quaternion<T>::operator[](size_t index) const -> T {
		return v.data[index < 4 ? index : 0];
	}
}

export namespace gse {
	template <typename T> using quat_t = quaternion<T>;
	using quati = quaternion<int>;
	using quat = quaternion<float>;
	using quatd = quaternion<double>;

	template <typename T> constexpr auto operator+(const quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T>;
	template <typename T> constexpr auto operator-(const quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T>;
	template <typename T> constexpr auto operator*(const quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T>;
	template <typename T> constexpr auto operator/(const quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T>;

	template <typename T> constexpr auto operator+=(quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T>&;
	template <typename T> constexpr auto operator-=(quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T>&;
	template <typename T> constexpr auto operator*=(quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T>&;
	template <typename T> constexpr auto operator/=(quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T>&;

	template <typename T> constexpr auto operator*(const quat_t<T>& lhs, const T& rhs) -> quat_t<T>;
	template <typename T> constexpr auto operator*(const T& lhs, const quat_t<T>& rhs) -> quat_t<T>;
	template <typename T> constexpr auto operator/(const quat_t<T>& lhs, const T& rhs) -> quat_t<T>;
	template <typename T> constexpr auto operator/(const T& lhs, const quat_t<T>& rhs) -> quat_t<T>;

	template <typename T> constexpr auto operator*=(quat_t<T>& lhs, const T& rhs) -> quat_t<T>&;
	template <typename T> constexpr auto operator/=(quat_t<T>& lhs, const T& rhs) -> quat_t<T>&;
}

template <typename T>
constexpr auto gse::operator+(const quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T> {
	quat_t<T> result;
	for (int i = 0; i < 4; ++i)
		result[i] = lhs[i] + rhs[i];
	return result;
}

template <typename T>
constexpr auto gse::operator-(const quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T> {
	quat_t<T> result;
	for (int i = 0; i < 4; ++i)
		result[i] = lhs[i] - rhs[i];
	return result;
}

template <typename T>
constexpr auto gse::operator*(const quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T> {
	auto v1 = unitless::vec_t<T, 3>{ lhs.x, lhs.y, lhs.z };
	auto v2 = unitless::vec_t<T, 3>{ rhs.x, rhs.y, rhs.z };
	auto vec_part = lhs.s * v2 + rhs.s * v1 + cross(v1, v2);
	T scalar_part = lhs.s * rhs.s - dot(v1, v2);
	return quat_t<T>(vec_part, scalar_part);
}

template <typename T>
constexpr auto gse::operator/(const quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T> {
	quat_t<T> rhs_conj(-rhs.x, -rhs.y, -rhs.z, rhs.s);
	T norm_sq = rhs.x * rhs.x + rhs.y * rhs.y + rhs.z * rhs.z + rhs.s * rhs.s;
	return lhs * rhs_conj / norm_sq;
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
	return quat_t<T>(lhs.s * rhs, lhs.x * rhs, lhs.y * rhs, lhs.z * rhs);
}

template <typename T>
constexpr auto gse::operator*(const T& lhs, const quat_t<T>& rhs) -> quat_t<T> {
	return rhs * lhs;
}

template <typename T>
constexpr auto gse::operator/(const quat_t<T>& lhs, const T& rhs) -> quat_t<T> {
	return quat_t<T>(lhs.x / rhs, lhs.y / rhs, lhs.z / rhs, lhs.s / rhs);
}

template <typename T>
constexpr auto gse::operator/(const T& lhs, const quat_t<T>& rhs) -> quat_t<T> {
	T norm_sq = rhs.x * rhs.x + rhs.y * rhs.y + rhs.z * rhs.z + rhs.s * rhs.s;
	quat_t<T> rhs_conj(rhs.s, -rhs.x, -rhs.y, -rhs.z);
	return lhs * (rhs_conj / norm_sq);
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

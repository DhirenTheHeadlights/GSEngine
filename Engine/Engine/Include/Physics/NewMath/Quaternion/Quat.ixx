export module gse.physics.math.quat;

import std;

import gse.physics.math.unit_vec;
import gse.physics.math.vec_math;
import gse.physics.math.units.ang;

namespace gse {
	template <typename T>
	struct quaternion {
		unitless_vec_t<T, 3> v;
		T s;

		constexpr quaternion() : v{ 0, 0, 0 }, s{ 1 } {}
		constexpr quaternion(const unitless_vec_t<T, 3>& v, T s) : v(v), s(s) {}
		constexpr quaternion(T x, T y, T z, T s) : v(x, y, z), s(s) {}
		constexpr quaternion(const unitless_vec_t<T, 3>& axis, angle_t<T> angle);
		constexpr quaternion(angle_t<T> angle, const unitless_vec_t<T, 3>& axis);

		constexpr auto operator[](size_t index)->T&;
		constexpr auto operator[](size_t index) const -> T;

		constexpr auto operator==(const quaternion& other) const -> bool;
		constexpr auto operator!=(const quaternion& other) const -> bool;

		constexpr auto operator+(const quaternion& other) const -> quaternion;
		constexpr auto operator-(const quaternion& other) const -> quaternion;
		constexpr auto operator*(const quaternion& other) const -> quaternion;
		constexpr auto operator*(T scalar) const -> quaternion;
		constexpr auto operator/(T scalar) const -> quaternion;

		constexpr auto operator+=(const quaternion& other) -> quaternion&;
		constexpr auto operator-=(const quaternion& other) -> quaternion&;
		constexpr auto operator*=(const quaternion& other) -> quaternion&;
		constexpr auto operator*=(T scalar) -> quaternion&;
		constexpr auto operator/=(T scalar) -> quaternion&;

		constexpr auto operator-() const -> quaternion;
	};
}

export namespace gse {
	template <typename T> using quat_t = quaternion<T>;

	using quati = quaternion<int>;
	using quat = quaternion<float>;
	using quatd = quaternion<double>;
}

template <typename T>
constexpr gse::quaternion<T>::quaternion(const unitless_vec_t<T, 3>& axis, angle_t<T> angle) {
	auto half_angle = angle.as<units::degrees>() / 2;
	auto sin_half_angle = std::sin(half_angle);
	v = axis * sin_half_angle;
	s = std::cos(half_angle);
}

template <typename T>
constexpr gse::quaternion<T>::quaternion(angle_t<T> angle, const unitless_vec_t<T, 3>& axis) : quaternion(axis, angle) {}

template <typename T>
constexpr auto gse::quaternion<T>::operator[](size_t index) -> T& {
	if (index < 3) return v[index];
	return s;
}

template <typename T>
constexpr auto gse::quaternion<T>::operator[](size_t index) const -> T {
	if (index < 3) return v[index];
	return s;
}

template <typename T>
constexpr auto gse::quaternion<T>::operator==(const quaternion& other) const -> bool {
	return v == other.v && s == other.s;
}

template <typename T>
constexpr auto gse::quaternion<T>::operator!=(const quaternion& other) const -> bool {
	return !(*this == other);
}

template <typename T>
constexpr auto gse::quaternion<T>::operator+(const quaternion& other) const -> quaternion {
	return quaternion(v + other.v, s + other.s);
}

template <typename T>
constexpr auto gse::quaternion<T>::operator-(const quaternion& other) const -> quaternion {
	return quaternion(v - other.v, s - other.s);
}

template <typename T>
constexpr auto gse::quaternion<T>::operator*(const quaternion& other) const -> quaternion {
	return quaternion(
		s * other.v + v * other.s + cross(v, other.v),
		s * other.s - dot(v, other.v)
	);
}

template <typename T>
constexpr auto gse::quaternion<T>::operator*(T scalar) const -> quaternion {
	return quaternion(v * scalar, s * scalar);
}

template <typename T>
constexpr auto gse::quaternion<T>::operator/(T scalar) const -> quaternion {
	return quaternion(v / scalar, s / scalar);
}

template <typename T>
constexpr auto gse::quaternion<T>::operator+=(const quaternion& other) -> quaternion& {
	v += other.v;
	s += other.s;
	return *this;
}

template <typename T>
constexpr auto gse::quaternion<T>::operator-=(const quaternion& other) -> quaternion& {
	v -= other.v;
	s -= other.s;
	return *this;
}

template <typename T>
constexpr auto gse::quaternion<T>::operator*=(const quaternion& other) -> quaternion& {
	v = v * other.s + s * other.v + cross(v, other.v);
	s = s * other.s - dot(v, other.v);
	return *this;
}

template <typename T>
constexpr auto gse::quaternion<T>::operator*=(T scalar) -> quaternion& {
	v *= scalar;
	s *= scalar;
	return *this;
}

template <typename T>
constexpr auto gse::quaternion<T>::operator/=(T scalar) -> quaternion& {
	v /= scalar;
	s /= scalar;
	return *this;
}

template <typename T>
constexpr auto gse::quaternion<T>::operator-() const -> quaternion {
	return quaternion(-v, -s);
}
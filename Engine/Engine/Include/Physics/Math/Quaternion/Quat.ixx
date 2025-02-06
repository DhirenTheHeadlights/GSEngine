export module gse.physics.math.quat;

import std;

import gse.physics.math.unitless_vec;
import gse.physics.math.unit_vec;
import gse.physics.math.vec_math;
import gse.physics.math.units;

namespace gse {
	template <typename T>
		requires std::is_arithmetic_v<T>
	struct quaternion {
		union {
			struct {
				T x, y, z, w;
			};
			struct {
				unitless::vec_t<T, 3> v;
				T s;
			};
		};

		constexpr quaternion() : v{ 0, 0, 0 }, s{ 1 } {}
		constexpr quaternion(const unitless::vec_t<T, 3>& v, T s) : v(v), s(s) {}
		constexpr quaternion(T x, T y, T z, T s) : v(x, y, z), s(s) {}
		constexpr quaternion(const unitless::vec_t<T, 3>& axis, angle_t<T> angle);
		constexpr quaternion(angle_t<T> angle, const unitless::vec_t<T, 3>& axis);

		constexpr auto operator[](size_t index)->T&;
		constexpr auto operator[](size_t index) const -> T;
	};

	template <typename T>
		requires std::is_arithmetic_v<T>
	constexpr quaternion<T>::quaternion(const unitless::vec_t<T, 3>& axis, angle_t<T> angle) {
		auto half_angle = angle.template as<units::degrees>() / 2;
		auto sin_half_angle = std::sin(half_angle);
		v = axis * sin_half_angle;
		s = std::cos(half_angle);
	}

	template <typename T> requires std::is_arithmetic_v<T>
	constexpr quaternion<T>::quaternion(angle_t<T> angle, const unitless::vec_t<T, 3>& axis) : quaternion(axis, angle) {}

	template <typename T> requires std::is_arithmetic_v<T>
	constexpr auto quaternion<T>::operator[](size_t index) -> T& {
		if (index < 3) return v[index];
		return s;
	}

	template <typename T> requires std::is_arithmetic_v<T>
	constexpr auto quaternion<T>::operator[](size_t index) const -> T {
		if (index < 3) return v[index];
		return s;
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
	return { lhs.v + rhs.v, lhs.s + rhs.s };
}

template <typename T>
constexpr auto gse::operator-(const quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T> {
	return { lhs.v - rhs.v, lhs.s - rhs.s };
}

template <typename T>
constexpr auto gse::operator*(const quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T> {
	return {
		lhs.s * rhs.v + rhs.s * lhs.v + cross(lhs.v, rhs.v),
		lhs.s * rhs.s - dot(lhs.v, rhs.v)
	};
}

template <typename T>
constexpr auto gse::operator/(const quat_t<T>& lhs, const quat_t<T>& rhs) -> quat_t<T> {
	const auto rhs_conj = quat_t{ -rhs.v, rhs.s };
	const auto norm_sq = dot(rhs, rhs);
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
	return { lhs.v * rhs, lhs.s * rhs };
}

template <typename T>
constexpr auto gse::operator*(const T& lhs, const quat_t<T>& rhs) -> quat_t<T> {
	return { lhs * rhs.v, lhs * rhs.s };
}

template <typename T>
constexpr auto gse::operator/(const quat_t<T>& lhs, const T& rhs) -> quat_t<T> {
	return { lhs.v / rhs, lhs.s / rhs };
}

template <typename T>
constexpr auto gse::operator/(const T& lhs, const quat_t<T>& rhs) -> quat_t<T> {
	return { lhs / rhs.v, lhs / rhs.s };
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

export module gse.physics.math.quat;

import std;

import gse.physics.math.unit_vec;
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
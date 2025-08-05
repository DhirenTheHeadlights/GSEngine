export module gse.physics.math:quat;

import std;

import :unitless_vec;
import :base_vec;
import :vec_math;
import :angle;
import :quant;

namespace gse {
	template <internal::is_arithmetic T>
	struct quaternion {
		union {
			vec::storage<T, 4> v;
			struct {
				T s, x, y, z;
			};
		};

		constexpr quaternion() : v{ 1, 0, 0, 0 } {}
		constexpr quaternion(const unitless::vec_t<T, 4>& vec) : v(vec.data()) {}
		constexpr quaternion(const unitless::vec_t<T, 3>& vec, const T scalar) : v{ scalar, vec.x, vec.y, vec.z } {}
		constexpr quaternion(const T scalar, const unitless::vec_t<T, 3>& vec) : v{ scalar, vec.x, vec.y, vec.z } {}
		constexpr quaternion(T s, T x, T y, T z) : v{ s, x, y, z } {}
		constexpr quaternion(const unitless::vec_t<T, 3>& axis, angle_t<T> angle);
		constexpr quaternion(angle_t<T> angle, const unitless::vec_t<T, 3>& axis);

		constexpr auto operator[](size_t index) -> T&;
		constexpr auto operator[](size_t index) const -> T;

		constexpr auto imaginary_part() const -> unitless::vec_t<T, 3> { return { x, y, z }; }
	};
}

export template <gse::internal::is_arithmetic T, typename CharT>
struct std::formatter<gse::quaternion<T>, CharT> {
	std::formatter<gse::vec::storage<T, 4>, CharT> vec_formatter;

	constexpr auto parse(std::format_parse_context& ctx) {
		return vec_formatter.parse(ctx);
	}

	template <typename FormatContext>
	auto format(const gse::quaternion<T>& q, FormatContext& ctx) const {
		auto out = ctx.out();
		out = std::format_to(out, "quat");
		return vec_formatter.format(q.v, ctx);
	}
};

export namespace gse {
	template <typename T> using quat_t = quaternion<T>;
	using quati = quaternion<int>;
	using quat  = quaternion<float>;
	using quatd = quaternion<double>;
}

template <gse::internal::is_arithmetic T>
constexpr gse::quaternion<T>::quaternion(const unitless::vec_t<T, 3>& axis, angle_t<T> angle) {
	auto half_angle = angle.template as<units::radians>() / T(2);
	auto sin_half_angle = std::sin(half_angle);
	*this = quaternion(axis * sin_half_angle, std::cos(half_angle));
}

template <gse::internal::is_arithmetic T>
constexpr gse::quaternion<T>::quaternion(angle_t<T> angle, const unitless::vec_t<T, 3>& axis)
	: quaternion(axis, angle) {
}

template <gse::internal::is_arithmetic T>
constexpr auto gse::quaternion<T>::operator[](size_t index) -> T& {
	return v.data[index < 4 ? index : 0];
}

template <gse::internal::is_arithmetic T>
constexpr auto gse::quaternion<T>::operator[](size_t index) const -> T {
	return v.data[index < 4 ? index : 0];
}
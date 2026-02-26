export module gse.math:quat;

import std;

import :vector;
import :units;
import :quant;

namespace gse {
	template <internal::is_arithmetic T>
	class quaternion : public unitless::vec_t<T, 4> {
	public:
		using base_type = unitless::vec_t<T, 4>;

		constexpr quaternion();
		constexpr quaternion(const unitless::vec_t<T, 4>& v4);
		constexpr quaternion(const unitless::vec_t<T, 3>& v3, T scalar);
		constexpr quaternion(T scalar, const unitless::vec_t<T, 3>& v3);
		constexpr quaternion(T s, T x, T y, T z);
		constexpr quaternion(const unitless::vec_t<T, 3>& axis, angle_t<T> angle);
		constexpr quaternion(angle_t<T> angle, const unitless::vec_t<T, 3>& axis);

		constexpr decltype(auto) operator[](this auto&& self, std::size_t index);

		constexpr decltype(auto) s(this auto& self);
		constexpr decltype(auto) x(this auto& self);
		constexpr decltype(auto) y(this auto& self);
		constexpr decltype(auto) z(this auto& self);

		constexpr auto imaginary_part() const -> unitless::vec_t<T, 3>;
		constexpr auto v4(this auto&& self) -> unitless::vec_t<T, 4>;
		constexpr auto euler_angles() const -> vec3<angle_t<T>>;
	private:
		using base_type::w;
		using base_type::r;
		using base_type::g;
		using base_type::b;
		using base_type::a;
	};

	export template <typename T> using quat_t = quaternion<T>;
	export using quati = quaternion<int>;
	export using quat = quaternion<float>;
	export using quatd = quaternion<double>;
}

export template <gse::internal::is_arithmetic T, typename CharT>
struct std::formatter<gse::quaternion<T>, CharT> {
    std::formatter<gse::unitless::vec_t<T, 4>, CharT> vec_formatter;

    constexpr auto parse(std::format_parse_context& ctx) {
        return vec_formatter.parse(ctx);
    }

    template <typename FormatContext>
    auto format(const gse::quaternion<T>& q, FormatContext& ctx) const {
        auto out = ctx.out();
        out = std::format_to(out, "quat");
        const auto& v4 = static_cast<const gse::unitless::vec_t<T,4>&>(q);
        return vec_formatter.format(v4, ctx);
    }
};


template <gse::internal::is_arithmetic T>
constexpr gse::quaternion<T>::quaternion() : unitless::vec_t<T, 4>{ T(1), T(0), T(0), T(0) } {}

template <gse::internal::is_arithmetic T>
constexpr gse::quaternion<T>::quaternion(const unitless::vec_t<T, 4>& v4) : unitless::vec_t<T, 4>{ v4.at(0), v4.at(1), v4.at(2), v4.at(3) } {}

template <gse::internal::is_arithmetic T>
constexpr gse::quaternion<T>::quaternion(const unitless::vec_t<T, 3>& v3, const T scalar) : unitless::vec_t<T, 4>{ scalar, v3.at(0), v3.at(1), v3.at(2) } {}

template <gse::internal::is_arithmetic T>
constexpr gse::quaternion<T>::quaternion(const T scalar, const unitless::vec_t<T, 3>& v3) : unitless::vec_t<T, 4>{ scalar, v3.at(0), v3.at(1), v3.at(2) } {}

template <gse::internal::is_arithmetic T>
constexpr gse::quaternion<T>::quaternion(T s, T x, T y, T z) : unitless::vec_t<T, 4>{ s, x, y, z } {}

template <gse::internal::is_arithmetic T>
constexpr gse::quaternion<T>::quaternion(const unitless::vec_t<T, 3>& axis, angle_t<T> angle) {
	auto half_angle = angle.template as<radians>() / T(2);
	auto sin_half_angle = std::sin(half_angle);
	*this = quaternion(axis * sin_half_angle, std::cos(half_angle));
}

template <gse::internal::is_arithmetic T>
constexpr gse::quaternion<T>::quaternion(angle_t<T> angle, const unitless::vec_t<T, 3>& axis) : quaternion(axis, angle) {}

template <gse::internal::is_arithmetic T>
constexpr decltype(auto) gse::quaternion<T>::operator[](this auto&& self, std::size_t index) {
	return self.at(index);
}

template <gse::internal::is_arithmetic T>
constexpr decltype(auto) gse::quaternion<T>::s(this auto& self) {
	return self.at(0);
}

template <gse::internal::is_arithmetic T>
constexpr decltype(auto) gse::quaternion<T>::x(this auto& self) {
	return self.at(1);
}

template <gse::internal::is_arithmetic T>
constexpr decltype(auto) gse::quaternion<T>::y(this auto& self) {
	return self.at(2);
}

template <gse::internal::is_arithmetic T>
constexpr decltype(auto) gse::quaternion<T>::z(this auto& self) {
	return self.at(3);
}

template <gse::internal::is_arithmetic T>
constexpr auto gse::quaternion<T>::imaginary_part() const -> unitless::vec_t<T, 3> {
	return { this->x(), this->y(), this->z() };
}

template <gse::internal::is_arithmetic T>
constexpr auto gse::quaternion<T>::v4(this auto&& self) -> unitless::vec_t<T, 4> {
	return static_cast<unitless::vec_t<T, 4>>(self);
}

template <gse::internal::is_arithmetic T>
constexpr auto gse::quaternion<T>::euler_angles() const -> vec3<angle_t<T>> {
	angle_t<T> angle_x;
	angle_t<T> angle_y;
	angle_t<T> angle_z;

	const T sinp = T(2) * (s() * y() - z() * x());
	if (std::abs(sinp) >= 1) {
		angle_y = radians(std::copysign(std::numbers::pi_v<T> / T(2), sinp)); 
		angle_x = radians(T(2) * std::atan2(x(), s()));               
		angle_z = radians(T(0));                                     
	}
	else {
		angle_y = radians(std::asin(sinp));
		angle_x = radians(std::atan2(T(2) * (s() * x() + y() * z()), T(1) - T(2) * (x() * x() + y() * y())));
		angle_z = radians(std::atan2(T(2) * (s() * z() + x() * y()), T(1) - T(2) * (y() * y() + z() * z())));
	}

	return { angle_x, angle_y, angle_z };
}

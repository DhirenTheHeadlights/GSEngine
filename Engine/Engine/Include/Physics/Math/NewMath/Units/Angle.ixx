export module gse.physics.math.units.ang;

import std;

import gse.physics.math.units.quant;
import gse.physics.math.unit_vec;

namespace gse::units {
	struct angle_tag {};

	constexpr char degrees_units[] = "deg";
	constexpr char radians_units[] = "rad";

	export using degrees = internal::unit<float, 1.0f, degrees_units>;
	export using radians = internal::unit<float, 1 / 0.01745329252f, radians_units>;

	using angle_units = internal::unit_list<
		degrees,
		radians
	>;
}

export namespace gse {
	template <typename T = float> using angle_t = internal::quantity<T, units::angle_tag, units::degrees, units::angle_units>;

	using angle = angle_t<>;

	template <typename T> constexpr auto degrees_t(T value) -> angle_t<T>;
	template <typename T> constexpr auto radians_t(T value) -> angle_t<T>;

	constexpr auto degrees(float value) -> angle;
	constexpr auto radians(float value) -> angle;
}

export namespace gse::vec {
	template <typename... Args>
	constexpr auto angle(Args... args) -> vec_t<angle_t<std::common_type_t<Args...>>, sizeof...(Args)>;

	template <typename... Args> constexpr auto degrees(Args... args) -> vec_t<angle_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto radians(Args... args) -> vec_t<angle_t<std::common_type_t<Args...>>, sizeof...(Args)>;
}
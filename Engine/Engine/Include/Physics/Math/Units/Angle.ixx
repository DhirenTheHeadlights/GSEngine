export module gse.physics.math.units.angle;

import std;

import gse.physics.math.units.quant;
import gse.physics.math.unit_vec;

namespace gse::units {

	constexpr char degrees_units[] = "deg";
	constexpr char radians_units[] = "rad";

	export using degrees = internal::unit<angle_tag, 1.0f, degrees_units>;
	export using radians = internal::unit<angle_tag, 1 / 0.01745329252f, radians_units>;

	using angle_units = internal::unit_list<
		degrees,
		radians
	>;
}

export namespace gse {
	template <typename T = float>
	struct angle_t : internal::quantity<angle_t<T>, T, units::dimensions::dim<0 ,0, 0>, units::angle_tag, units::degrees, units::angle_units> {
		using internal::quantity<angle_t, T, units::dimensions::dim<0, 0, 0>, units::angle_tag, units::degrees, units::angle_units>::quantity;
	};

	using angle = angle_t<>;

	template <typename T> constexpr auto degrees_t(T value) -> angle_t<T>;
	template <typename T> constexpr auto radians_t(T value) -> angle_t<T>;

	constexpr auto degrees(float value) -> angle;
	constexpr auto radians(float value) -> angle;
}

export namespace gse::vec {
	template <internal::is_unit U, typename... Args>
	constexpr auto angle(Args... args) -> vec_t<angle_t<std::common_type_t<Args...>>, sizeof...(Args)>;

	template <typename... Args> constexpr auto degrees(Args... args) -> vec_t<angle_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto radians(Args... args) -> vec_t<angle_t<std::common_type_t<Args...>>, sizeof...(Args)>;
}

template <typename T>
constexpr auto gse::degrees_t(const T value) -> angle_t<T> {
	return angle_t<T>::template from<units::degrees>(value);
}

template <typename T>
constexpr auto gse::radians_t(const T value) -> angle_t<T> {
	return angle_t<T>::template from<units::radians>(value);
}

constexpr auto gse::degrees(const float value) -> angle {
	return degrees_t<float>(value);
}

constexpr auto gse::radians(const float value) -> angle {
	return radians_t<float>(value);
}

template <gse::internal::is_unit U, typename... Args>
constexpr auto gse::vec::angle(Args... args) -> vec_t<angle_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { angle_t<std::common_type_t<Args...>>::template from<U>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::degrees(Args... args) -> vec_t<angle_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { angle_t<std::common_type_t<Args...>>::template from<units::degrees>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::radians(Args... args) -> vec_t<angle_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { angle_t<std::common_type_t<Args...>>::template from<units::radians>(args)... };
}
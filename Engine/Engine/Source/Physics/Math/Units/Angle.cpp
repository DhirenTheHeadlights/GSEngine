module gse.physics.math.units.ang;

import std;

import gse.physics.math.units.quant;
import gse.physics.math.unit_vec;

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

template <typename... Args>
constexpr auto gse::vec::angle(Args... args) -> vec_t<angle_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return vec_t<angle_t<std::common_type_t<Args...>>, sizeof...(Args)>(args...);
}

template <typename... Args>
constexpr auto gse::vec::degrees(Args... args) -> vec_t<angle_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return vec_t<angle_t<std::common_type_t<Args...>>, sizeof...(Args)>(args...);
}

template <typename... Args>
constexpr auto gse::vec::radians(Args... args) -> vec_t<angle_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return vec_t<angle_t<std::common_type_t<Args...>>, sizeof...(Args)>(args...);
}
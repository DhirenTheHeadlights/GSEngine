module gse.physics.math.units.len;

import gse.physics.math.units.quantity;
import gse.physics.math.unit_vec;

template <typename T>
constexpr auto gse::kilometers_t(const T value) -> length_t<T> {
	return length_t<T>::template from<units::kilometers>(value);
}

template <typename T>
constexpr auto gse::meters_t(const T value) -> length_t<T> {
	return length_t<T>::template from<units::meters>(value);
}

template <typename T>
constexpr auto gse::centimeters_t(const T value) -> length_t<T> {
	return length_t<T>::template from<units::centimeters>(value);
}

template <typename T>
constexpr auto gse::millimeters_t(const T value) -> length_t<T> {
	return length_t<T>::template from<units::millimeters>(value);
}

template <typename T>
constexpr auto gse::yards_t(const T value) -> length_t<T> {
	return length_t<T>::template from<units::yards>(value);
}

template <typename T>
constexpr auto gse::feet_t(const T value) -> length_t<T> {
	return length_t<T>::template from<units::feet>(value);
}

template <typename T>
constexpr auto gse::inches_t(const T value) -> length_t<T> {
	return length_t<T>::template from<units::inches>(value);
}

constexpr auto gse::kilometers(const float value) -> length {
	return kilometers_t<float>(value);
}

constexpr auto gse::meters(const float value) -> length {
	return meters_t<float>(value);
}

constexpr auto gse::centimeters(const float value) -> length {
	return centimeters_t<float>(value);
}

constexpr auto gse::millimeters(const float value) -> length {
	return millimeters_t<float>(value);
}

constexpr auto gse::yards(const float value) -> length {
	return yards_t<float>(value);
}

constexpr auto gse::feet(const float value) -> length {
	return feet_t<float>(value);
}

constexpr auto gse::inches(const float value) -> length {
	return inches_t<float>(value);
}

template <gse::internal::is_unit U, typename ... Args>
constexpr auto gse::vec::length(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { length_t<std::common_type_t<Args...>>::template from<U>(args)... };
}

template <typename ... Args>
constexpr auto gse::vec::centimeters(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return length<units::centimeters>(args...);
}

template <typename ... Args>
constexpr auto gse::vec::meters(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return length<units::meters>(args...);
}

template <typename ... Args>
constexpr auto gse::vec::millimeters(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return length<units::millimeters>(args...);
}

template <typename ... Args>
constexpr auto gse::vec::kilometers(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return length<units::kilometers>(args...);
}

template <typename ... Args>
constexpr auto gse::vec::yards(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return length<units::yards>(args...);
}

template <typename ... Args>
constexpr auto gse::vec::feet(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return length<units::feet>(args...);
}

template <typename ... Args>
constexpr auto gse::vec::inches(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return length<units::inches>(args...);
}
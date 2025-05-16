export module gse.physics.math.units.length;

import std;

import gse.physics.math.units.quant;
import gse.physics.math.unit_vec;

namespace gse::units {

    inline constexpr char kilometers_units[] = "km";
    inline constexpr char meters_units[] = "m";
    inline constexpr char centimeters_units[] = "cm";
    inline constexpr char millimeters_units[] = "mm";
    inline constexpr char yards_units[] = "yd";
    inline constexpr char feet_units[] = "ft";
    inline constexpr char inches_units[] = "in";

    export using kilometers = internal::unit<length_tag, 1000.0f, kilometers_units>;
    export using meters = internal::unit<length_tag, 1.0f, meters_units>;
    export using centimeters = internal::unit<length_tag, 0.01f, centimeters_units>;
    export using millimeters = internal::unit<length_tag, 0.001f, millimeters_units>;
    export using yards = internal::unit<length_tag, 0.9144f, yards_units>;
    export using feet = internal::unit<length_tag, 0.3048f, feet_units>;
    export using inches = internal::unit<length_tag, 0.0254f, inches_units>;

    using length_units = internal::unit_list <
        kilometers,
        meters,
        centimeters,
        millimeters,
        yards,
        feet,
        inches
    >;
}

export namespace gse {
	template <typename T = float>
	struct length_t : internal::quantity<length_t<T>, T, units::dimensions::dim<1, 0, 0>, units::length_tag, units::meters, units::length_units> {
		using internal::quantity<length_t, T, units::dimensions::dim<1, 0, 0>, units::length_tag, units::meters, units::length_units>::quantity;
	};

    using length = length_t<>;

    template <typename T> constexpr auto kilometers_t(T value) -> length_t<T>;
    template <typename T> constexpr auto meters_t(T value) -> length_t<T>;
    template <typename T> constexpr auto centimeters_t(T value) -> length_t<T>;
    template <typename T> constexpr auto millimeters_t(T value) -> length_t<T>;
    template <typename T> constexpr auto yards_t(T value) -> length_t<T>;
    template <typename T> constexpr auto feet_t(T value) -> length_t<T>;
    template <typename T> constexpr auto inches_t(T value) -> length_t<T>;

    constexpr auto kilometers(float value) -> length;
    constexpr auto meters(float value) -> length;
    constexpr auto centimeters(float value) -> length;
    constexpr auto millimeters(float value) -> length;
    constexpr auto yards(float value) -> length;
    constexpr auto feet(float value) -> length;
    constexpr auto inches(float value) -> length;
}

export namespace gse::vec {
    template <internal::is_unit U, typename... Args>
    constexpr auto length(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)>;

    template <typename... Args> constexpr auto centimeters(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto meters(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto millimeters(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto kilometers(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto yards(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto feet(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto inches(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)>;
}

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

template <typename... Args>
constexpr auto gse::vec::centimeters(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { length_t<std::common_type_t<Args...>>::template from<units::centimeters>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::meters(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { length_t<std::common_type_t<Args...>>::template from<units::meters>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::millimeters(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { length_t<std::common_type_t<Args...>>::template from<units::millimeters>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::kilometers(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { length_t<std::common_type_t<Args...>>::template from<units::kilometers>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::yards(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { length_t<std::common_type_t<Args...>>::template from<units::yards>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::feet(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { length_t<std::common_type_t<Args...>>::template from<units::feet>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::inches(Args&&... args) -> vec_t<length_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { length_t<std::common_type_t<Args...>>::template from<units::inches>(args)... };
}
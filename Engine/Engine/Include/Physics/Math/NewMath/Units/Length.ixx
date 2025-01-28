export module gse.physics.math.units.len;

import std;

import gse.physics.math.units.quant;
import gse.physics.math.unit_vec;

namespace gse::units {
    struct length_tag {};

    constexpr char kilometers_units[] = "km";
    constexpr char meters_units[] = "m";
    constexpr char centimeters_units[] = "cm";
    constexpr char millimeters_units[] = "mm";
    constexpr char yards_units[] = "yd";
    constexpr char feet_units[] = "ft";
    constexpr char inches_units[] = "in";

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
    template <typename T = float> using length_t = internal::quantity<T, units::length_tag, units::meters, units::length_units>;

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
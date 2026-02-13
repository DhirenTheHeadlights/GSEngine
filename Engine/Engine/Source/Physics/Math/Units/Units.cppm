export module gse.physics.math:units;

import std;
import :dimension;
import :quant;

export namespace gse {
    using pi_approx = std::ratio<355, 113>;

    struct angle_tag {};

    constexpr internal::unit<angle_tag, std::ratio<1>, "rad"> radians;
    constexpr internal::unit<angle_tag, std::ratio_divide<pi_approx, std::ratio<180>>, "deg"> degrees;

    template <typename T = float, auto U = radians>
    using angle_t = internal::quantity<T, internal::dim<0, 0, 0>, angle_tag, decltype(U)>;
    using angle = angle_t<>;

    template <>
    struct internal::quantity_traits<angle_tag> {
        template <typename T, auto U = radians>
        using type = angle_t<T, U>;
    };
}

export namespace gse {
    struct length_tag {};

    constexpr internal::unit<length_tag, std::kilo, "km"> kilometers;
    constexpr internal::unit<length_tag, std::ratio<1>, "m"> meters;
    constexpr internal::unit<length_tag, std::centi, "cm"> centimeters;
    constexpr internal::unit<length_tag, std::milli, "mm"> millimeters;
    constexpr internal::unit<length_tag, std::ratio<1143, 1250>, "yd"> yards;
    constexpr internal::unit<length_tag, std::ratio<381, 1250>, "ft"> feet;
    constexpr internal::unit<length_tag, std::ratio<127, 5000>, "in"> inches;

    template <typename T = float, auto U = meters>
    using length_t = internal::quantity<T, internal::dim<1, 0, 0>, length_tag, decltype(U)>;
    using length = length_t<>;

    template <>
    struct internal::quantity_traits<length_tag> {
        template <typename T, auto U = meters>
        using type = length_t<T, U>;
    };
}

export namespace gse {
    struct time_tag {};

    constexpr internal::unit<time_tag, std::ratio<1>, "ns"> nanoseconds;
    constexpr internal::unit<time_tag, std::kilo, "us"> microseconds;
    constexpr internal::unit<time_tag, std::mega, "ms"> milliseconds;
    constexpr internal::unit<time_tag, std::giga, "s"> seconds;
    constexpr internal::unit<time_tag, std::ratio<60'000'000'000>, "min"> minutes;
    constexpr internal::unit<time_tag, std::ratio<3'600'000'000'000>, "hr"> hours;

    template <typename T = float, auto U = nanoseconds>
    using time_t = internal::quantity<T, internal::dim<0, 1, 0>, time_tag, decltype(U)>;
    using time = time_t<>;

    template <>
    struct internal::quantity_traits<time_tag> {
        template <typename T, auto U = nanoseconds>
        using type = time_t<T, U>;
    };
}

export namespace gse {
    struct mass_tag {};

    constexpr internal::unit<mass_tag, std::ratio<1>, "kg"> kilograms;
    constexpr internal::unit<mass_tag, std::milli, "g"> grams;
    constexpr internal::unit<mass_tag, std::micro, "mg"> milligrams;
    constexpr internal::unit<mass_tag, std::ratio<45359237, 100000000>, "lb"> pounds;
    constexpr internal::unit<mass_tag, std::ratio<45359237, 1600000000>, "oz"> ounces;

    template <typename T = float, auto U = kilograms>
    using mass_t = internal::quantity<T, internal::dim<0, 0, 1>, mass_tag, decltype(U)>;
    using mass = mass_t<>;

    template <>
    struct internal::quantity_traits<mass_tag> {
        template <typename T, auto U = kilograms>
        using type = mass_t<T, U>;
    };
}

export namespace gse {
    struct inverse_mass_tag {};

    constexpr internal::unit<inverse_mass_tag, std::ratio<1>, "1/kg"> per_kilograms;

    template <typename T = float, auto U = per_kilograms>
    using inverse_mass_t = internal::quantity<T, internal::dim<0, 0, -1>, inverse_mass_tag, decltype(U)>;
    using inverse_mass = inverse_mass_t<>;

    template <>
    struct internal::quantity_traits<inverse_mass_tag> {
        template <typename T, auto U = per_kilograms>
        using type = inverse_mass_t<T, U>;
    };
}

export namespace gse {
    struct force_tag {};

    constexpr internal::unit<force_tag, std::ratio<1>, "N"> newtons;
    constexpr internal::unit<force_tag, std::ratio<222411, 50000>, "lbf"> pounds_force;

    template <typename T = float, auto U = newtons>
    using force_t = internal::quantity<T, internal::dim<1, -2, 1>, force_tag, decltype(U)>;
    using force = force_t<>;

    template <>
    struct internal::quantity_traits<force_tag> {
        template <typename T, auto U = newtons>
        using type = force_t<T, U>;
    };
}

export namespace gse {
    struct energy_tag {};

    constexpr internal::unit<energy_tag, std::giga, "GJ"> gigajoules;
    constexpr internal::unit<energy_tag, std::mega, "MJ"> megajoules;
    constexpr internal::unit<energy_tag, std::kilo, "kJ"> kilojoules;
    constexpr internal::unit<energy_tag, std::ratio<1>, "J"> joules;
    constexpr internal::unit<energy_tag, std::ratio<4184>, "kcal"> kilocalories;
    constexpr internal::unit<energy_tag, std::ratio<523, 125>, "cal"> calories;

    template <typename T = float, auto U = joules>
    using energy_t = internal::quantity<T, internal::dim<2, -2, 1>, energy_tag, decltype(U)>;
    using energy = energy_t<>;

    template <>
    struct internal::quantity_traits<energy_tag> {
        template <typename T, auto U = joules>
        using type = energy_t<T, U>;
    };
}

export namespace gse {
    struct torque_tag {};

    constexpr internal::unit<torque_tag, std::ratio<1>, "N-m"> newton_meters;
    constexpr internal::unit<torque_tag, std::ratio<67791, 50000>, "lbf-ft"> pound_feet;

    template <typename T = float, auto U = newton_meters>
    using torque_t = internal::quantity<T, internal::dim<2, -2, 1>, torque_tag, decltype(U)>;
    using torque = torque_t<>;

    template <>
    struct internal::quantity_traits<torque_tag> {
        template <typename T, auto U = newton_meters>
        using type = torque_t<T, U>;
    };
}

export namespace gse {
    struct power_tag {};

    constexpr internal::unit<power_tag, std::giga, "GW"> gigawatts;
    constexpr internal::unit<power_tag, std::mega, "MW"> megawatts;
    constexpr internal::unit<power_tag, std::kilo, "kW"> kilowatts;
    constexpr internal::unit<power_tag, std::ratio<1>, "W"> watts;
    constexpr internal::unit<power_tag, std::ratio<7457, 10>, "hp"> horsepower;

    template <typename T = float, auto U = watts>
    using power_t = internal::quantity<T, internal::dim<2, -3, 1>, power_tag, decltype(U)>;
    using power = power_t<>;

    template <>
    struct internal::quantity_traits<power_tag> {
        template <typename T, auto U = watts>
        using type = power_t<T, U>;
    };
}

export namespace gse {
    struct inertia_tag {};

    constexpr internal::unit<inertia_tag, std::ratio<1>, "kg-m^2"> kilograms_meters_squared;
    constexpr internal::unit<inertia_tag, std::ratio<4214011, 100000000>, "lb-ft^2"> pounds_feet_squared;

    template <typename T = float, auto U = kilograms_meters_squared>
    using inertia_t = internal::quantity<T, internal::dim<2, 0, 1>, inertia_tag, decltype(U)>;
    using inertia = inertia_t<>;

    template <>
    struct internal::quantity_traits<inertia_tag> {
        template <typename T, auto U = kilograms_meters_squared>
        using type = inertia_t<T, U>;
    };
}

export namespace gse {
    struct velocity_tag {};

    constexpr internal::unit<velocity_tag, std::ratio<1>, "m/s"> meters_per_second;
    constexpr internal::unit<velocity_tag, std::ratio<5, 18>, "km/h"> kilometers_per_hour;
    constexpr internal::unit<velocity_tag, std::ratio<1397, 3125>, "mph"> miles_per_hour;

    template <typename T = float, auto U = meters_per_second>
    using velocity_t = internal::quantity<T, internal::dim<1, -1, 0>, velocity_tag, decltype(U)>;
    using velocity = velocity_t<>;

    template <>
    struct internal::quantity_traits<velocity_tag> {
        template <typename T, auto U = meters_per_second>
        using type = velocity_t<T, U>;
    };
}

export namespace gse {
    struct acceleration_tag {};

    constexpr internal::unit<acceleration_tag, std::ratio<1>, "m/s^2"> meters_per_second_squared;

    template <typename T = float, auto U = meters_per_second_squared>
    using acceleration_t = internal::quantity<T, internal::dim<1, -2, 0>, acceleration_tag, decltype(U)>;
    using acceleration = acceleration_t<>;

    template <>
    struct internal::quantity_traits<acceleration_tag> {
        template <typename T, auto U = meters_per_second_squared>
        using type = acceleration_t<T, U>;
    };
}

export namespace gse {
    struct angular_velocity_tag {};

    constexpr internal::unit<angular_velocity_tag, std::ratio<1>, "rad/s"> radians_per_second;
    constexpr internal::unit<angular_velocity_tag, std::ratio_divide<pi_approx, std::ratio<180>>, "deg/s"> degrees_per_second;

    template <typename T = float, auto U = radians_per_second>
    using angular_velocity_t = internal::quantity<T, internal::dim<0, -1, 0>, angular_velocity_tag, decltype(U)>;
    using angular_velocity = angular_velocity_t<>;

    template <>
    struct internal::quantity_traits<angular_velocity_tag> {
        template <typename T, auto U = radians_per_second>
        using type = angular_velocity_t<T, U>;
    };
}

export namespace gse {
    struct angular_acceleration_tag {};

    constexpr internal::unit<angular_acceleration_tag, std::ratio<1>, "rad/s^2"> radians_per_second_squared;
    constexpr internal::unit<angular_acceleration_tag, std::ratio_divide<pi_approx, std::ratio<180>>, "deg/s^2"> degrees_per_second_squared;

    template <typename T = float, auto U = radians_per_second_squared>
    using angular_acceleration_t = internal::quantity<T, internal::dim<0, -2, 0>, angular_acceleration_tag, decltype(U)>;
    using angular_acceleration = angular_acceleration_t<>;

    template <>
    struct internal::quantity_traits<angular_acceleration_tag> {
        template <typename T, auto U = radians_per_second_squared>
        using type = angular_acceleration_t<T, U>;
    };
}

export namespace gse {
    struct density_tag {};

    constexpr internal::unit<density_tag, std::ratio<1>, "kg/m^3"> kilograms_per_cubic_meter;
    constexpr internal::unit<density_tag, std::ratio<625, 2266>, "lb/ft^3"> pounds_per_cubic_foot;

    template <typename T = float, auto U = kilograms_per_cubic_meter>
    using density_t = internal::quantity<T, internal::dim<-3, 0, 1>, density_tag, decltype(U)>;
    using density = density_t<>;

    template <>
    struct internal::quantity_traits<density_tag> {
        template <typename T, auto U = kilograms_per_cubic_meter>
        using type = density_t<T, U>;
    };
}

export namespace gse {
    struct area_tag {};

    constexpr internal::unit<area_tag, std::ratio<1>, "m^2"> square_meters;
    constexpr internal::unit<area_tag, std::ratio<1027639, 10000000>, "ft^2"> square_feet;

    template <typename T = float, auto U = square_meters>
    using area_t = internal::quantity<T, internal::dim<2, 0, 0>, area_tag, decltype(U)>;
    using area = area_t<>;

    template <>
    struct internal::quantity_traits<area_tag> {
        template <typename T, auto U = square_meters>
        using type = area_t<T, U>;
    };
}
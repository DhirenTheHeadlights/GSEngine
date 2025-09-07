export module gse.physics.math:units;

import std;

import :dimension;
import :quant;

export namespace gse {
    using pi_approx = std::ratio<355, 113>;

    struct angle_tag {};

    constexpr internal::unit<angle_tag, std::ratio<1>, "rad"> radians;
    constexpr internal::unit<angle_tag, std::ratio_divide<pi_approx, std::ratio<180>>, "deg"> degrees;

    template <>
    struct internal::dimension_traits<internal::dim<0, 0, 0>> {
        using tag = angle_tag;
        using default_unit = decltype(radians);
    };

    template <typename T = float>
    using angle_t = internal::quantity<T, internal::dim<0, 0, 0>>;
    using angle = angle_t<>;

    template <>
    struct internal::quantity_traits<angle_tag> {
        template <typename T>
        using type = angle_t<T>;
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

    template <>
    struct internal::dimension_traits<internal::dim<1, 0, 0>> {
        using tag = length_tag;
        using default_unit = decltype(meters);
    };

    template <typename T = float>
    using length_t = internal::quantity<T, internal::dim<1, 0, 0>>;
    using length = length_t<>;

    template <>
    struct internal::quantity_traits<length_tag> {
        template <typename T>
        using type = length_t<T>;
    };
}

export namespace gse {
    struct time_tag {};

    constexpr internal::unit<time_tag, std::ratio<3600>, "hr"> hours;
    constexpr internal::unit<time_tag, std::ratio<60>, "min"> minutes;
    constexpr internal::unit<time_tag, std::ratio<1>, "s"> seconds;
    constexpr internal::unit<time_tag, std::milli, "ms"> milliseconds;
    constexpr internal::unit<time_tag, std::micro, "us"> microseconds;
    constexpr internal::unit<time_tag, std::nano, "ns"> nanoseconds;

    template <>
    struct internal::dimension_traits<internal::dim<0, 1, 0>> {
        using tag = time_tag;
        using default_unit = decltype(seconds);
    };

    template <typename T = float>
    using time_t = internal::quantity<T, internal::dim<0, 1, 0>>;
    using time = time_t<>;

    template <>
    struct internal::quantity_traits<time_tag> {
        template <typename T>
        using type = time_t<T>;
    };
}

export namespace gse {
    struct mass_tag {};

    constexpr internal::unit<mass_tag, std::ratio<1>, "kg"> kilograms;
    constexpr internal::unit<mass_tag, std::milli, "g"> grams;
    constexpr internal::unit<mass_tag, std::micro, "mg"> milligrams;
    constexpr internal::unit<mass_tag, std::ratio<45359237, 100000000>, "lb"> pounds;
    constexpr internal::unit<mass_tag, std::ratio<45359237, 1600000000>, "oz"> ounces;

    template <>
    struct internal::dimension_traits<internal::dim<0, 0, 1>> {
        using tag = mass_tag;
        using default_unit = decltype(kilograms);
    };

    template <typename T = float>
    using mass_t = internal::quantity<T, internal::dim<0, 0, 1>>;
    using mass = mass_t<>;

    template <>
    struct internal::quantity_traits<mass_tag> {
        template <typename T>
        using type = mass_t<T>;
    };
}

export namespace gse {
	struct inverse_mass_tag {};

	constexpr internal::unit<inverse_mass_tag, std::ratio<1>, "1/kg"> per_kilograms;

    template <>
    struct internal::dimension_traits<internal::dim<0, 0, -1>> {
        using tag = inverse_mass_tag;
        using default_unit = decltype(per_kilograms);
	};

    template <typename T = float>
	using inverse_mass_t = internal::quantity<T, internal::dim<0, 0, -1>>;
	using inverse_mass = inverse_mass_t<>;

    template <>
    struct internal::quantity_traits<inverse_mass_tag> {
        template <typename T>
        using type = inverse_mass_t<T>;
	};
}

export namespace gse {
    struct force_tag {};

    constexpr internal::unit<force_tag, std::ratio<1>, "N"> newtons;
    constexpr internal::unit<force_tag, std::ratio<222411, 50000>, "lbf"> pounds_force;

    template <>
    struct internal::dimension_traits<internal::dim<1, -2, 1>> {
        using tag = force_tag;
        using default_unit = decltype(newtons);
    };

    template <typename T = float>
    using force_t = internal::quantity<T, internal::dim<1, -2, 1>>;
    using force = force_t<>;

    template <>
    struct internal::quantity_traits<force_tag> {
        template <typename T>
        using type = force_t<T>;
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

    template <>
    struct internal::dimension_traits<internal::dim<2, -2, 1>> {
        using tag = energy_tag;
        using default_unit = decltype(joules);
    };

    template <typename T = float>
    using energy_t = internal::quantity<T, internal::dim<2, -2, 1>>;
    using energy = energy_t<>;

    template <>
    struct internal::quantity_traits<energy_tag> {
        template <typename T>
        using type = energy_t<T>;
    };
}

export namespace gse {
    struct torque_tag {};

    constexpr internal::unit<torque_tag, std::ratio<1>, "N-m"> newton_meters;
    constexpr internal::unit<torque_tag, std::ratio<67791, 50000>, "lbf-ft"> pound_feet;

    template <typename T = float>
    using torque_t = internal::quantity<T, internal::dim<2, -2, 1>>;
    using torque = torque_t<>;

    template <>
    struct internal::quantity_traits<torque_tag> {
        template <typename T>
        using type = torque_t<T>;
    };
}

export namespace gse {
    struct power_tag {};

    constexpr internal::unit<power_tag, std::giga, "GW"> gigawatts;
    constexpr internal::unit<power_tag, std::mega, "MW"> megawatts;
    constexpr internal::unit<power_tag, std::kilo, "kW"> kilowatts;
    constexpr internal::unit<power_tag, std::ratio<1>, "W"> watts;
    constexpr internal::unit<power_tag, std::ratio<7457, 10>, "hp"> horsepower;

    template <>
    struct internal::dimension_traits<internal::dim<2, -3, 1>> {
        using tag = power_tag;
        using default_unit = decltype(watts);
    };

    template <typename T = float>
    using power_t = internal::quantity<T, internal::dim<2, -3, 1>>;
    using power = power_t<>;

    template <>
    struct internal::quantity_traits<power_tag> {
        template <typename T>
        using type = power_t<T>;
    };
}

export namespace gse {
    struct inertia_tag {};

    constexpr internal::unit<inertia_tag, std::ratio<1>, "kg-m^2"> kilograms_meters_squared;
    constexpr internal::unit<inertia_tag, std::ratio<4214011, 100000000>, "lb-ft^2"> pounds_feet_squared;

    template <>
    struct internal::dimension_traits<internal::dim<2, 0, 1>> {
        using tag = inertia_tag;
        using default_unit = decltype(kilograms_meters_squared);
    };

    template <typename T = float>
    using inertia_t = internal::quantity<T, internal::dim<2, 0, 1>>;
    using inertia = inertia_t<>;

    template <>
    struct internal::quantity_traits<inertia_tag> {
        template <typename T>
        using type = inertia_t<T>;
    };
}

export namespace gse {
    struct velocity_tag {};

    constexpr internal::unit<velocity_tag, std::ratio<1>, "m/s"> meters_per_second;
    constexpr internal::unit<velocity_tag, std::ratio<5, 18>, "km/h"> kilometers_per_hour;
    constexpr internal::unit<velocity_tag, std::ratio<1397, 3125>, "mph"> miles_per_hour;

    template <>
    struct internal::dimension_traits<internal::dim<1, -1, 0>> {
        using tag = velocity_tag;
        using default_unit = decltype(meters_per_second);
    };

    template <typename T = float>
    using velocity_t = internal::quantity<T, internal::dim<1, -1, 0>>;
    using velocity = velocity_t<>;

    template <>
    struct internal::quantity_traits<velocity_tag> {
        template <typename T>
        using type = velocity_t<T>;
    };
}

export namespace gse {
    struct acceleration_tag {};

    constexpr internal::unit<acceleration_tag, std::ratio<1>, "m/s^2"> meters_per_second_squared;

    template <>
    struct internal::dimension_traits<internal::dim<1, -2, 0>> {
        using tag = acceleration_tag;
        using default_unit = decltype(meters_per_second_squared);
    };

    template <typename T = float>
    using acceleration_t = internal::quantity<T, internal::dim<1, -2, 0>>;
    using acceleration = acceleration_t<>;

    template <>
    struct internal::quantity_traits<acceleration_tag> {
        template <typename T>
        using type = acceleration_t<T>;
    };
}

export namespace gse {
    struct angular_velocity_tag {};

    constexpr internal::unit<angular_velocity_tag, std::ratio<1>, "rad/s"> radians_per_second;
    constexpr internal::unit<angular_velocity_tag, std::ratio_divide<pi_approx, std::ratio<180>>, "deg/s"> degrees_per_second;

    template <>
    struct internal::dimension_traits<internal::dim<0, -1, 0>> {
        using tag = angular_velocity_tag;
        using default_unit = decltype(radians_per_second);
    };

    template <typename T = float>
    using angular_velocity_t = internal::quantity<T, internal::dim<0, -1, 0>>;
    using angular_velocity = angular_velocity_t<>;

    template <>
    struct internal::quantity_traits<angular_velocity_tag> {
        template <typename T>
        using type = angular_velocity_t<T>;
    };
}

export namespace gse {
    struct angular_acceleration_tag {};

    constexpr internal::unit<angular_acceleration_tag, std::ratio<1>, "rad/s^2"> radians_per_second_squared;
    constexpr internal::unit<angular_acceleration_tag, std::ratio_divide<pi_approx, std::ratio<180>>, "deg/s^2"> degrees_per_second_squared;

    template <>
    struct internal::dimension_traits<internal::dim<0, -2, 0>> {
        using tag = angular_acceleration_tag;
        using default_unit = decltype(radians_per_second_squared);
    };

    template <typename T = float>
    using angular_acceleration_t = internal::quantity<T, internal::dim<0, -2, 0>>;
    using angular_acceleration = angular_acceleration_t<>;

    template <>
    struct internal::quantity_traits<angular_acceleration_tag> {
        template <typename T>
        using type = angular_acceleration_t<T>;
    };
}
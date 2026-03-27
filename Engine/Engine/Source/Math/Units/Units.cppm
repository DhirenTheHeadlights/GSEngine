export module gse.math:units;

import std;
import :dimension;
import :quant;

export namespace gse::spec {
    struct angle                       : internal::quantity_spec<> {};
    struct length                      : internal::quantity_spec<> {};
    struct time                        : internal::quantity_spec<> {};
    struct mass                        : internal::quantity_spec<> {};
    struct inverse_mass                : internal::quantity_spec<> {};
    struct force                       : internal::quantity_spec<> {};
    struct energy                      : internal::kind_quantity_spec<> {};
    struct torque                      : internal::kind_quantity_spec<> {};
    struct power                       : internal::quantity_spec<> {};
    struct inertia                     : internal::quantity_spec<> {};
    struct velocity                    : internal::quantity_spec<> {};
    struct acceleration                : internal::quantity_spec<> {};
    struct angular_velocity            : internal::quantity_spec<> {};
    struct angular_acceleration        : internal::quantity_spec<> {};
    struct density                     : internal::quantity_spec<> {};
    struct time_squared                : internal::quantity_spec<> {};
    struct stiffness                   : internal::quantity_spec<> {};
    struct stiffness_per_length        : internal::quantity_spec<> {};
    struct inverse_inertia             : internal::quantity_spec<> {};
    struct area                        : internal::quantity_spec<> {};
    struct volume                      : internal::quantity_spec<> {};
    struct angular_stiffness           : internal::quantity_spec<> {};
    struct angular_stiffness_per_angle : internal::quantity_spec<> {};
    struct linear_angular_stiffness    : internal::quantity_spec<> {};
    struct displacement                : internal::quantity_spec<length> {};
}

export namespace gse {
    using pi_approx = std::ratio<355, 113>;

    constexpr internal::unit<spec::angle, std::ratio<1>, "rad"> radians;
    constexpr internal::unit<spec::angle, std::ratio_divide<pi_approx, std::ratio<180>>, "deg"> degrees;

    template <typename T = float, auto U = radians>
    using angle_t = internal::quantity<T, internal::dimi<0, 0, 0, 1>, spec::angle, decltype(U)>;
    using angle = angle_t<>;

    template <>
    struct internal::quantity_traits<spec::angle> {
        template <typename T, auto U = radians>
        using type = angle_t<T, U>;
    };

    constexpr angle rad = radians(1.f);
}

export namespace gse {
    constexpr internal::unit<spec::length, std::kilo, "km"> kilometers;
    constexpr internal::unit<spec::length, std::ratio<1>, "m"> meters;
    constexpr internal::unit<spec::length, std::centi, "cm"> centimeters;
    constexpr internal::unit<spec::length, std::milli, "mm"> millimeters;
    constexpr internal::unit<spec::length, std::ratio<1143, 1250>, "yd"> yards;
    constexpr internal::unit<spec::length, std::ratio<381, 1250>, "ft"> feet;
    constexpr internal::unit<spec::length, std::ratio<127, 5000>, "in"> inches;

    template <typename T = float, auto U = meters>
    using length_t = internal::quantity<T, internal::dimi<1, 0, 0, 0>, spec::length, decltype(U)>;
    using length = length_t<>;

    template <>
    struct internal::quantity_traits<spec::length> {
        template <typename T, auto U = meters>
        using type = length_t<T, U>;
    };
}

export namespace gse {
    template <typename T = float, auto U = meters>
    using displacement_t = internal::quantity<T, internal::dimi<1, 0, 0, 0>, spec::displacement, decltype(U)>;
    using displacement = displacement_t<>;

    template <>
    struct internal::quantity_traits<spec::displacement> {
        template <typename T, auto U = meters>
        using type = displacement_t<T, U>;
    };
}

export namespace gse {
    template <typename T = float, auto U = meters>
    using position_t = internal::quantity_point<displacement_t<T, U>>;
    using position = position_t<>;
}

export namespace gse {
    constexpr internal::unit<spec::time, std::ratio<1>, "ns"> nanoseconds;
    constexpr internal::unit<spec::time, std::kilo, "us"> microseconds;
    constexpr internal::unit<spec::time, std::mega, "ms"> milliseconds;
    constexpr internal::unit<spec::time, std::giga, "s"> seconds;
    constexpr internal::unit<spec::time, std::ratio<60'000'000'000>, "min"> minutes;
    constexpr internal::unit<spec::time, std::ratio<3'600'000'000'000>, "hr"> hours;

    template <typename T = float, auto U = nanoseconds>
    using time_t = internal::quantity<T, internal::dimi<0, 1, 0, 0>, spec::time, decltype(U)>;
    using time = time_t<>;

    template <>
    struct internal::quantity_traits<spec::time> {
        template <typename T, auto U = nanoseconds>
        using type = time_t<T, U>;
    };
}

export namespace gse {
    constexpr internal::unit<spec::mass, std::ratio<1>, "kg"> kilograms;
    constexpr internal::unit<spec::mass, std::milli, "g"> grams;
    constexpr internal::unit<spec::mass, std::micro, "mg"> milligrams;
    constexpr internal::unit<spec::mass, std::ratio<45359237, 100000000>, "lb"> pounds;
    constexpr internal::unit<spec::mass, std::ratio<45359237, 1600000000>, "oz"> ounces;

    template <typename T = float, auto U = kilograms>
    using mass_t = internal::quantity<T, internal::dimi<0, 0, 1, 0>, spec::mass, decltype(U)>;
    using mass = mass_t<>;

    template <>
    struct internal::quantity_traits<spec::mass> {
        template <typename T, auto U = kilograms>
        using type = mass_t<T, U>;
    };
}

export namespace gse {
    constexpr internal::unit<spec::inverse_mass, std::ratio<1>, "1/kg"> per_kilograms;

    template <typename T = float, auto U = per_kilograms>
    using inverse_mass_t = internal::quantity<T, internal::dimi<0, 0, -1, 0>, spec::inverse_mass, decltype(U)>;
    using inverse_mass = inverse_mass_t<>;

    template <>
    struct internal::quantity_traits<spec::inverse_mass> {
        template <typename T, auto U = per_kilograms>
        using type = inverse_mass_t<T, U>;
    };
}

export namespace gse {
    constexpr internal::unit<spec::force, std::ratio<1>, "N"> newtons;
    constexpr internal::unit<spec::force, std::ratio<222411, 50000>, "lbf"> pounds_force;

    template <typename T = float, auto U = newtons>
    using force_t = internal::quantity<T, internal::dimi<1, -2, 1, 0>, spec::force, decltype(U)>;
    using force = force_t<>;

    template <>
    struct internal::quantity_traits<spec::force> {
        template <typename T, auto U = newtons>
        using type = force_t<T, U>;
    };
}

export namespace gse {
    constexpr internal::unit<spec::energy, std::giga, "GJ"> gigajoules;
    constexpr internal::unit<spec::energy, std::mega, "MJ"> megajoules;
    constexpr internal::unit<spec::energy, std::kilo, "kJ"> kilojoules;
    constexpr internal::unit<spec::energy, std::ratio<1>, "J"> joules;
    constexpr internal::unit<spec::energy, std::ratio<4184>, "kcal"> kilocalories;
    constexpr internal::unit<spec::energy, std::ratio<523, 125>, "cal"> calories;

    template <typename T = float, auto U = joules>
    using energy_t = internal::quantity<T, internal::dimi<2, -2, 1, 0>, spec::energy, decltype(U)>;
    using energy = energy_t<>;

    template <>
    struct internal::quantity_traits<spec::energy> {
        template <typename T, auto U = joules>
        using type = energy_t<T, U>;
    };
}

export namespace gse {
    constexpr internal::unit<spec::torque, std::ratio<1>, "N-m"> newton_meters;
    constexpr internal::unit<spec::torque, std::ratio<67791, 50000>, "lbf-ft"> pound_feet;

    template <typename T = float, auto U = newton_meters>
    using torque_t = internal::quantity<T, internal::dimi<2, -2, 1, 0>, spec::torque, decltype(U)>;
    using torque = torque_t<>;

    template <>
    struct internal::quantity_traits<spec::torque> {
        template <typename T, auto U = newton_meters>
        using type = torque_t<T, U>;
    };
}

export namespace gse {
    constexpr internal::unit<spec::power, std::giga, "GW"> gigawatts;
    constexpr internal::unit<spec::power, std::mega, "MW"> megawatts;
    constexpr internal::unit<spec::power, std::kilo, "kW"> kilowatts;
    constexpr internal::unit<spec::power, std::ratio<1>, "W"> watts;
    constexpr internal::unit<spec::power, std::ratio<7457, 10>, "hp"> horsepower;

    template <typename T = float, auto U = watts>
    using power_t = internal::quantity<T, internal::dimi<2, -3, 1, 0>, spec::power, decltype(U)>;
    using power = power_t<>;

    template <>
    struct internal::quantity_traits<spec::power> {
        template <typename T, auto U = watts>
        using type = power_t<T, U>;
    };
}

export namespace gse {
    constexpr internal::unit<spec::inertia, std::ratio<1>, "kg-m^2"> kilograms_meters_squared;
    constexpr internal::unit<spec::inertia, std::ratio<4214011, 100000000>, "lb-ft^2"> pounds_feet_squared;

    template <typename T = float, auto U = kilograms_meters_squared>
    using inertia_t = internal::quantity<T, internal::dimi<2, 0, 1, 0>, spec::inertia, decltype(U)>;
    using inertia = inertia_t<>;

    template <>
    struct internal::quantity_traits<spec::inertia> {
        template <typename T, auto U = kilograms_meters_squared>
        using type = inertia_t<T, U>;
    };
}

export namespace gse {
    constexpr internal::unit<spec::velocity, std::ratio<1>, "m/s"> meters_per_second;
    constexpr internal::unit<spec::velocity, std::ratio<5, 18>, "km/h"> kilometers_per_hour;
    constexpr internal::unit<spec::velocity, std::ratio<1397, 3125>, "mph"> miles_per_hour;

    template <typename T = float, auto U = meters_per_second>
    using velocity_t = internal::quantity<T, internal::dimi<1, -1, 0, 0>, spec::velocity, decltype(U)>;
    using velocity = velocity_t<>;

    template <>
    struct internal::quantity_traits<spec::velocity> {
        template <typename T, auto U = meters_per_second>
        using type = velocity_t<T, U>;
    };
}

export namespace gse {
    constexpr internal::unit<spec::acceleration, std::ratio<1>, "m/s^2"> meters_per_second_squared;

    template <typename T = float, auto U = meters_per_second_squared>
    using acceleration_t = internal::quantity<T, internal::dimi<1, -2, 0, 0>, spec::acceleration, decltype(U)>;
    using acceleration = acceleration_t<>;

    template <>
    struct internal::quantity_traits<spec::acceleration> {
        template <typename T, auto U = meters_per_second_squared>
        using type = acceleration_t<T, U>;
    };
}

export namespace gse {
    constexpr internal::unit<spec::angular_velocity, std::ratio<1>, "rad/s"> radians_per_second;
    constexpr internal::unit<spec::angular_velocity, std::ratio_divide<pi_approx, std::ratio<180>>, "deg/s"> degrees_per_second;

    template <typename T = float, auto U = radians_per_second>
    using angular_velocity_t = internal::quantity<T, internal::dimi<0, -1, 0, 1>, spec::angular_velocity, decltype(U)>;
    using angular_velocity = angular_velocity_t<>;

    template <>
    struct internal::quantity_traits<spec::angular_velocity> {
        template <typename T, auto U = radians_per_second>
        using type = angular_velocity_t<T, U>;
    };
}

export namespace gse {
    constexpr internal::unit<spec::angular_acceleration, std::ratio<1>, "rad/s^2"> radians_per_second_squared;
    constexpr internal::unit<spec::angular_acceleration, std::ratio_divide<pi_approx, std::ratio<180>>, "deg/s^2"> degrees_per_second_squared;

    template <typename T = float, auto U = radians_per_second_squared>
    using angular_acceleration_t = internal::quantity<T, internal::dimi<0, -2, 0, 1>, spec::angular_acceleration, decltype(U)>;
    using angular_acceleration = angular_acceleration_t<>;

    template <>
    struct internal::quantity_traits<spec::angular_acceleration> {
        template <typename T, auto U = radians_per_second_squared>
        using type = angular_acceleration_t<T, U>;
    };
}

export namespace gse {
    constexpr internal::unit<spec::density, std::ratio<1>, "kg/m^3"> kilograms_per_cubic_meter;
    constexpr internal::unit<spec::density, std::ratio<625, 2266>, "lb/ft^3"> pounds_per_cubic_foot;

    template <typename T = float, auto U = kilograms_per_cubic_meter>
    using density_t = internal::quantity<T, internal::dimi<-3, 0, 1, 0>, spec::density, decltype(U)>;
    using density = density_t<>;

    template <>
    struct internal::quantity_traits<spec::density> {
        template <typename T, auto U = kilograms_per_cubic_meter>
        using type = density_t<T, U>;
    };
}

export namespace gse {
    constexpr internal::unit<spec::time_squared, std::ratio<1>, "s^2"> seconds_squared;

    template <typename T = float, auto U = seconds_squared>
    using time_squared_t = internal::quantity<T, internal::dimi<0, 2, 0, 0>, spec::time_squared, decltype(U)>;
    using time_squared = time_squared_t<>;

    template <>
    struct internal::quantity_traits<spec::time_squared> {
        template <typename T, auto U = seconds_squared>
        using type = time_squared_t<T, U>;
    };
}

export namespace gse {
    constexpr internal::unit<spec::stiffness, std::ratio<1>, "N/m"> newtons_per_meter;

    template <typename T = float, auto U = newtons_per_meter>
    using stiffness_t = internal::quantity<T, internal::dimi<0, -2, 1, 0>, spec::stiffness, decltype(U)>;
    using stiffness = stiffness_t<>;

    template <>
    struct internal::quantity_traits<spec::stiffness> {
        template <typename T, auto U = newtons_per_meter>
        using type = stiffness_t<T, U>;
    };
}

export namespace gse {
    constexpr internal::unit<spec::stiffness_per_length, std::ratio<1>, "N/m^2"> newtons_per_meter_squared;

    template <typename T = float, auto U = newtons_per_meter_squared>
    using stiffness_per_length_t = internal::quantity<T, internal::dimi<-1, -2, 1, 0>, spec::stiffness_per_length, decltype(U)>;
    using stiffness_per_length = stiffness_per_length_t<>;

    template <>
    struct internal::quantity_traits<spec::stiffness_per_length> {
        template <typename T, auto U = newtons_per_meter_squared>
        using type = stiffness_per_length_t<T, U>;
    };
}

export namespace gse {
    constexpr internal::unit<spec::inverse_inertia, std::ratio<1>, "1/(kg-m^2)"> per_kilogram_meter_squared;

    template <typename T = float, auto U = per_kilogram_meter_squared>
    using inverse_inertia_t = internal::quantity<T, internal::dimi<-2, 0, -1, 0>, spec::inverse_inertia, decltype(U)>;
    using inverse_inertia = inverse_inertia_t<>;

    template <>
    struct internal::quantity_traits<spec::inverse_inertia> {
        template <typename T, auto U = per_kilogram_meter_squared>
        using type = inverse_inertia_t<T, U>;
    };
}

export namespace gse {
    constexpr internal::unit<spec::area, std::ratio<1>, "m^2"> square_meters;
    constexpr internal::unit<spec::area, std::ratio<1027639, 10000000>, "ft^2"> square_feet;

    template <typename T = float, auto U = square_meters>
    using area_t = internal::quantity<T, internal::dimi<2, 0, 0, 0>, spec::area, decltype(U)>;
    using area = area_t<>;

    template <>
    struct internal::quantity_traits<spec::area> {
        template <typename T, auto U = square_meters>
        using type = area_t<T, U>;
    };
}

export namespace gse {
    constexpr internal::unit<spec::volume, std::ratio<1>, "m^3"> cubic_meters;

    template <typename T = float, auto U = cubic_meters>
    using volume_t = internal::quantity<T, internal::dimi<3, 0, 0, 0>, spec::volume, decltype(U)>;
    using volume = volume_t<>;

    template <>
    struct internal::quantity_traits<spec::volume> {
        template <typename T, auto U = cubic_meters>
        using type = volume_t<T, U>;
    };
}

export namespace gse {
    constexpr internal::unit<spec::angular_stiffness, std::ratio<1>, "N-m/rad"> newton_meters_per_radian;

    template <typename T = float, auto U = newton_meters_per_radian>
    using angular_stiffness_t = internal::quantity<T, internal::dimi<2, -2, 1, -1>, spec::angular_stiffness, decltype(U)>;
    using angular_stiffness = angular_stiffness_t<>;

    template <>
    struct internal::quantity_traits<spec::angular_stiffness> {
        template <typename T, auto U = newton_meters_per_radian>
        using type = angular_stiffness_t<T, U>;
    };
}

export namespace gse {
    constexpr internal::unit<spec::angular_stiffness_per_angle, std::ratio<1>, "N-m/rad^2"> newton_meters_per_radian_squared;

    template <typename T = float, auto U = newton_meters_per_radian_squared>
    using angular_stiffness_per_angle_t = internal::quantity<T, internal::dimi<2, -2, 1, -2>, spec::angular_stiffness_per_angle, decltype(U)>;
    using angular_stiffness_per_angle = angular_stiffness_per_angle_t<>;

    template <>
    struct internal::quantity_traits<spec::angular_stiffness_per_angle> {
        template <typename T, auto U = newton_meters_per_radian_squared>
        using type = angular_stiffness_per_angle_t<T, U>;
    };
}

export namespace gse {
    constexpr internal::unit<spec::linear_angular_stiffness, std::ratio<1>, "N/rad"> newtons_per_radian;

    template <typename T = float, auto U = newtons_per_radian>
    using linear_angular_stiffness_t = internal::quantity<T, internal::dimi<1, -2, 1, -1>, spec::linear_angular_stiffness, decltype(U)>;
    using linear_angular_stiffness = linear_angular_stiffness_t<>;

    template <>
    struct internal::quantity_traits<spec::linear_angular_stiffness> {
        template <typename T, auto U = newtons_per_radian>
        using type = linear_angular_stiffness_t<T, U>;
    };
}

export namespace gse {
    template <typename T>
    constexpr auto cos(const angle_t<T>& q) -> T {
        return std::cos(q.template as<radians>());
    }

    template <typename T>
    constexpr auto sin(const angle_t<T>& q) -> T {
        return std::sin(q.template as<radians>());
    }

    template <typename T>
    constexpr auto tan(const angle_t<T>& q) -> T {
        return std::tan(q.template as<radians>());
    }
}

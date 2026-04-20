export module gse.math:units;

import std;
import :dimension;
import :quant;

export namespace gse {
    using pi_approx = std::ratio<355, 113>;

    struct angle_tag {};

    template <>
    struct internal::quantity_tag_traits<angle_tag> {
        using parent_tag = no_parent_quantity_tag;
        using unit_tag = angle_tag;
        using relative_tag = angle_tag;
        static constexpr auto semantic_kind = quantity_semantic_kind::measurement;
    };

    constexpr internal::unit<angle_tag, std::ratio<1>, "rad"> radians;
    constexpr internal::unit<angle_tag, std::ratio_divide<pi_approx, std::ratio<180>>, "deg"> degrees;

    template <typename T = float, auto U = radians>
    using angle_t = internal::quantity<T, internal::dimi<0, 0, 0, 1>, angle_tag, decltype(U)>;
    using angle = angle_t<>;

    template <>
    struct internal::quantity_traits<angle_tag> {
        template <typename T, auto U = radians>
        using type = angle_t<T, U>;
    };

    constexpr angle rad = radians(1.f);
}

export namespace gse {
    struct length_tag {};

    template <>
    struct internal::quantity_tag_traits<length_tag> {
        using parent_tag = no_parent_quantity_tag;
        using unit_tag = length_tag;
        using relative_tag = length_tag;
        static constexpr auto semantic_kind = quantity_semantic_kind::relative;
    };

    constexpr internal::unit<length_tag, std::kilo, "km"> kilometers;
    constexpr internal::unit<length_tag, std::ratio<1>, "m"> meters;
    constexpr internal::unit<length_tag, std::centi, "cm"> centimeters;
    constexpr internal::unit<length_tag, std::milli, "mm"> millimeters;
    constexpr internal::unit<length_tag, std::ratio<1143, 1250>, "yd"> yards;
    constexpr internal::unit<length_tag, std::ratio<381, 1250>, "ft"> feet;
    constexpr internal::unit<length_tag, std::ratio<127, 5000>, "in"> inches;

    template <typename T = float, auto U = meters>
    using length_t = internal::quantity<T, internal::dimi<1, 0, 0, 0>, length_tag, decltype(U)>;
    using length = length_t<>;

    template <>
    struct internal::quantity_traits<length_tag> {
        template <typename T, auto U = meters>
        using type = length_t<T, U>;
    };

    struct volume_tag {};
    template <>
    struct internal::quantity_tag_traits<volume_tag> {
        using parent_tag = no_parent_quantity_tag;
        using unit_tag = volume_tag;
        using relative_tag = volume_tag;
        static constexpr auto semantic_kind = quantity_semantic_kind::measurement;
    };

    constexpr internal::unit<volume_tag, std::ratio<1>, "m^3"> cubic_meters;

    template <typename T = float, auto U = cubic_meters>
    using volume_t = internal::quantity<T, internal::dimi<3, 0, 0, 0>, volume_tag, decltype(U)>;
    using volume = volume_t<>;

    template <>
    struct internal::quantity_traits<volume_tag> {
        template <typename T, auto U = cubic_meters>
        using type = volume_t<T, U>;
    };

    struct displacement_tag {};
    template <>
    struct internal::quantity_tag_traits<displacement_tag> {
        using parent_tag = length_tag;
        using unit_tag = length_tag;
        using relative_tag = displacement_tag;
        static constexpr auto semantic_kind = quantity_semantic_kind::relative;
    };

    template <typename T = float, auto U = meters>
    using displacement_t = internal::quantity<T, internal::dimi<1, 0, 0, 0>, displacement_tag, decltype(U)>;
    using displacement = displacement_t<>;

    template <>
    struct internal::quantity_traits<displacement_tag> {
        template <typename T, auto U = meters>
        using type = displacement_t<T, U>;
    };

    struct position_tag {};
    template <>
    struct internal::quantity_tag_traits<position_tag> {
        using parent_tag = length_tag;
        using unit_tag = length_tag;
        using relative_tag = displacement_tag;
        static constexpr auto semantic_kind = quantity_semantic_kind::absolute;
    };

    template <typename T = float, auto U = meters>
    using position_t = internal::quantity<T, internal::dimi<1, 0, 0, 0>, position_tag, decltype(U)>;
    using position = position_t<>;

    template <>
    struct internal::quantity_traits<position_tag> {
        template <typename T, auto U = meters>
        using type = position_t<T, U>;
    };

    struct current_position_tag {};
    template <>
    struct internal::quantity_tag_traits<current_position_tag> {
        using parent_tag = position_tag;
        using unit_tag = length_tag;
        using relative_tag = displacement_tag;
        static constexpr auto semantic_kind = quantity_semantic_kind::absolute;
    };

    template <typename T = float, auto U = meters>
    using current_position_t = internal::quantity<T, internal::dimi<1, 0, 0, 0>, current_position_tag, decltype(U)>;
    using current_position = current_position_t<>;

    template <>
    struct internal::quantity_traits<current_position_tag> {
        template <typename T, auto U = meters>
        using type = current_position_t<T, U>;
    };

    struct previous_position_tag {};
    template <>
    struct internal::quantity_tag_traits<previous_position_tag> {
        using parent_tag = position_tag;
        using unit_tag = length_tag;
        using relative_tag = displacement_tag;
        static constexpr auto semantic_kind = quantity_semantic_kind::absolute;
    };

    template <typename T = float, auto U = meters>
    using previous_position_t = internal::quantity<T, internal::dimi<1, 0, 0, 0>, previous_position_tag, decltype(U)>;
    using previous_position = previous_position_t<>;

    template <>
    struct internal::quantity_traits<previous_position_tag> {
        template <typename T, auto U = meters>
        using type = previous_position_t<T, U>;
    };

    struct render_position_tag {};
    template <>
    struct internal::quantity_tag_traits<render_position_tag> {
        using parent_tag = position_tag;
        using unit_tag = length_tag;
        using relative_tag = displacement_tag;
        static constexpr auto semantic_kind = quantity_semantic_kind::absolute;
    };

    template <typename T = float, auto U = meters>
    using render_position_t = internal::quantity<T, internal::dimi<1, 0, 0, 0>, render_position_tag, decltype(U)>;
    using render_position = render_position_t<>;

    template <>
    struct internal::quantity_traits<render_position_tag> {
        template <typename T, auto U = meters>
        using type = render_position_t<T, U>;
    };

    struct predicted_position_tag {};
    template <>
    struct internal::quantity_tag_traits<predicted_position_tag> {
        using parent_tag = position_tag;
        using unit_tag = length_tag;
        using relative_tag = displacement_tag;
        static constexpr auto semantic_kind = quantity_semantic_kind::absolute;
    };

    template <typename T = float, auto U = meters>
    using predicted_position_t = internal::quantity<T, internal::dimi<1, 0, 0, 0>, predicted_position_tag, decltype(U)>;
    using predicted_position = predicted_position_t<>;

    template <>
    struct internal::quantity_traits<predicted_position_tag> {
        template <typename T, auto U = meters>
        using type = predicted_position_t<T, U>;
    };

    struct target_position_tag {};
    template <>
    struct internal::quantity_tag_traits<target_position_tag> {
        using parent_tag = position_tag;
        using unit_tag = length_tag;
        using relative_tag = displacement_tag;
        static constexpr auto semantic_kind = quantity_semantic_kind::absolute;
    };

    template <typename T = float, auto U = meters>
    using target_position_t = internal::quantity<T, internal::dimi<1, 0, 0, 0>, target_position_tag, decltype(U)>;
    using target_position = target_position_t<>;

    template <>
    struct internal::quantity_traits<target_position_tag> {
        template <typename T, auto U = meters>
        using type = target_position_t<T, U>;
    };

    struct offset_tag {};
    template <>
    struct internal::quantity_tag_traits<offset_tag> {
        using parent_tag = displacement_tag;
        using unit_tag = length_tag;
        using relative_tag = displacement_tag;
        static constexpr auto semantic_kind = quantity_semantic_kind::relative;
    };

    template <typename T = float, auto U = meters>
    using offset_t = internal::quantity<T, internal::dimi<1, 0, 0, 0>, offset_tag, decltype(U)>;
    using offset = offset_t<>;

    template <>
    struct internal::quantity_traits<offset_tag> {
        template <typename T, auto U = meters>
        using type = offset_t<T, U>;
    };

    struct lever_arm_tag {};
    template <>
    struct internal::quantity_tag_traits<lever_arm_tag> {
        using parent_tag = displacement_tag;
        using unit_tag = length_tag;
        using relative_tag = displacement_tag;
        static constexpr auto semantic_kind = quantity_semantic_kind::relative;
    };

    template <typename T = float, auto U = meters>
    using lever_arm_t = internal::quantity<T, internal::dimi<1, 0, 0, 0>, lever_arm_tag, decltype(U)>;
    using lever_arm = lever_arm_t<>;

    template <>
    struct internal::quantity_traits<lever_arm_tag> {
        template <typename T, auto U = meters>
        using type = lever_arm_t<T, U>;
    };

    struct gap_tag {};
    template <>
    struct internal::quantity_tag_traits<gap_tag> {
        using parent_tag = displacement_tag;
        using unit_tag = length_tag;
        using relative_tag = displacement_tag;
        static constexpr auto semantic_kind = quantity_semantic_kind::relative;
    };

    template <typename T = float, auto U = meters>
    using gap_t = internal::quantity<T, internal::dimi<1, 0, 0, 0>, gap_tag, decltype(U)>;
    using gap = gap_t<>;

    template <>
    struct internal::quantity_traits<gap_tag> {
        template <typename T, auto U = meters>
        using type = gap_t<T, U>;
    };

    struct separation_tag {};
    template <>
    struct internal::quantity_tag_traits<separation_tag> {
        using parent_tag = gap_tag;
        using unit_tag = length_tag;
        using relative_tag = displacement_tag;
        static constexpr auto semantic_kind = quantity_semantic_kind::relative;
    };

    template <typename T = float, auto U = meters>
    using separation_t = internal::quantity<T, internal::dimi<1, 0, 0, 0>, separation_tag, decltype(U)>;
    using separation = separation_t<>;

    template <>
    struct internal::quantity_traits<separation_tag> {
        template <typename T, auto U = meters>
        using type = separation_t<T, U>;
    };

    struct penetration_tag {};
    template <>
    struct internal::quantity_tag_traits<penetration_tag> {
        using parent_tag = gap_tag;
        using unit_tag = length_tag;
        using relative_tag = displacement_tag;
        static constexpr auto semantic_kind = quantity_semantic_kind::relative;
    };

    template <typename T = float, auto U = meters>
    using penetration_t = internal::quantity<T, internal::dimi<1, 0, 0, 0>, penetration_tag, decltype(U)>;
    using penetration = penetration_t<>;

    template <>
    struct internal::quantity_traits<penetration_tag> {
        template <typename T, auto U = meters>
        using type = penetration_t<T, U>;
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
    using time_t = internal::quantity<T, internal::dimi<0, 1, 0, 0>, time_tag, decltype(U)>;
    using time = time_t<>;

    template <>
    struct internal::quantity_traits<time_tag> {
        template <typename T, auto U = nanoseconds>
        using type = time_t<T, U>;

        static constexpr auto units = std::tuple{ nanoseconds, microseconds, milliseconds, seconds, minutes, hours };
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
    using mass_t = internal::quantity<T, internal::dimi<0, 0, 1, 0>, mass_tag, decltype(U)>;
    using mass = mass_t<>;

    template <>
    struct internal::quantity_traits<mass_tag> {
        template <typename T, auto U = kilograms>
        using type = mass_t<T, U>;
    };
}

export namespace gse {
    struct inverse_mass_tag {};

    template <>
    struct internal::quantity_tag_traits<inverse_mass_tag> {
        using parent_tag = no_parent_quantity_tag;
        using unit_tag = inverse_mass_tag;
        using relative_tag = inverse_mass_tag;
        static constexpr auto semantic_kind = quantity_semantic_kind::measurement;
    };

    constexpr internal::unit<inverse_mass_tag, std::ratio<1>, "1/kg"> per_kilograms;

    template <typename T = float, auto U = per_kilograms>
    using inverse_mass_t = internal::quantity<T, internal::dimi<0, 0, -1, 0>, inverse_mass_tag, decltype(U)>;
    using inverse_mass = inverse_mass_t<>;

    template <>
    struct internal::quantity_traits<inverse_mass_tag> {
        template <typename T, auto U = per_kilograms>
        using type = inverse_mass_t<T, U>;
    };

    struct linear_compliance_tag {};
    template <>
    struct internal::quantity_tag_traits<linear_compliance_tag> {
        using parent_tag = inverse_mass_tag;
        using unit_tag = inverse_mass_tag;
        using relative_tag = linear_compliance_tag;
        static constexpr auto semantic_kind = quantity_semantic_kind::measurement;
    };

    template <typename T = float, auto U = per_kilograms>
    using linear_compliance_t = internal::quantity<T, internal::dimi<0, 0, -1, 0>, linear_compliance_tag, decltype(U)>;
    using linear_compliance = linear_compliance_t<>;

    template <>
    struct internal::quantity_traits<linear_compliance_tag> {
        template <typename T, auto U = per_kilograms>
        using type = linear_compliance_t<T, U>;
    };
}

export namespace gse {
    struct force_tag {};

    constexpr internal::unit<force_tag, std::ratio<1>, "N"> newtons;
    constexpr internal::unit<force_tag, std::ratio<222411, 50000>, "lbf"> pounds_force;

    template <typename T = float, auto U = newtons>
    using force_t = internal::quantity<T, internal::dimi<1, -2, 1, 0>, force_tag, decltype(U)>;
    using force = force_t<>;

    template <>
    struct internal::quantity_traits<force_tag> {
        template <typename T, auto U = newtons>
        using type = force_t<T, U>;
    };
}

export namespace gse {
    struct impulse_tag {};

    constexpr internal::unit<impulse_tag, std::ratio<1>, "N-s"> newton_seconds;

    template <typename T = float, auto U = newton_seconds>
    using impulse_t = internal::quantity<T, internal::dimi<1, -1, 1, 0>, impulse_tag, decltype(U)>;
    using impulse = impulse_t<>;

    template <>
    struct internal::quantity_traits<impulse_tag> {
        template <typename T, auto U = newton_seconds>
        using type = impulse_t<T, U>;
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
    using energy_t = internal::quantity<T, internal::dimi<2, -2, 1, 0>, energy_tag, decltype(U)>;
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
    using torque_t = internal::quantity<T, internal::dimi<2, -2, 1, 0>, torque_tag, decltype(U)>;
    using torque = torque_t<>;

    template <>
    struct internal::quantity_traits<torque_tag> {
        template <typename T, auto U = newton_meters>
        using type = torque_t<T, U>;
    };
}

export namespace gse {
    struct angular_impulse_tag {};

    constexpr internal::unit<angular_impulse_tag, std::ratio<1>, "N-m-s"> newton_meter_seconds;

    template <typename T = float, auto U = newton_meter_seconds>
    using angular_impulse_t = internal::quantity<T, internal::dimi<2, -1, 1, 0>, angular_impulse_tag, decltype(U)>;
    using angular_impulse = angular_impulse_t<>;

    template <>
    struct internal::quantity_traits<angular_impulse_tag> {
        template <typename T, auto U = newton_meter_seconds>
        using type = angular_impulse_t<T, U>;
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
    using power_t = internal::quantity<T, internal::dimi<2, -3, 1, 0>, power_tag, decltype(U)>;
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
    using inertia_t = internal::quantity<T, internal::dimi<2, 0, 1, 0>, inertia_tag, decltype(U)>;
    using inertia = inertia_t<>;

    template <>
    struct internal::quantity_traits<inertia_tag> {
        template <typename T, auto U = kilograms_meters_squared>
        using type = inertia_t<T, U>;
    };
}

export namespace gse {
    struct velocity_tag {};

    template <>
    struct internal::quantity_tag_traits<velocity_tag> {
        using parent_tag = no_parent_quantity_tag;
        using unit_tag = velocity_tag;
        using relative_tag = velocity_tag;
        static constexpr auto semantic_kind = quantity_semantic_kind::relative;
    };

    constexpr internal::unit<velocity_tag, std::ratio<1>, "m/s"> meters_per_second;
    constexpr internal::unit<velocity_tag, std::ratio<5, 18>, "km/h"> kilometers_per_hour;
    constexpr internal::unit<velocity_tag, std::ratio<1397, 3125>, "mph"> miles_per_hour;

    template <typename T = float, auto U = meters_per_second>
    using velocity_t = internal::quantity<T, internal::dimi<1, -1, 0, 0>, velocity_tag, decltype(U)>;
    using velocity = velocity_t<>;

    template <>
    struct internal::quantity_traits<velocity_tag> {
        template <typename T, auto U = meters_per_second>
        using type = velocity_t<T, U>;
    };

    struct normal_speed_tag {};
    template <>
    struct internal::quantity_tag_traits<normal_speed_tag> {
        using parent_tag = velocity_tag;
        using unit_tag = velocity_tag;
        using relative_tag = normal_speed_tag;
        static constexpr auto semantic_kind = quantity_semantic_kind::relative;
    };

    template <typename T = float, auto U = meters_per_second>
    using normal_speed_t = internal::quantity<T, internal::dimi<1, -1, 0, 0>, normal_speed_tag, decltype(U)>;
    using normal_speed = normal_speed_t<>;

    template <>
    struct internal::quantity_traits<normal_speed_tag> {
        template <typename T, auto U = meters_per_second>
        using type = normal_speed_t<T, U>;
    };

    struct closing_speed_tag {};
    template <>
    struct internal::quantity_tag_traits<closing_speed_tag> {
        using parent_tag = normal_speed_tag;
        using unit_tag = velocity_tag;
        using relative_tag = normal_speed_tag;
        static constexpr auto semantic_kind = quantity_semantic_kind::relative;
    };

    template <typename T = float, auto U = meters_per_second>
    using closing_speed_t = internal::quantity<T, internal::dimi<1, -1, 0, 0>, closing_speed_tag, decltype(U)>;
    using closing_speed = closing_speed_t<>;

    template <>
    struct internal::quantity_traits<closing_speed_tag> {
        template <typename T, auto U = meters_per_second>
        using type = closing_speed_t<T, U>;
    };
}

export namespace gse {
    struct acceleration_tag {};

    constexpr internal::unit<acceleration_tag, std::ratio<1>, "m/s^2"> meters_per_second_squared;

    template <typename T = float, auto U = meters_per_second_squared>
    using acceleration_t = internal::quantity<T, internal::dimi<1, -2, 0, 0>, acceleration_tag, decltype(U)>;
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
    using angular_velocity_t = internal::quantity<T, internal::dimi<0, -1, 0, 1>, angular_velocity_tag, decltype(U)>;
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
    using angular_acceleration_t = internal::quantity<T, internal::dimi<0, -2, 0, 1>, angular_acceleration_tag, decltype(U)>;
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
    using density_t = internal::quantity<T, internal::dimi<-3, 0, 1, 0>, density_tag, decltype(U)>;
    using density = density_t<>;

    template <>
    struct internal::quantity_traits<density_tag> {
        template <typename T, auto U = kilograms_per_cubic_meter>
        using type = density_t<T, U>;
    };
}

export namespace gse {
    struct time_squared_tag {};

    constexpr internal::unit<time_squared_tag, std::ratio<1>, "s^2"> seconds_squared;

    template <typename T = float, auto U = seconds_squared>
    using time_squared_t = internal::quantity<T, internal::dimi<0, 2, 0, 0>, time_squared_tag, decltype(U)>;
    using time_squared = time_squared_t<>;

    template <>
    struct internal::quantity_traits<time_squared_tag> {
        template <typename T, auto U = seconds_squared>
        using type = time_squared_t<T, U>;
    };
}

export namespace gse {
    struct stiffness_tag {};

    constexpr internal::unit<stiffness_tag, std::ratio<1>, "N/m"> newtons_per_meter;

    template <typename T = float, auto U = newtons_per_meter>
    using stiffness_t = internal::quantity<T, internal::dimi<0, -2, 1, 0>, stiffness_tag, decltype(U)>;
    using stiffness = stiffness_t<>;

    template <>
    struct internal::quantity_traits<stiffness_tag> {
        template <typename T, auto U = newtons_per_meter>
        using type = stiffness_t<T, U>;
    };
}

export namespace gse {
    struct inverse_inertia_tag {};

    template <>
    struct internal::quantity_tag_traits<inverse_inertia_tag> {
        using parent_tag = no_parent_quantity_tag;
        using unit_tag = inverse_inertia_tag;
        using relative_tag = inverse_inertia_tag;
        static constexpr auto semantic_kind = quantity_semantic_kind::measurement;
    };

    constexpr internal::unit<inverse_inertia_tag, std::ratio<1>, "1/(kg-m^2)"> per_kilogram_meter_squared;

    template <typename T = float, auto U = per_kilogram_meter_squared>
    using inverse_inertia_t = internal::quantity<T, internal::dimi<-2, 0, -1, 0>, inverse_inertia_tag, decltype(U)>;
    using inverse_inertia = inverse_inertia_t<>;

    template <>
    struct internal::quantity_traits<inverse_inertia_tag> {
        template <typename T, auto U = per_kilogram_meter_squared>
        using type = inverse_inertia_t<T, U>;
    };

    struct angular_compliance_tag {};
    template <>
    struct internal::quantity_tag_traits<angular_compliance_tag> {
        using parent_tag = inverse_inertia_tag;
        using unit_tag = inverse_inertia_tag;
        using relative_tag = angular_compliance_tag;
        static constexpr auto semantic_kind = quantity_semantic_kind::measurement;
    };

    template <typename T = float, auto U = per_kilogram_meter_squared>
    using angular_compliance_t = internal::quantity<T, internal::dimi<-2, 0, -1, 0>, angular_compliance_tag, decltype(U)>;
    using angular_compliance = angular_compliance_t<>;

    template <>
    struct internal::quantity_traits<angular_compliance_tag> {
        template <typename T, auto U = per_kilogram_meter_squared>
        using type = angular_compliance_t<T, U>;
    };
}

export namespace gse {
    struct area_tag {};

    constexpr internal::unit<area_tag, std::ratio<1>, "m^2"> square_meters;
    constexpr internal::unit<area_tag, std::ratio<1027639, 10000000>, "ft^2"> square_feet;

    template <typename T = float, auto U = square_meters>
    using area_t = internal::quantity<T, internal::dimi<2, 0, 0, 0>, area_tag, decltype(U)>;
    using area = area_t<>;

    template <>
    struct internal::quantity_traits<area_tag> {
        template <typename T, auto U = square_meters>
        using type = area_t<T, U>;
    };
}

export namespace gse {
    struct angular_stiffness_tag {};

    constexpr internal::unit<angular_stiffness_tag, std::ratio<1>, "N-m/rad"> newton_meters_per_radian;

    template <typename T = float, auto U = newton_meters_per_radian>
    using angular_stiffness_t = internal::quantity<T, internal::dimi<2, -2, 1, -1>, angular_stiffness_tag, decltype(U)>;
    using angular_stiffness = angular_stiffness_t<>;

    template <>
    struct internal::quantity_traits<angular_stiffness_tag> {
        template <typename T, auto U = newton_meters_per_radian>
        using type = angular_stiffness_t<T, U>;
    };
}

export namespace gse {
    struct linear_angular_stiffness_tag {};

    constexpr internal::unit<linear_angular_stiffness_tag, std::ratio<1>, "N/rad"> newtons_per_radian;

    template <typename T = float, auto U = newtons_per_radian>
    using linear_angular_stiffness_t = internal::quantity<T, internal::dimi<1, -2, 1, -1>, linear_angular_stiffness_tag, decltype(U)>;
    using linear_angular_stiffness = linear_angular_stiffness_t<>;

    template <>
    struct internal::quantity_traits<linear_angular_stiffness_tag> {
        template <typename T, auto U = newtons_per_radian>
        using type = linear_angular_stiffness_t<T, U>;
    };
}

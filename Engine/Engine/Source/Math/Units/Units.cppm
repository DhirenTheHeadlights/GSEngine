export module gse.math:units;

import std;

import :dimension;
import :quant;

export namespace gse {
    using pi_approx = std::ratio<355, 113>;

    struct angle_tag;

    constexpr internal::unit<angle_tag, std::ratio<1>, "rad"> radians;
    constexpr internal::unit<angle_tag, std::ratio_divide<pi_approx, std::ratio<180>>, "deg"> degrees;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<0, 0, 0, 1>,
        ^^radians,
        ^^internal::no_parent_quantity_tag,
        ^^angle_tag,
        ^^angle_tag,
        internal::quantity_semantic_kind::measurement
        > ]] angle_tag {};

    template <typename T = float, auto U = radians>
    using angle_t = typename internal::quantity_traits<angle_tag>::template type<T, U>;
    using angle = angle_t<>;

    constexpr angle rad = radians(1.f);
}

export namespace gse {
    struct length_tag;

    constexpr internal::unit<length_tag, std::kilo, "km"> kilometers;
    constexpr internal::unit<length_tag, std::ratio<1>, "m"> meters;
    constexpr internal::unit<length_tag, std::centi, "cm"> centimeters;
    constexpr internal::unit<length_tag, std::milli, "mm"> millimeters;
    constexpr internal::unit<length_tag, std::ratio<1143, 1250>, "yd"> yards;
    constexpr internal::unit<length_tag, std::ratio<381, 1250>, "ft"> feet;
    constexpr internal::unit<length_tag, std::ratio<127, 5000>, "in"> inches;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<1, 0, 0, 0>,
        ^^meters,
        ^^internal::no_parent_quantity_tag,
        ^^length_tag,
        ^^length_tag,
        internal::quantity_semantic_kind::relative
        > ]] length_tag {};

    template <typename T = float, auto U = meters>
    using length_t = typename internal::quantity_traits<length_tag>::template type<T, U>;
    using length = length_t<>;

    struct volume_tag;

    constexpr internal::unit<volume_tag, std::ratio<1>, "m^3"> cubic_meters;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<3, 0, 0, 0>,
        ^^cubic_meters,
        ^^internal::no_parent_quantity_tag,
        ^^volume_tag,
        ^^volume_tag,
        internal::quantity_semantic_kind::measurement
        > ]] volume_tag {};

    template <typename T = float, auto U = cubic_meters>
    using volume_t = typename internal::quantity_traits<volume_tag>::template type<T, U>;
    using volume = volume_t<>;

    struct displacement_tag;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<1, 0, 0, 0>,
        ^^meters,
        ^^length_tag,
        ^^length_tag,
        ^^displacement_tag,
        internal::quantity_semantic_kind::relative
        > ]] displacement_tag {};

    template <typename T = float, auto U = meters>
    using displacement_t = typename internal::quantity_traits<displacement_tag>::template type<T, U>;
    using displacement = displacement_t<>;

    struct position_tag;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<1, 0, 0, 0>,
        ^^meters,
        ^^length_tag,
        ^^length_tag,
        ^^displacement_tag,
        internal::quantity_semantic_kind::absolute
        > ]] position_tag {};

    template <typename T = float, auto U = meters>
    using position_t = typename internal::quantity_traits<position_tag>::template type<T, U>;
    using position = position_t<>;

    struct current_position_tag;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<1, 0, 0, 0>,
        ^^meters,
        ^^position_tag,
        ^^length_tag,
        ^^displacement_tag,
        internal::quantity_semantic_kind::absolute
        > ]] current_position_tag {};

    template <typename T = float, auto U = meters>
    using current_position_t = typename internal::quantity_traits<current_position_tag>::template type<T, U>;
    using current_position = current_position_t<>;

    struct previous_position_tag;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<1, 0, 0, 0>,
        ^^meters,
        ^^position_tag,
        ^^length_tag,
        ^^displacement_tag,
        internal::quantity_semantic_kind::absolute
        > ]] previous_position_tag {};

    template <typename T = float, auto U = meters>
    using previous_position_t = typename internal::quantity_traits<previous_position_tag>::template type<T, U>;
    using previous_position = previous_position_t<>;

    struct render_position_tag;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<1, 0, 0, 0>,
        ^^meters,
        ^^position_tag,
        ^^length_tag,
        ^^displacement_tag,
        internal::quantity_semantic_kind::absolute
        > ]] render_position_tag {};

    template <typename T = float, auto U = meters>
    using render_position_t = typename internal::quantity_traits<render_position_tag>::template type<T, U>;
    using render_position = render_position_t<>;

    struct predicted_position_tag;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<1, 0, 0, 0>,
        ^^meters,
        ^^position_tag,
        ^^length_tag,
        ^^displacement_tag,
        internal::quantity_semantic_kind::absolute
        > ]] predicted_position_tag {};

    template <typename T = float, auto U = meters>
    using predicted_position_t = typename internal::quantity_traits<predicted_position_tag>::template type<T, U>;
    using predicted_position = predicted_position_t<>;

    struct target_position_tag;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<1, 0, 0, 0>,
        ^^meters,
        ^^position_tag,
        ^^length_tag,
        ^^displacement_tag,
        internal::quantity_semantic_kind::absolute
        > ]] target_position_tag {};

    template <typename T = float, auto U = meters>
    using target_position_t = typename internal::quantity_traits<target_position_tag>::template type<T, U>;
    using target_position = target_position_t<>;

    struct offset_tag;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<1, 0, 0, 0>,
        ^^meters,
        ^^displacement_tag,
        ^^length_tag,
        ^^displacement_tag,
        internal::quantity_semantic_kind::relative
        > ]] offset_tag {};

    template <typename T = float, auto U = meters>
    using offset_t = typename internal::quantity_traits<offset_tag>::template type<T, U>;
    using offset = offset_t<>;

    struct lever_arm_tag;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<1, 0, 0, 0>,
        ^^meters,
        ^^displacement_tag,
        ^^length_tag,
        ^^displacement_tag,
        internal::quantity_semantic_kind::relative
        > ]] lever_arm_tag {};

    template <typename T = float, auto U = meters>
    using lever_arm_t = typename internal::quantity_traits<lever_arm_tag>::template type<T, U>;
    using lever_arm = lever_arm_t<>;

    struct gap_tag;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<1, 0, 0, 0>,
        ^^meters,
        ^^displacement_tag,
        ^^length_tag,
        ^^displacement_tag,
        internal::quantity_semantic_kind::relative
        > ]] gap_tag {};

    template <typename T = float, auto U = meters>
    using gap_t = typename internal::quantity_traits<gap_tag>::template type<T, U>;
    using gap = gap_t<>;

    struct separation_tag;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<1, 0, 0, 0>,
        ^^meters,
        ^^gap_tag,
        ^^length_tag,
        ^^displacement_tag,
        internal::quantity_semantic_kind::relative
        > ]] separation_tag {};

    template <typename T = float, auto U = meters>
    using separation_t = typename internal::quantity_traits<separation_tag>::template type<T, U>;
    using separation = separation_t<>;

    struct penetration_tag;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<1, 0, 0, 0>,
        ^^meters,
        ^^gap_tag,
        ^^length_tag,
        ^^displacement_tag,
        internal::quantity_semantic_kind::relative
        > ]] penetration_tag {};

    template <typename T = float, auto U = meters>
    using penetration_t = typename internal::quantity_traits<penetration_tag>::template type<T, U>;
    using penetration = penetration_t<>;
}

export namespace gse {
    struct time_tag;

    constexpr internal::unit<time_tag, std::ratio<1>, "ns"> nanoseconds;
    constexpr internal::unit<time_tag, std::kilo, "us"> microseconds;
    constexpr internal::unit<time_tag, std::mega, "ms"> milliseconds;
    constexpr internal::unit<time_tag, std::giga, "s"> seconds;
    constexpr internal::unit<time_tag, std::ratio<60'000'000'000>, "min"> minutes;
    constexpr internal::unit<time_tag, std::ratio<3'600'000'000'000>, "hr"> hours;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<0, 1, 0, 0>,
        ^^nanoseconds,
        ^^internal::no_parent_quantity_tag,
        ^^time_tag,
        ^^time_tag,
        internal::quantity_semantic_kind::measurement
        > ]] time_tag {};

    template <typename T = float, auto U = nanoseconds>
    using time_t = typename internal::quantity_traits<time_tag>::template type<T, U>;
    using time = time_t<>;
}

template <>
struct gse::internal::quantity_units<gse::time_tag> {
    static constexpr auto units = std::tuple{
        gse::nanoseconds,
        gse::microseconds,
        gse::milliseconds,
        gse::seconds,
        gse::minutes,
        gse::hours,
    };
};

export namespace gse {
    struct mass_tag;

    constexpr internal::unit<mass_tag, std::ratio<1>, "kg"> kilograms;
    constexpr internal::unit<mass_tag, std::milli, "g"> grams;
    constexpr internal::unit<mass_tag, std::micro, "mg"> milligrams;
    constexpr internal::unit<mass_tag, std::ratio<45359237, 100000000>, "lb"> pounds;
    constexpr internal::unit<mass_tag, std::ratio<45359237, 1600000000>, "oz"> ounces;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<0, 0, 1, 0>,
        ^^kilograms,
        ^^internal::no_parent_quantity_tag,
        ^^mass_tag,
        ^^mass_tag,
        internal::quantity_semantic_kind::measurement
        > ]] mass_tag {};

    template <typename T = float, auto U = kilograms>
    using mass_t = typename internal::quantity_traits<mass_tag>::template type<T, U>;
    using mass = mass_t<>;
}

export namespace gse {
    struct inverse_mass_tag;

    constexpr internal::unit<inverse_mass_tag, std::ratio<1>, "1/kg"> per_kilograms;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<0, 0, -1, 0>,
        ^^per_kilograms,
        ^^internal::no_parent_quantity_tag,
        ^^inverse_mass_tag,
        ^^inverse_mass_tag,
        internal::quantity_semantic_kind::measurement
        > ]] inverse_mass_tag {};

    template <typename T = float, auto U = per_kilograms>
    using inverse_mass_t = typename internal::quantity_traits<inverse_mass_tag>::template type<T, U>;
    using inverse_mass = inverse_mass_t<>;

    struct linear_compliance_tag;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<0, 0, -1, 0>,
        ^^per_kilograms,
        ^^inverse_mass_tag,
        ^^inverse_mass_tag,
        ^^linear_compliance_tag,
        internal::quantity_semantic_kind::measurement
        > ]] linear_compliance_tag {};

    template <typename T = float, auto U = per_kilograms>
    using linear_compliance_t = typename internal::quantity_traits<linear_compliance_tag>::template type<T, U>;
    using linear_compliance = linear_compliance_t<>;
}

export namespace gse {
    struct force_tag;

    constexpr internal::unit<force_tag, std::ratio<1>, "N"> newtons;
    constexpr internal::unit<force_tag, std::ratio<222411, 50000>, "lbf"> pounds_force;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<1, -2, 1, 0>,
        ^^newtons,
        ^^internal::no_parent_quantity_tag,
        ^^force_tag,
        ^^force_tag,
        internal::quantity_semantic_kind::measurement
        > ]] force_tag {};

    template <typename T = float, auto U = newtons>
    using force_t = typename internal::quantity_traits<force_tag>::template type<T, U>;
    using force = force_t<>;
}

export namespace gse {
    struct impulse_tag;

    constexpr internal::unit<impulse_tag, std::ratio<1>, "N-s"> newton_seconds;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<1, -1, 1, 0>,
        ^^newton_seconds,
        ^^internal::no_parent_quantity_tag,
        ^^impulse_tag,
        ^^impulse_tag,
        internal::quantity_semantic_kind::measurement
        > ]] impulse_tag {};

    template <typename T = float, auto U = newton_seconds>
    using impulse_t = typename internal::quantity_traits<impulse_tag>::template type<T, U>;
    using impulse = impulse_t<>;
}

export namespace gse {
    struct energy_tag;

    constexpr internal::unit<energy_tag, std::giga, "GJ"> gigajoules;
    constexpr internal::unit<energy_tag, std::mega, "MJ"> megajoules;
    constexpr internal::unit<energy_tag, std::kilo, "kJ"> kilojoules;
    constexpr internal::unit<energy_tag, std::ratio<1>, "J"> joules;
    constexpr internal::unit<energy_tag, std::ratio<4184>, "kcal"> kilocalories;
    constexpr internal::unit<energy_tag, std::ratio<523, 125>, "cal"> calories;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<2, -2, 1, 0>,
        ^^joules,
        ^^internal::no_parent_quantity_tag,
        ^^energy_tag,
        ^^energy_tag,
        internal::quantity_semantic_kind::measurement
        > ]] energy_tag {};

    template <typename T = float, auto U = joules>
    using energy_t = typename internal::quantity_traits<energy_tag>::template type<T, U>;
    using energy = energy_t<>;
}

export namespace gse {
    struct torque_tag;

    constexpr internal::unit<torque_tag, std::ratio<1>, "N-m"> newton_meters;
    constexpr internal::unit<torque_tag, std::ratio<67791, 50000>, "lbf-ft"> pound_feet;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<2, -2, 1, 0>,
        ^^newton_meters,
        ^^internal::no_parent_quantity_tag,
        ^^torque_tag,
        ^^torque_tag,
        internal::quantity_semantic_kind::measurement
        > ]] torque_tag {};

    template <typename T = float, auto U = newton_meters>
    using torque_t = typename internal::quantity_traits<torque_tag>::template type<T, U>;
    using torque = torque_t<>;
}

export namespace gse {
    struct angular_impulse_tag;

    constexpr internal::unit<angular_impulse_tag, std::ratio<1>, "N-m-s"> newton_meter_seconds;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<2, -1, 1, 0>,
        ^^newton_meter_seconds,
        ^^internal::no_parent_quantity_tag,
        ^^angular_impulse_tag,
        ^^angular_impulse_tag,
        internal::quantity_semantic_kind::measurement
        > ]] angular_impulse_tag {};

    template <typename T = float, auto U = newton_meter_seconds>
    using angular_impulse_t = typename internal::quantity_traits<angular_impulse_tag>::template type<T, U>;
    using angular_impulse = angular_impulse_t<>;
}

export namespace gse {
    struct power_tag;

    constexpr internal::unit<power_tag, std::giga, "GW"> gigawatts;
    constexpr internal::unit<power_tag, std::mega, "MW"> megawatts;
    constexpr internal::unit<power_tag, std::kilo, "kW"> kilowatts;
    constexpr internal::unit<power_tag, std::ratio<1>, "W"> watts;
    constexpr internal::unit<power_tag, std::ratio<7457, 10>, "hp"> horsepower;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<2, -3, 1, 0>,
        ^^watts,
        ^^internal::no_parent_quantity_tag,
        ^^power_tag,
        ^^power_tag,
        internal::quantity_semantic_kind::measurement
        > ]] power_tag {};

    template <typename T = float, auto U = watts>
    using power_t = typename internal::quantity_traits<power_tag>::template type<T, U>;
    using power = power_t<>;
}

export namespace gse {
    struct inertia_tag;

    constexpr internal::unit<inertia_tag, std::ratio<1>, "kg-m^2"> kilograms_meters_squared;
    constexpr internal::unit<inertia_tag, std::ratio<4214011, 100000000>, "lb-ft^2"> pounds_feet_squared;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<2, 0, 1, 0>,
        ^^kilograms_meters_squared,
        ^^internal::no_parent_quantity_tag,
        ^^inertia_tag,
        ^^inertia_tag,
        internal::quantity_semantic_kind::measurement
        > ]] inertia_tag {};

    template <typename T = float, auto U = kilograms_meters_squared>
    using inertia_t = typename internal::quantity_traits<inertia_tag>::template type<T, U>;
    using inertia = inertia_t<>;
}

export namespace gse {
    struct velocity_tag;

    constexpr internal::unit<velocity_tag, std::ratio<1>, "m/s"> meters_per_second;
    constexpr internal::unit<velocity_tag, std::ratio<5, 18>, "km/h"> kilometers_per_hour;
    constexpr internal::unit<velocity_tag, std::ratio<1397, 3125>, "mph"> miles_per_hour;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<1, -1, 0, 0>,
        ^^meters_per_second,
        ^^internal::no_parent_quantity_tag,
        ^^velocity_tag,
        ^^velocity_tag,
        internal::quantity_semantic_kind::relative
        > ]] velocity_tag {};

    template <typename T = float, auto U = meters_per_second>
    using velocity_t = typename internal::quantity_traits<velocity_tag>::template type<T, U>;
    using velocity = velocity_t<>;

    struct normal_speed_tag;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<1, -1, 0, 0>,
        ^^meters_per_second,
        ^^velocity_tag,
        ^^velocity_tag,
        ^^normal_speed_tag,
        internal::quantity_semantic_kind::relative
        > ]] normal_speed_tag {};

    template <typename T = float, auto U = meters_per_second>
    using normal_speed_t = typename internal::quantity_traits<normal_speed_tag>::template type<T, U>;
    using normal_speed = normal_speed_t<>;

    struct closing_speed_tag;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<1, -1, 0, 0>,
        ^^meters_per_second,
        ^^normal_speed_tag,
        ^^velocity_tag,
        ^^normal_speed_tag,
        internal::quantity_semantic_kind::relative
        > ]] closing_speed_tag {};

    template <typename T = float, auto U = meters_per_second>
    using closing_speed_t = typename internal::quantity_traits<closing_speed_tag>::template type<T, U>;
    using closing_speed = closing_speed_t<>;
}

export namespace gse {
    struct acceleration_tag;

    constexpr internal::unit<acceleration_tag, std::ratio<1>, "m/s^2"> meters_per_second_squared;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<1, -2, 0, 0>,
        ^^meters_per_second_squared,
        ^^internal::no_parent_quantity_tag,
        ^^acceleration_tag,
        ^^acceleration_tag,
        internal::quantity_semantic_kind::measurement
        > ]] acceleration_tag {};

    template <typename T = float, auto U = meters_per_second_squared>
    using acceleration_t = typename internal::quantity_traits<acceleration_tag>::template type<T, U>;
    using acceleration = acceleration_t<>;
}

export namespace gse {
    struct angular_velocity_tag;

    constexpr internal::unit<angular_velocity_tag, std::ratio<1>, "rad/s"> radians_per_second;
    constexpr internal::unit<angular_velocity_tag, std::ratio_divide<pi_approx, std::ratio<180>>, "deg/s"> degrees_per_second;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<0, -1, 0, 1>,
        ^^radians_per_second,
        ^^internal::no_parent_quantity_tag,
        ^^angular_velocity_tag,
        ^^angular_velocity_tag,
        internal::quantity_semantic_kind::measurement
        > ]] angular_velocity_tag {};

    template <typename T = float, auto U = radians_per_second>
    using angular_velocity_t = typename internal::quantity_traits<angular_velocity_tag>::template type<T, U>;
    using angular_velocity = angular_velocity_t<>;
}

export namespace gse {
    struct angular_acceleration_tag;

    constexpr internal::unit<angular_acceleration_tag, std::ratio<1>, "rad/s^2"> radians_per_second_squared;
    constexpr internal::unit<angular_acceleration_tag, std::ratio_divide<pi_approx, std::ratio<180>>, "deg/s^2"> degrees_per_second_squared;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<0, -2, 0, 1>,
        ^^radians_per_second_squared,
        ^^internal::no_parent_quantity_tag,
        ^^angular_acceleration_tag,
        ^^angular_acceleration_tag,
        internal::quantity_semantic_kind::measurement
        > ]] angular_acceleration_tag {};

    template <typename T = float, auto U = radians_per_second_squared>
    using angular_acceleration_t = typename internal::quantity_traits<angular_acceleration_tag>::template type<T, U>;
    using angular_acceleration = angular_acceleration_t<>;
}

export namespace gse {
    struct density_tag;

    constexpr internal::unit<density_tag, std::ratio<1>, "kg/m^3"> kilograms_per_cubic_meter;
    constexpr internal::unit<density_tag, std::ratio<625, 2266>, "lb/ft^3"> pounds_per_cubic_foot;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<-3, 0, 1, 0>,
        ^^kilograms_per_cubic_meter,
        ^^internal::no_parent_quantity_tag,
        ^^density_tag,
        ^^density_tag,
        internal::quantity_semantic_kind::measurement
        > ]] density_tag {};

    template <typename T = float, auto U = kilograms_per_cubic_meter>
    using density_t = typename internal::quantity_traits<density_tag>::template type<T, U>;
    using density = density_t<>;
}

export namespace gse {
    struct time_squared_tag;

    constexpr internal::unit<time_squared_tag, std::ratio<1>, "s^2"> seconds_squared;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<0, 2, 0, 0>,
        ^^seconds_squared,
        ^^internal::no_parent_quantity_tag,
        ^^time_squared_tag,
        ^^time_squared_tag,
        internal::quantity_semantic_kind::measurement
        > ]] time_squared_tag {};

    template <typename T = float, auto U = seconds_squared>
    using time_squared_t = typename internal::quantity_traits<time_squared_tag>::template type<T, U>;
    using time_squared = time_squared_t<>;
}

export namespace gse {
    struct stiffness_tag;

    constexpr internal::unit<stiffness_tag, std::ratio<1>, "N/m"> newtons_per_meter;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<0, -2, 1, 0>,
        ^^newtons_per_meter,
        ^^internal::no_parent_quantity_tag,
        ^^stiffness_tag,
        ^^stiffness_tag,
        internal::quantity_semantic_kind::measurement
        > ]] stiffness_tag {};

    template <typename T = float, auto U = newtons_per_meter>
    using stiffness_t = typename internal::quantity_traits<stiffness_tag>::template type<T, U>;
    using stiffness = stiffness_t<>;
}

export namespace gse {
    struct inverse_inertia_tag;

    constexpr internal::unit<inverse_inertia_tag, std::ratio<1>, "1/(kg-m^2)"> per_kilogram_meter_squared;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<-2, 0, -1, 0>,
        ^^per_kilogram_meter_squared,
        ^^internal::no_parent_quantity_tag,
        ^^inverse_inertia_tag,
        ^^inverse_inertia_tag,
        internal::quantity_semantic_kind::measurement
        > ]] inverse_inertia_tag {};

    template <typename T = float, auto U = per_kilogram_meter_squared>
    using inverse_inertia_t = typename internal::quantity_traits<inverse_inertia_tag>::template type<T, U>;
    using inverse_inertia = inverse_inertia_t<>;

    struct angular_compliance_tag;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<-2, 0, -1, 0>,
        ^^per_kilogram_meter_squared,
        ^^inverse_inertia_tag,
        ^^inverse_inertia_tag,
        ^^angular_compliance_tag,
        internal::quantity_semantic_kind::measurement
        > ]] angular_compliance_tag {};

    template <typename T = float, auto U = per_kilogram_meter_squared>
    using angular_compliance_t = typename internal::quantity_traits<angular_compliance_tag>::template type<T, U>;
    using angular_compliance = angular_compliance_t<>;
}

export namespace gse {
    struct area_tag;

    constexpr internal::unit<area_tag, std::ratio<1>, "m^2"> square_meters;
    constexpr internal::unit<area_tag, std::ratio<1027639, 10000000>, "ft^2"> square_feet;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<2, 0, 0, 0>,
        ^^square_meters,
        ^^internal::no_parent_quantity_tag,
        ^^area_tag,
        ^^area_tag,
        internal::quantity_semantic_kind::measurement
        > ]] area_tag {};

    template <typename T = float, auto U = square_meters>
    using area_t = typename internal::quantity_traits<area_tag>::template type<T, U>;
    using area = area_t<>;
}

export namespace gse {
    struct angular_stiffness_tag;

    constexpr internal::unit<angular_stiffness_tag, std::ratio<1>, "N-m/rad"> newton_meters_per_radian;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<2, -2, 1, -1>,
        ^^newton_meters_per_radian,
        ^^internal::no_parent_quantity_tag,
        ^^angular_stiffness_tag,
        ^^angular_stiffness_tag,
        internal::quantity_semantic_kind::measurement
        > ]] angular_stiffness_tag {};

    template <typename T = float, auto U = newton_meters_per_radian>
    using angular_stiffness_t = typename internal::quantity_traits<angular_stiffness_tag>::template type<T, U>;
    using angular_stiffness = angular_stiffness_t<>;
}

export namespace gse {
    struct linear_angular_stiffness_tag;

    constexpr internal::unit<linear_angular_stiffness_tag, std::ratio<1>, "N/rad"> newtons_per_radian;

    struct [[= internal::quantity_spec_v<
        ^^internal::dimi<1, -2, 1, -1>,
        ^^newtons_per_radian,
        ^^internal::no_parent_quantity_tag,
        ^^linear_angular_stiffness_tag,
        ^^linear_angular_stiffness_tag,
        internal::quantity_semantic_kind::measurement
        > ]] linear_angular_stiffness_tag {};

    template <typename T = float, auto U = newtons_per_radian>
    using linear_angular_stiffness_t = typename internal::quantity_traits<linear_angular_stiffness_tag>::template type<T, U>;
    using linear_angular_stiffness = linear_angular_stiffness_t<>;
}

namespace gse::internal {
    static_assert(std::same_as<force::dimension, dimi<1, -2, 1, 0>>);
    static_assert(std::same_as<quantity_tag_traits<closing_speed_tag>::parent_tag, normal_speed_tag>);
    static_assert(quantity_tag_traits<position_tag>::semantic_kind == quantity_semantic_kind::absolute);
    static_assert(has_unit_list<time_tag>);
}

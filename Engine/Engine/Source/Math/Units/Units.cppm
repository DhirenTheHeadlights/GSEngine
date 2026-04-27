export module gse.math:units;

import std;

import :dimension;
import :quant;

export namespace gse {
    using pi_approx = std::ratio<355, 113>;

    struct [[= internal::quantity_root<^^internal::dimi<0, 0, 0, 1>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "rad">]]
    angle_tag {};

    constexpr internal::unit<angle_tag, std::ratio<1>, "rad"> radians;
    constexpr internal::unit<angle_tag, std::ratio_divide<pi_approx, std::ratio<180>>, "deg"> degrees;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^angle_tag) :])>
    using angle_t = internal::quantity_t<angle_tag, T, U>;
    using angle = angle_t<>;

    constexpr angle rad = radians(1.f);
}

namespace gse::internal {
    template <> struct dimension_to_tag<dimi<0, 0, 0, 1>> { static constexpr bool found = true; using tag = gse::angle_tag; };

    template <> struct quantity_units<gse::angle_tag> {
        static constexpr auto units = std::tuple{ gse::radians, gse::degrees };
    };
}

export namespace gse {
    struct [[= internal::quantity_root<^^internal::dimi<1, 0, 0, 0>, internal::quantity_semantic_kind::relative, std::ratio<1>, "m">]]
    length_tag {};

    constexpr internal::unit<length_tag, std::kilo, "km"> kilometers;
    constexpr internal::unit<length_tag, std::ratio<1>, "m"> meters;
    constexpr internal::unit<length_tag, std::centi, "cm"> centimeters;
    constexpr internal::unit<length_tag, std::milli, "mm"> millimeters;
    constexpr internal::unit<length_tag, std::ratio<1143, 1250>, "yd"> yards;
    constexpr internal::unit<length_tag, std::ratio<381, 1250>, "ft"> feet;
    constexpr internal::unit<length_tag, std::ratio<127, 5000>, "in"> inches;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^length_tag) :])>
    using length_t = internal::quantity_t<length_tag, T, U>;
    using length = length_t<>;

    struct [[= internal::quantity_root<^^internal::dimi<3, 0, 0, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "m^3">]]
    volume_tag {};

    constexpr internal::unit<volume_tag, std::ratio<1>, "m^3"> cubic_meters;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^volume_tag) :])>
    using volume_t = internal::quantity_t<volume_tag, T, U>;
    using volume = volume_t<>;

    struct [[= internal::quantity_sub_root<^^length_tag>]]
    displacement_tag {};

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^displacement_tag) :])>
    using displacement_t = internal::quantity_t<displacement_tag, T, U>;
    using displacement = displacement_t<>;

    struct [[= internal::quantity_absolute<^^length_tag, ^^displacement_tag>]]
    position_tag {};

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^position_tag) :])>
    using position_t = internal::quantity_t<position_tag, T, U>;
    using position = position_t<>;

    struct [[= internal::quantity_child<^^position_tag, internal::quantity_semantic_kind::absolute>]]
    current_position_tag {};

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^current_position_tag) :])>
    using current_position_t = internal::quantity_t<current_position_tag, T, U>;
    using current_position = current_position_t<>;

    struct [[= internal::quantity_child<^^position_tag, internal::quantity_semantic_kind::absolute>]]
    previous_position_tag {};

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^previous_position_tag) :])>
    using previous_position_t = internal::quantity_t<previous_position_tag, T, U>;
    using previous_position = previous_position_t<>;

    struct [[= internal::quantity_child<^^position_tag, internal::quantity_semantic_kind::absolute>]]
    render_position_tag {};

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^render_position_tag) :])>
    using render_position_t = internal::quantity_t<render_position_tag, T, U>;
    using render_position = render_position_t<>;

    struct [[= internal::quantity_child<^^position_tag, internal::quantity_semantic_kind::absolute>]]
    predicted_position_tag {};

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^predicted_position_tag) :])>
    using predicted_position_t = internal::quantity_t<predicted_position_tag, T, U>;
    using predicted_position = predicted_position_t<>;

    struct [[= internal::quantity_child<^^position_tag, internal::quantity_semantic_kind::absolute>]]
    target_position_tag {};

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^target_position_tag) :])>
    using target_position_t = internal::quantity_t<target_position_tag, T, U>;
    using target_position = target_position_t<>;

    struct [[= internal::quantity_child<^^displacement_tag, internal::quantity_semantic_kind::relative>]]
    offset_tag {};

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^offset_tag) :])>
    using offset_t = internal::quantity_t<offset_tag, T, U>;
    using offset = offset_t<>;

    struct [[= internal::quantity_child<^^displacement_tag, internal::quantity_semantic_kind::relative>]]
    lever_arm_tag {};

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^lever_arm_tag) :])>
    using lever_arm_t = internal::quantity_t<lever_arm_tag, T, U>;
    using lever_arm = lever_arm_t<>;

    struct [[= internal::quantity_child<^^displacement_tag, internal::quantity_semantic_kind::relative>]]
    gap_tag {};

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^gap_tag) :])>
    using gap_t = internal::quantity_t<gap_tag, T, U>;
    using gap = gap_t<>;

    struct [[= internal::quantity_child<^^gap_tag, internal::quantity_semantic_kind::relative>]]
    separation_tag {};

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^separation_tag) :])>
    using separation_t = internal::quantity_t<separation_tag, T, U>;
    using separation = separation_t<>;

    struct [[= internal::quantity_child<^^gap_tag, internal::quantity_semantic_kind::relative>]]
    penetration_tag {};

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^penetration_tag) :])>
    using penetration_t = internal::quantity_t<penetration_tag, T, U>;
    using penetration = penetration_t<>;
}

namespace gse::internal {
    template <> struct dimension_to_tag<dimi<1, 0, 0, 0>> { static constexpr bool found = true; using tag = gse::length_tag; };
    template <> struct dimension_to_tag<dimi<3, 0, 0, 0>> { static constexpr bool found = true; using tag = gse::volume_tag; };

    template <> struct quantity_units<gse::length_tag> {
        static constexpr auto units = std::tuple{
            gse::kilometers, gse::meters, gse::centimeters, gse::millimeters,
            gse::yards, gse::feet, gse::inches
        };
    };
}

export namespace gse {
    struct [[= internal::quantity_root<^^internal::dimi<0, 1, 0, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "ns">]]
    time_tag {};

    constexpr internal::unit<time_tag, std::ratio<1>, "ns"> nanoseconds;
    constexpr internal::unit<time_tag, std::kilo, "us"> microseconds;
    constexpr internal::unit<time_tag, std::mega, "ms"> milliseconds;
    constexpr internal::unit<time_tag, std::giga, "s"> seconds;
    constexpr internal::unit<time_tag, std::ratio<60'000'000'000>, "min"> minutes;
    constexpr internal::unit<time_tag, std::ratio<3'600'000'000'000>, "hr"> hours;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^time_tag) :])>
    using time_t = internal::quantity_t<time_tag, T, U>;
    using time = time_t<>;
}

namespace gse::internal {
    template <> struct dimension_to_tag<dimi<0, 1, 0, 0>> { static constexpr bool found = true; using tag = gse::time_tag; };

    template <> struct quantity_units<gse::time_tag> {
        static constexpr auto units = std::tuple{
            gse::nanoseconds, gse::microseconds, gse::milliseconds,
            gse::seconds, gse::minutes, gse::hours
        };
    };
}

export namespace gse {
    struct [[= internal::quantity_root<^^internal::dimi<0, 0, 1, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "kg">]]
    mass_tag {};

    constexpr internal::unit<mass_tag, std::ratio<1>, "kg"> kilograms;
    constexpr internal::unit<mass_tag, std::milli, "g"> grams;
    constexpr internal::unit<mass_tag, std::micro, "mg"> milligrams;
    constexpr internal::unit<mass_tag, std::ratio<45359237, 100000000>, "lb"> pounds;
    constexpr internal::unit<mass_tag, std::ratio<45359237, 1600000000>, "oz"> ounces;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^mass_tag) :])>
    using mass_t = internal::quantity_t<mass_tag, T, U>;
    using mass = mass_t<>;
}

namespace gse::internal {
    template <> struct dimension_to_tag<dimi<0, 0, 1, 0>> { static constexpr bool found = true; using tag = gse::mass_tag; };

    template <> struct quantity_units<gse::mass_tag> {
        static constexpr auto units = std::tuple{
            gse::kilograms, gse::grams, gse::milligrams, gse::pounds, gse::ounces
        };
    };
}

export namespace gse {
    struct [[= internal::quantity_root<^^internal::dimi<0, 0, -1, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "1/kg">]]
    inverse_mass_tag {};

    constexpr internal::unit<inverse_mass_tag, std::ratio<1>, "1/kg"> per_kilograms;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^inverse_mass_tag) :])>
    using inverse_mass_t = internal::quantity_t<inverse_mass_tag, T, U>;
    using inverse_mass = inverse_mass_t<>;

    struct [[= internal::quantity_child<^^inverse_mass_tag, internal::quantity_semantic_kind::measurement>]]
    linear_compliance_tag {};

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^linear_compliance_tag) :])>
    using linear_compliance_t = internal::quantity_t<linear_compliance_tag, T, U>;
    using linear_compliance = linear_compliance_t<>;
}

namespace gse::internal {
    template <> struct dimension_to_tag<dimi<0, 0, -1, 0>> { static constexpr bool found = true; using tag = gse::inverse_mass_tag; };
}

export namespace gse {
    struct [[= internal::quantity_root<^^internal::dimi<1, -2, 1, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "N">]]
    force_tag {};

    constexpr internal::unit<force_tag, std::ratio<1>, "N"> newtons;
    constexpr internal::unit<force_tag, std::ratio<222411, 50000>, "lbf"> pounds_force;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^force_tag) :])>
    using force_t = internal::quantity_t<force_tag, T, U>;
    using force = force_t<>;
}

namespace gse::internal {
    template <> struct dimension_to_tag<dimi<1, -2, 1, 0>> { static constexpr bool found = true; using tag = gse::force_tag; };

    template <> struct quantity_units<gse::force_tag> {
        static constexpr auto units = std::tuple{ gse::newtons, gse::pounds_force };
    };
}

export namespace gse {
    struct [[= internal::quantity_root<^^internal::dimi<1, -1, 1, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "N-s">]]
    impulse_tag {};

    constexpr internal::unit<impulse_tag, std::ratio<1>, "N-s"> newton_seconds;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^impulse_tag) :])>
    using impulse_t = internal::quantity_t<impulse_tag, T, U>;
    using impulse = impulse_t<>;
}

namespace gse::internal {
    template <> struct dimension_to_tag<dimi<1, -1, 1, 0>> { static constexpr bool found = true; using tag = gse::impulse_tag; };
}

export namespace gse {
    struct [[= internal::quantity_root<^^internal::dimi<2, -2, 1, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "J">]]
    energy_tag {};

    constexpr internal::unit<energy_tag, std::giga, "GJ"> gigajoules;
    constexpr internal::unit<energy_tag, std::mega, "MJ"> megajoules;
    constexpr internal::unit<energy_tag, std::kilo, "kJ"> kilojoules;
    constexpr internal::unit<energy_tag, std::ratio<1>, "J"> joules;
    constexpr internal::unit<energy_tag, std::ratio<4184>, "kcal"> kilocalories;
    constexpr internal::unit<energy_tag, std::ratio<523, 125>, "cal"> calories;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^energy_tag) :])>
    using energy_t = internal::quantity_t<energy_tag, T, U>;
    using energy = energy_t<>;
}

namespace gse::internal {
    template <> struct quantity_units<gse::energy_tag> {
        static constexpr auto units = std::tuple{
            gse::gigajoules, gse::megajoules, gse::kilojoules,
            gse::joules, gse::kilocalories, gse::calories
        };
    };
}

export namespace gse {
    struct [[= internal::quantity_root<^^internal::dimi<2, -2, 1, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "N-m">]]
    torque_tag {};

    constexpr internal::unit<torque_tag, std::ratio<1>, "N-m"> newton_meters;
    constexpr internal::unit<torque_tag, std::ratio<67791, 50000>, "lbf-ft"> pound_feet;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^torque_tag) :])>
    using torque_t = internal::quantity_t<torque_tag, T, U>;
    using torque = torque_t<>;
}

namespace gse::internal {
    template <> struct quantity_units<gse::torque_tag> {
        static constexpr auto units = std::tuple{ gse::newton_meters, gse::pound_feet };
    };
}

export namespace gse {
    struct [[= internal::quantity_root<^^internal::dimi<2, -1, 1, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "N-m-s">]]
    angular_impulse_tag {};

    constexpr internal::unit<angular_impulse_tag, std::ratio<1>, "N-m-s"> newton_meter_seconds;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^angular_impulse_tag) :])>
    using angular_impulse_t = internal::quantity_t<angular_impulse_tag, T, U>;
    using angular_impulse = angular_impulse_t<>;
}

namespace gse::internal {
    template <> struct dimension_to_tag<dimi<2, -1, 1, 0>> { static constexpr bool found = true; using tag = gse::angular_impulse_tag; };
}

export namespace gse {
    struct [[= internal::quantity_root<^^internal::dimi<2, -3, 1, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "W">]]
    power_tag {};

    constexpr internal::unit<power_tag, std::giga, "GW"> gigawatts;
    constexpr internal::unit<power_tag, std::mega, "MW"> megawatts;
    constexpr internal::unit<power_tag, std::kilo, "kW"> kilowatts;
    constexpr internal::unit<power_tag, std::ratio<1>, "W"> watts;
    constexpr internal::unit<power_tag, std::ratio<7457, 10>, "hp"> horsepower;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^power_tag) :])>
    using power_t = internal::quantity_t<power_tag, T, U>;
    using power = power_t<>;
}

namespace gse::internal {
    template <> struct dimension_to_tag<dimi<2, -3, 1, 0>> { static constexpr bool found = true; using tag = gse::power_tag; };

    template <> struct quantity_units<gse::power_tag> {
        static constexpr auto units = std::tuple{
            gse::gigawatts, gse::megawatts, gse::kilowatts,
            gse::watts, gse::horsepower
        };
    };
}

export namespace gse {
    struct [[= internal::quantity_root<^^internal::dimi<2, 0, 1, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "kg-m^2">]]
    inertia_tag {};

    constexpr internal::unit<inertia_tag, std::ratio<1>, "kg-m^2"> kilograms_meters_squared;
    constexpr internal::unit<inertia_tag, std::ratio<4214011, 100000000>, "lb-ft^2"> pounds_feet_squared;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^inertia_tag) :])>
    using inertia_t = internal::quantity_t<inertia_tag, T, U>;
    using inertia = inertia_t<>;
}

namespace gse::internal {
    template <> struct dimension_to_tag<dimi<2, 0, 1, 0>> { static constexpr bool found = true; using tag = gse::inertia_tag; };

    template <> struct quantity_units<gse::inertia_tag> {
        static constexpr auto units = std::tuple{ gse::kilograms_meters_squared, gse::pounds_feet_squared };
    };
}

export namespace gse {
    struct [[= internal::quantity_root<^^internal::dimi<1, -1, 0, 0>, internal::quantity_semantic_kind::relative, std::ratio<1>, "m/s">]]
    velocity_tag {};

    constexpr internal::unit<velocity_tag, std::ratio<1>, "m/s"> meters_per_second;
    constexpr internal::unit<velocity_tag, std::ratio<5, 18>, "km/h"> kilometers_per_hour;
    constexpr internal::unit<velocity_tag, std::ratio<1397, 3125>, "mph"> miles_per_hour;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^velocity_tag) :])>
    using velocity_t = internal::quantity_t<velocity_tag, T, U>;
    using velocity = velocity_t<>;

    struct [[= internal::quantity_sub_root<^^velocity_tag>]]
    normal_speed_tag {};

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^normal_speed_tag) :])>
    using normal_speed_t = internal::quantity_t<normal_speed_tag, T, U>;
    using normal_speed = normal_speed_t<>;

    struct [[= internal::quantity_child<^^normal_speed_tag, internal::quantity_semantic_kind::relative>]]
    closing_speed_tag {};

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^closing_speed_tag) :])>
    using closing_speed_t = internal::quantity_t<closing_speed_tag, T, U>;
    using closing_speed = closing_speed_t<>;
}

namespace gse::internal {
    template <> struct dimension_to_tag<dimi<1, -1, 0, 0>> { static constexpr bool found = true; using tag = gse::velocity_tag; };

    template <> struct quantity_units<gse::velocity_tag> {
        static constexpr auto units = std::tuple{
            gse::meters_per_second, gse::kilometers_per_hour, gse::miles_per_hour
        };
    };
}

export namespace gse {
    struct [[= internal::quantity_root<^^internal::dimi<1, -2, 0, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "m/s^2">]]
    acceleration_tag {};

    constexpr internal::unit<acceleration_tag, std::ratio<1>, "m/s^2"> meters_per_second_squared;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^acceleration_tag) :])>
    using acceleration_t = internal::quantity_t<acceleration_tag, T, U>;
    using acceleration = acceleration_t<>;
}

namespace gse::internal {
    template <> struct dimension_to_tag<dimi<1, -2, 0, 0>> { static constexpr bool found = true; using tag = gse::acceleration_tag; };
}

export namespace gse {
    struct [[= internal::quantity_root<^^internal::dimi<0, -1, 0, 1>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "rad/s">]]
    angular_velocity_tag {};

    constexpr internal::unit<angular_velocity_tag, std::ratio<1>, "rad/s"> radians_per_second;
    constexpr internal::unit<angular_velocity_tag, std::ratio_divide<pi_approx, std::ratio<180>>, "deg/s"> degrees_per_second;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^angular_velocity_tag) :])>
    using angular_velocity_t = internal::quantity_t<angular_velocity_tag, T, U>;
    using angular_velocity = angular_velocity_t<>;
}

namespace gse::internal {
    template <> struct dimension_to_tag<dimi<0, -1, 0, 1>> { static constexpr bool found = true; using tag = gse::angular_velocity_tag; };

    template <> struct quantity_units<gse::angular_velocity_tag> {
        static constexpr auto units = std::tuple{ gse::radians_per_second, gse::degrees_per_second };
    };
}

export namespace gse {
    struct [[= internal::quantity_root<^^internal::dimi<0, -2, 0, 1>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "rad/s^2">]]
    angular_acceleration_tag {};

    constexpr internal::unit<angular_acceleration_tag, std::ratio<1>, "rad/s^2"> radians_per_second_squared;
    constexpr internal::unit<angular_acceleration_tag, std::ratio_divide<pi_approx, std::ratio<180>>, "deg/s^2"> degrees_per_second_squared;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^angular_acceleration_tag) :])>
    using angular_acceleration_t = internal::quantity_t<angular_acceleration_tag, T, U>;
    using angular_acceleration = angular_acceleration_t<>;
}

namespace gse::internal {
    template <> struct dimension_to_tag<dimi<0, -2, 0, 1>> { static constexpr bool found = true; using tag = gse::angular_acceleration_tag; };

    template <> struct quantity_units<gse::angular_acceleration_tag> {
        static constexpr auto units = std::tuple{ gse::radians_per_second_squared, gse::degrees_per_second_squared };
    };
}

export namespace gse {
    struct [[= internal::quantity_root<^^internal::dimi<-3, 0, 1, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "kg/m^3">]]
    density_tag {};

    constexpr internal::unit<density_tag, std::ratio<1>, "kg/m^3"> kilograms_per_cubic_meter;
    constexpr internal::unit<density_tag, std::ratio<625, 2266>, "lb/ft^3"> pounds_per_cubic_foot;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^density_tag) :])>
    using density_t = internal::quantity_t<density_tag, T, U>;
    using density = density_t<>;
}

namespace gse::internal {
    template <> struct dimension_to_tag<dimi<-3, 0, 1, 0>> { static constexpr bool found = true; using tag = gse::density_tag; };

    template <> struct quantity_units<gse::density_tag> {
        static constexpr auto units = std::tuple{ gse::kilograms_per_cubic_meter, gse::pounds_per_cubic_foot };
    };
}

export namespace gse {
    struct [[= internal::quantity_root<^^internal::dimi<0, 2, 0, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "s^2">]]
    time_squared_tag {};

    constexpr internal::unit<time_squared_tag, std::ratio<1>, "s^2"> seconds_squared;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^time_squared_tag) :])>
    using time_squared_t = internal::quantity_t<time_squared_tag, T, U>;
    using time_squared = time_squared_t<>;
}

namespace gse::internal {
    template <> struct dimension_to_tag<dimi<0, 2, 0, 0>> { static constexpr bool found = true; using tag = gse::time_squared_tag; };
}

export namespace gse {
    struct [[= internal::quantity_root<^^internal::dimi<0, -2, 1, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "N/m">]]
    stiffness_tag {};

    constexpr internal::unit<stiffness_tag, std::ratio<1>, "N/m"> newtons_per_meter;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^stiffness_tag) :])>
    using stiffness_t = internal::quantity_t<stiffness_tag, T, U>;
    using stiffness = stiffness_t<>;
}

namespace gse::internal {
    template <> struct dimension_to_tag<dimi<0, -2, 1, 0>> { static constexpr bool found = true; using tag = gse::stiffness_tag; };
}

export namespace gse {
    struct [[= internal::quantity_root<^^internal::dimi<-2, 0, -1, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "1/(kg-m^2)">]]
    inverse_inertia_tag {};

    constexpr internal::unit<inverse_inertia_tag, std::ratio<1>, "1/(kg-m^2)"> per_kilogram_meter_squared;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^inverse_inertia_tag) :])>
    using inverse_inertia_t = internal::quantity_t<inverse_inertia_tag, T, U>;
    using inverse_inertia = inverse_inertia_t<>;

    struct [[= internal::quantity_child<^^inverse_inertia_tag, internal::quantity_semantic_kind::measurement>]]
    angular_compliance_tag {};

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^angular_compliance_tag) :])>
    using angular_compliance_t = internal::quantity_t<angular_compliance_tag, T, U>;
    using angular_compliance = angular_compliance_t<>;
}

namespace gse::internal {
    template <> struct dimension_to_tag<dimi<-2, 0, -1, 0>> { static constexpr bool found = true; using tag = gse::inverse_inertia_tag; };
}

export namespace gse {
    struct [[= internal::quantity_root<^^internal::dimi<2, 0, 0, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "m^2">]]
    area_tag {};

    constexpr internal::unit<area_tag, std::ratio<1>, "m^2"> square_meters;
    constexpr internal::unit<area_tag, std::ratio<1027639, 10000000>, "ft^2"> square_feet;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^area_tag) :])>
    using area_t = internal::quantity_t<area_tag, T, U>;
    using area = area_t<>;
}

namespace gse::internal {
    template <> struct dimension_to_tag<dimi<2, 0, 0, 0>> { static constexpr bool found = true; using tag = gse::area_tag; };

    template <> struct quantity_units<gse::area_tag> {
        static constexpr auto units = std::tuple{ gse::square_meters, gse::square_feet };
    };
}

export namespace gse {
    struct [[= internal::quantity_root<^^internal::dimi<2, -2, 1, -1>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "N-m/rad">]]
    angular_stiffness_tag {};

    constexpr internal::unit<angular_stiffness_tag, std::ratio<1>, "N-m/rad"> newton_meters_per_radian;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^angular_stiffness_tag) :])>
    using angular_stiffness_t = internal::quantity_t<angular_stiffness_tag, T, U>;
    using angular_stiffness = angular_stiffness_t<>;
}

namespace gse::internal {
    template <> struct dimension_to_tag<dimi<2, -2, 1, -1>> { static constexpr bool found = true; using tag = gse::angular_stiffness_tag; };
}

export namespace gse {
    struct [[= internal::quantity_root<^^internal::dimi<1, -2, 1, -1>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "N/rad">]]
    linear_angular_stiffness_tag {};

    constexpr internal::unit<linear_angular_stiffness_tag, std::ratio<1>, "N/rad"> newtons_per_radian;

    template <typename T = float, auto U = ([: internal::resolve_default_unit_info(^^linear_angular_stiffness_tag) :])>
    using linear_angular_stiffness_t = internal::quantity_t<linear_angular_stiffness_tag, T, U>;
    using linear_angular_stiffness = linear_angular_stiffness_t<>;
}

namespace gse::internal {
    template <> struct dimension_to_tag<dimi<1, -2, 1, -1>> { static constexpr bool found = true; using tag = gse::linear_angular_stiffness_tag; };
}

namespace gse::internal {
    static_assert(std::same_as<force::dimension, dimi<1, -2, 1, 0>>);
    static_assert(std::same_as<quantity_tag_traits<closing_speed_tag>::parent_tag, normal_speed_tag>);
    static_assert(quantity_tag_traits<position_tag>::semantic_kind == quantity_semantic_kind::absolute);
    static_assert(has_unit_list<time_tag>);
}

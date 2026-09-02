export module gse.math:units;

import std;

import :dimension;
import :quant;

export namespace gse::inline quantities {
	using pi_approx = std::ratio<355, 113>;

	struct [[= internal::quantity_root<^^internal::dimi<0, 0, 0, 1>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "rad">]] angle_tag {};

	constexpr internal::unit<angle_tag, std::ratio<1>, "rad"> radians;
	constexpr internal::unit<angle_tag, std::ratio_divide<pi_approx, std::ratio<180>>, "deg"> degrees;

	template <typename T = float, auto... U> using angle_t = internal::quantity_t<angle_tag, T, U...>;
	using angle = angle_t<>;

	constexpr angle rad = radians(1.f);

	struct [[= internal::quantity_root<^^internal::dimi<1, 0, 0, 0>, internal::quantity_semantic_kind::relative, std::ratio<1>, "m">]] length_tag {};

	constexpr internal::unit<length_tag, std::kilo, "km"> kilometers;
	constexpr internal::unit<length_tag, std::ratio<1>, "m"> meters;
	constexpr internal::unit<length_tag, std::centi, "cm"> centimeters;
	constexpr internal::unit<length_tag, std::milli, "mm"> millimeters;
	constexpr internal::unit<length_tag, std::ratio<1143, 1250>, "yd"> yards;
	constexpr internal::unit<length_tag, std::ratio<381, 1250>, "ft"> feet;
	constexpr internal::unit<length_tag, std::ratio<127, 5000>, "in"> inches;

	template <typename T = float, auto... U> using length_t = internal::quantity_t<length_tag, T, U...>;
	using length = length_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<2, 0, 0, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "m^2">]] area_tag {};

	constexpr internal::unit<area_tag, std::ratio<1>, "m^2"> square_meters;
	constexpr internal::unit<area_tag, std::ratio<1027639, 10000000>, "ft^2"> square_feet;

	template <typename T = float, auto... U> using area_t = internal::quantity_t<area_tag, T, U...>;
	using area = area_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<3, 0, 0, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "m^3">]] volume_tag {};

	constexpr internal::unit<volume_tag, std::ratio<1>, "m^3"> cubic_meters;

	template <typename T = float, auto... U> using volume_t = internal::quantity_t<volume_tag, T, U...>;
	using volume = volume_t<>;

	struct [[= internal::quantity_sub_root<^^length_tag>]] displacement_tag {};

	template <typename T = float, auto... U> using displacement_t = internal::quantity_t<displacement_tag, T, U...>;
	using displacement = displacement_t<>;

	struct [[= internal::quantity_absolute<^^length_tag, ^^displacement_tag>]] position_tag {};

	template <typename T = float, auto... U> using position_t = internal::quantity_t<position_tag, T, U...>;
	using position = position_t<>;

	struct [[= internal::quantity_child<^^position_tag, internal::quantity_semantic_kind::absolute>]] current_position_tag {};

	template <typename T = float, auto... U> using current_position_t = internal::quantity_t<current_position_tag, T, U...>;
	using current_position = current_position_t<>;

	struct [[= internal::quantity_child<^^position_tag, internal::quantity_semantic_kind::absolute>]] previous_position_tag {};

	template <typename T = float, auto... U> using previous_position_t = internal::quantity_t<previous_position_tag, T, U...>;
	using previous_position = previous_position_t<>;

	struct [[= internal::quantity_child<^^position_tag, internal::quantity_semantic_kind::absolute>]] render_position_tag {};

	template <typename T = float, auto... U> using render_position_t = internal::quantity_t<render_position_tag, T, U...>;
	using render_position = render_position_t<>;

	struct [[= internal::quantity_child<^^position_tag, internal::quantity_semantic_kind::absolute>]] predicted_position_tag {};

	template <typename T = float, auto... U> using predicted_position_t = internal::quantity_t<predicted_position_tag, T, U...>;
	using predicted_position = predicted_position_t<>;

	struct [[= internal::quantity_child<^^position_tag, internal::quantity_semantic_kind::absolute>]] target_position_tag {};

	template <typename T = float, auto... U> using target_position_t = internal::quantity_t<target_position_tag, T, U...>;
	using target_position = target_position_t<>;

	struct [[= internal::quantity_child<^^displacement_tag, internal::quantity_semantic_kind::relative>]] offset_tag {};

	template <typename T = float, auto... U> using offset_t = internal::quantity_t<offset_tag, T, U...>;
	using offset = offset_t<>;

	struct [[= internal::quantity_child<^^displacement_tag, internal::quantity_semantic_kind::relative>]] lever_arm_tag {};

	template <typename T = float, auto... U> using lever_arm_t = internal::quantity_t<lever_arm_tag, T, U...>;
	using lever_arm = lever_arm_t<>;

	struct [[= internal::quantity_child<^^displacement_tag, internal::quantity_semantic_kind::relative>]] gap_tag {};

	template <typename T = float, auto... U> using gap_t = internal::quantity_t<gap_tag, T, U...>;
	using gap = gap_t<>;

	struct [[= internal::quantity_child<^^gap_tag, internal::quantity_semantic_kind::relative>]] separation_tag {};

	template <typename T = float, auto... U> using separation_t = internal::quantity_t<separation_tag, T, U...>;
	using separation = separation_t<>;

	struct [[= internal::quantity_child<^^gap_tag, internal::quantity_semantic_kind::relative>]] penetration_tag {};

	template <typename T = float, auto... U> using penetration_t = internal::quantity_t<penetration_tag, T, U...>;
	using penetration = penetration_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<-1, 0, 0, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "1/m">]] inverse_length_tag {};

	constexpr internal::unit<inverse_length_tag, std::ratio<1>, "1/m"> per_meter;
	constexpr internal::unit<inverse_length_tag, std::milli, "1/km"> per_kilometer;

	template <typename T = float, auto... U> using inverse_length_t = internal::quantity_t<inverse_length_tag, T, U...>;
	using inverse_length = inverse_length_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<0, 1, 0, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "ns">]] time_tag {};

	constexpr internal::unit<time_tag, std::ratio<1>, "ns"> nanoseconds;
	constexpr internal::unit<time_tag, std::kilo, "us"> microseconds;
	constexpr internal::unit<time_tag, std::mega, "ms"> milliseconds;
	constexpr internal::unit<time_tag, std::giga, "s"> seconds;
	constexpr internal::unit<time_tag, std::ratio<60'000'000'000>, "min"> minutes;
	constexpr internal::unit<time_tag, std::ratio<3'600'000'000'000>, "hr"> hours;

	template <typename T = float, auto... U> using time_t = internal::quantity_t<time_tag, T, U...>;
	using time = time_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<0, 2, 0, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "s^2">]] time_squared_tag {};

	constexpr internal::unit<time_squared_tag, std::ratio<1>, "s^2"> seconds_squared;

	template <typename T = float, auto... U> using time_squared_t = internal::quantity_t<time_squared_tag, T, U...>;
	using time_squared = time_squared_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<0, -1, 0, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "1/s">]] inverse_time_tag {};

	constexpr internal::unit<inverse_time_tag, std::ratio<1>, "1/s"> per_second;

	template <typename T = float, auto... U> using inverse_time_t = internal::quantity_t<inverse_time_tag, T, U...>;
	using inverse_time = inverse_time_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<0, 0, 1, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "kg">]] mass_tag {};

	constexpr internal::unit<mass_tag, std::ratio<1>, "kg"> kilograms;
	constexpr internal::unit<mass_tag, std::milli, "g"> grams;
	constexpr internal::unit<mass_tag, std::micro, "mg"> milligrams;
	constexpr internal::unit<mass_tag, std::ratio<45359237, 100000000>, "lb"> pounds;
	constexpr internal::unit<mass_tag, std::ratio<45359237, 1600000000>, "oz"> ounces;

	template <typename T = float, auto... U> using mass_t = internal::quantity_t<mass_tag, T, U...>;
	using mass = mass_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<0, 0, -1, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "1/kg">]] inverse_mass_tag {};

	constexpr internal::unit<inverse_mass_tag, std::ratio<1>, "1/kg"> per_kilograms;

	template <typename T = float, auto... U> using inverse_mass_t = internal::quantity_t<inverse_mass_tag, T, U...>;
	using inverse_mass = inverse_mass_t<>;

	struct [[= internal::quantity_child<^^inverse_mass_tag, internal::quantity_semantic_kind::measurement>]] linear_compliance_tag {};

	template <typename T = float, auto... U> using linear_compliance_t = internal::quantity_t<linear_compliance_tag, T, U...>;
	using linear_compliance = linear_compliance_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<2, 0, 1, -2>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "kg-m^2">]] inertia_tag {};

	constexpr internal::unit<inertia_tag, std::ratio<1>, "kg-m^2"> kilograms_meters_squared;
	constexpr internal::unit<inertia_tag, std::ratio<4214011, 100000000>, "lb-ft^2"> pounds_feet_squared;

	template <typename T = float, auto... U> using inertia_t = internal::quantity_t<inertia_tag, T, U...>;
	using inertia = inertia_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<-2, 0, -1, 2>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "1/(kg-m^2)">]] inverse_inertia_tag {};

	constexpr internal::unit<inverse_inertia_tag, std::ratio<1>, "1/(kg-m^2)"> per_kilogram_meter_squared;

	template <typename T = float, auto... U> using inverse_inertia_t = internal::quantity_t<inverse_inertia_tag, T, U...>;
	using inverse_inertia = inverse_inertia_t<>;

	struct [[= internal::quantity_child<^^inverse_inertia_tag, internal::quantity_semantic_kind::measurement>]] angular_compliance_tag {};

	template <typename T = float, auto... U> using angular_compliance_t = internal::quantity_t<angular_compliance_tag, T, U...>;
	using angular_compliance = angular_compliance_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<-3, 0, 1, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "kg/m^3">]] density_tag {};

	constexpr internal::unit<density_tag, std::ratio<1>, "kg/m^3"> kilograms_per_cubic_meter;
	constexpr internal::unit<density_tag, std::ratio<625, 2266>, "lb/ft^3"> pounds_per_cubic_foot;

	template <typename T = float, auto... U> using density_t = internal::quantity_t<density_tag, T, U...>;
	using density = density_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<1, -1, 0, 0>, internal::quantity_semantic_kind::relative, std::ratio<1>, "m/s">]] velocity_tag {};

	constexpr internal::unit<velocity_tag, std::ratio<1>, "m/s"> meters_per_second;
	constexpr internal::unit<velocity_tag, std::ratio<5, 18>, "km/h"> kilometers_per_hour;
	constexpr internal::unit<velocity_tag, std::ratio<1397, 3125>, "mph"> miles_per_hour;

	template <typename T = float, auto... U> using velocity_t = internal::quantity_t<velocity_tag, T, U...>;
	using velocity = velocity_t<>;

	struct [[= internal::quantity_sub_root<^^velocity_tag>]] normal_speed_tag {};

	template <typename T = float, auto... U> using normal_speed_t = internal::quantity_t<normal_speed_tag, T, U...>;
	using normal_speed = normal_speed_t<>;

	struct [[= internal::quantity_child<^^normal_speed_tag, internal::quantity_semantic_kind::relative>]] closing_speed_tag {};

	template <typename T = float, auto... U> using closing_speed_t = internal::quantity_t<closing_speed_tag, T, U...>;
	using closing_speed = closing_speed_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<1, -2, 0, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "m/s^2">]] acceleration_tag {};

	constexpr internal::unit<acceleration_tag, std::ratio<1>, "m/s^2"> meters_per_second_squared;

	template <typename T = float, auto... U> using acceleration_t = internal::quantity_t<acceleration_tag, T, U...>;
	using acceleration = acceleration_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<0, -1, 0, 1>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "rad/s">]] angular_velocity_tag {};

	constexpr internal::unit<angular_velocity_tag, std::ratio<1>, "rad/s"> radians_per_second;
	constexpr internal::unit<angular_velocity_tag, std::ratio_divide<pi_approx, std::ratio<180>>, "deg/s"> degrees_per_second;

	template <typename T = float, auto... U> using angular_velocity_t = internal::quantity_t<angular_velocity_tag, T, U...>;
	using angular_velocity = angular_velocity_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<0, -2, 0, 1>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "rad/s^2">]] angular_acceleration_tag {};

	constexpr internal::unit<angular_acceleration_tag, std::ratio<1>, "rad/s^2"> radians_per_second_squared;
	constexpr internal::unit<angular_acceleration_tag, std::ratio_divide<pi_approx, std::ratio<180>>, "deg/s^2"> degrees_per_second_squared;

	template <typename T = float, auto... U> using angular_acceleration_t = internal::quantity_t<angular_acceleration_tag, T, U...>;
	using angular_acceleration = angular_acceleration_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<1, -2, 1, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "N">]] force_tag {};

	constexpr internal::unit<force_tag, std::ratio<1>, "N"> newtons;
	constexpr internal::unit<force_tag, std::ratio<222411, 50000>, "lbf"> pounds_force;

	template <typename T = float, auto... U> using force_t = internal::quantity_t<force_tag, T, U...>;
	using force = force_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<1, -1, 1, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "N-s">]] impulse_tag {};

	constexpr internal::unit<impulse_tag, std::ratio<1>, "N-s"> newton_seconds;

	template <typename T = float, auto... U> using impulse_t = internal::quantity_t<impulse_tag, T, U...>;
	using impulse = impulse_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<2, -2, 1, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "J">]] energy_tag {};

	constexpr internal::unit<energy_tag, std::giga, "GJ"> gigajoules;
	constexpr internal::unit<energy_tag, std::mega, "MJ"> megajoules;
	constexpr internal::unit<energy_tag, std::kilo, "kJ"> kilojoules;
	constexpr internal::unit<energy_tag, std::ratio<1>, "J"> joules;
	constexpr internal::unit<energy_tag, std::ratio<4184>, "kcal"> kilocalories;
	constexpr internal::unit<energy_tag, std::ratio<523, 125>, "cal"> calories;

	template <typename T = float, auto... U> using energy_t = internal::quantity_t<energy_tag, T, U...>;
	using energy = energy_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<2, -2, 1, -1>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "N-m">]] torque_tag {};

	constexpr internal::unit<torque_tag, std::ratio<1>, "N-m"> newton_meters;
	constexpr internal::unit<torque_tag, std::ratio<67791, 50000>, "lbf-ft"> pound_feet;

	template <typename T = float, auto... U> using torque_t = internal::quantity_t<torque_tag, T, U...>;
	using torque = torque_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<2, -1, 1, -1>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "N-m-s">]] angular_impulse_tag {};

	constexpr internal::unit<angular_impulse_tag, std::ratio<1>, "N-m-s"> newton_meter_seconds;

	template <typename T = float, auto... U> using angular_impulse_t = internal::quantity_t<angular_impulse_tag, T, U...>;
	using angular_impulse = angular_impulse_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<2, -3, 1, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "W">]] power_tag {};

	constexpr internal::unit<power_tag, std::giga, "GW"> gigawatts;
	constexpr internal::unit<power_tag, std::mega, "MW"> megawatts;
	constexpr internal::unit<power_tag, std::kilo, "kW"> kilowatts;
	constexpr internal::unit<power_tag, std::ratio<1>, "W"> watts;
	constexpr internal::unit<power_tag, std::ratio<7457, 10>, "hp"> horsepower;

	template <typename T = float, auto... U> using power_t = internal::quantity_t<power_tag, T, U...>;
	using power = power_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<0, -3, 1, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "W/m^2">]] irradiance_tag {};

	constexpr internal::unit<irradiance_tag, std::ratio<1>, "W/m^2"> watts_per_square_meter;

	template <typename T = float, auto... U> using irradiance_t = internal::quantity_t<irradiance_tag, T, U...>;
	using irradiance = irradiance_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<0, -2, 1, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "N/m">]] stiffness_tag {};

	constexpr internal::unit<stiffness_tag, std::ratio<1>, "N/m"> newtons_per_meter;

	template <typename T = float, auto... U> using stiffness_t = internal::quantity_t<stiffness_tag, T, U...>;
	using stiffness = stiffness_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<-1, -2, 1, 0>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "N/m^2">]] stiffness_per_length_tag {};

	constexpr internal::unit<stiffness_per_length_tag, std::ratio<1>, "N/m^2"> newtons_per_meter_squared;

	template <typename T = float, auto... U> using stiffness_per_length_t = internal::quantity_t<stiffness_per_length_tag, T, U...>;
	using stiffness_per_length = stiffness_per_length_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<2, -2, 1, -2>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "N-m/rad">]] angular_stiffness_tag {};

	constexpr internal::unit<angular_stiffness_tag, std::ratio<1>, "N-m/rad"> newton_meters_per_radian;

	template <typename T = float, auto... U> using angular_stiffness_t = internal::quantity_t<angular_stiffness_tag, T, U...>;
	using angular_stiffness = angular_stiffness_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<1, 0, 0, -1>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "m/rad">]] angular_jacobian_tag {};

	constexpr internal::unit<angular_jacobian_tag, std::ratio<1>, "m/rad"> meters_per_radian;

	template <typename T = float, auto... U> using angular_jacobian_t = internal::quantity_t<angular_jacobian_tag, T, U...>;
	using angular_jacobian = angular_jacobian_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<1, -2, 1, -1>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "N/rad">]] linear_angular_stiffness_tag {};

	constexpr internal::unit<linear_angular_stiffness_tag, std::ratio<1>, "N/rad"> newtons_per_radian;

	template <typename T = float, auto... U> using linear_angular_stiffness_t = internal::quantity_t<linear_angular_stiffness_tag, T, U...>;
	using linear_angular_stiffness = linear_angular_stiffness_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<2, -2, 1, -3>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "N-m/rad^2">]] angular_stiffness_per_angle_tag {};

	constexpr internal::unit<angular_stiffness_per_angle_tag, std::ratio<1>, "N-m/rad^2"> newton_meters_per_radian_squared;

	template <typename T = float, auto... U> using angular_stiffness_per_angle_t = internal::quantity_t<angular_stiffness_per_angle_tag, T, U...>;
	using angular_stiffness_per_angle = angular_stiffness_per_angle_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<0, 0, 0, 0, 1>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "b">]] data_size_tag {};

	constexpr internal::unit<data_size_tag, std::ratio<1>, "b"> bits;
	constexpr internal::unit<data_size_tag, std::kilo, "kb"> kilobits;
	constexpr internal::unit<data_size_tag, std::mega, "Mb"> megabits;
	constexpr internal::unit<data_size_tag, std::giga, "Gb"> gigabits;
	constexpr internal::unit<data_size_tag, std::ratio<8>, "B"> bytes;
	constexpr internal::unit<data_size_tag, std::ratio<8'000>, "kB"> kilobytes;
	constexpr internal::unit<data_size_tag, std::ratio<8'000'000>, "MB"> megabytes;
	constexpr internal::unit<data_size_tag, std::ratio<8'000'000'000>, "GB"> gigabytes;

	template <typename T = float, auto... U> using data_size_t = internal::quantity_t<data_size_tag, T, U...>;
	using data_size = data_size_t<>;

	struct [[= internal::quantity_root<^^internal::dimi<0, -1, 0, 0, 1>, internal::quantity_semantic_kind::measurement, std::ratio<1>, "b/s">]] bitrate_tag {};

	constexpr internal::unit<bitrate_tag, std::ratio<1>, "b/s"> bits_per_second;
	constexpr internal::unit<bitrate_tag, std::kilo, "kb/s"> kilobits_per_second;
	constexpr internal::unit<bitrate_tag, std::mega, "Mb/s"> megabits_per_second;
	constexpr internal::unit<bitrate_tag, std::giga, "Gb/s"> gigabits_per_second;
	constexpr internal::unit<bitrate_tag, std::ratio<8'000'000>, "MB/s"> megabytes_per_second;

	template <typename T = float, auto... U> using bitrate_t = internal::quantity_t<bitrate_tag, T, U...>;
	using bitrate = bitrate_t<>;
}

namespace gse::internal {
	template <>
	struct canonical_tag_list<void> {
		using type = scanned_tag_list<^^gse::quantities>;
	};

	template <>
	struct unit_registry<void> {
		using type = scanned_unit_set<^^gse::quantities>;
	};

	template <>
	struct base_unit_override<time_tag> {
		using type = std::remove_cvref_t<decltype(seconds)>;
	};
}

export namespace gse {
	using inverse_area = decltype(std::declval<inverse_length>() * std::declval<inverse_length>());

	template <internal::is_quantity Q>
	requires internal::has_same_dimensions<typename Q::dimension, typename angle::dimension>
	constexpr auto sin(const Q& a) -> typename Q::value_type {
		return std::sin(internal::value_in<angle::default_unit>(a));
	}

	template <internal::is_quantity Q>
	requires internal::has_same_dimensions<typename Q::dimension, typename angle::dimension>
	constexpr auto cos(const Q& a) -> typename Q::value_type {
		return std::cos(internal::value_in<angle::default_unit>(a));
	}

	template <internal::is_quantity Q>
	requires internal::has_same_dimensions<typename Q::dimension, typename angle::dimension>
	constexpr auto tan(const Q& a) -> typename Q::value_type {
		return std::tan(internal::value_in<angle::default_unit>(a));
	}

	template <std::floating_point T>
	constexpr auto asin(const T v) -> angle_t<T> {
		return radians(std::asin(v));
	}

	template <internal::is_quantity Q>
	requires internal::has_same_dimensions<typename Q::dimension, internal::dimensionless>
	constexpr auto asin(const Q& q) -> angle_t<typename Q::value_type> {
		return radians(std::asin(internal::value_in<typename Q::default_unit>(q)));
	}

	template <std::floating_point T>
	constexpr auto acos(const T v) -> angle_t<T> {
		return radians(std::acos(v));
	}

	template <internal::is_quantity Q>
	requires internal::has_same_dimensions<typename Q::dimension, internal::dimensionless>
	constexpr auto acos(const Q& q) -> angle_t<typename Q::value_type> {
		return radians(std::acos(internal::value_in<typename Q::default_unit>(q)));
	}

	template <std::floating_point T>
	constexpr auto atan(const T v) -> angle_t<T> {
		return radians(std::atan(v));
	}

	template <internal::is_quantity Q>
	requires internal::has_same_dimensions<typename Q::dimension, internal::dimensionless>
	constexpr auto atan(const Q& q) -> angle_t<typename Q::value_type> {
		return radians(std::atan(internal::value_in<typename Q::default_unit>(q)));
	}

	template <std::floating_point T>
	constexpr auto atan2(const T y, const T x) -> angle_t<T> {
		return radians(std::atan2(y, x));
	}

	template <internal::is_quantity Q1, internal::is_quantity Q2>
	requires internal::has_same_dimension_as<Q1, Q2>
	constexpr auto atan2(const Q1& y, const Q2& x) -> angle_t<std::common_type_t<typename Q1::value_type, typename Q2::value_type>> {
		using common_type = std::common_type_t<typename Q1::value_type, typename Q2::value_type>;
		using reference_unit = internal::quantity_base_unit_t<Q1>;
		return radians(
			std::atan2(
				static_cast<common_type>(internal::value_in<reference_unit>(y)),
				static_cast<common_type>(internal::value_in<reference_unit>(x))
			)
		);
	}
}

namespace gse::internal {
	static_assert(
		std::same_as<force::dimension, dimi<1, -2, 1, 0>>
	);
	static_assert(
		std::same_as<quantity_tag_traits<closing_speed_tag>::parent_tag, normal_speed_tag>
	);
	static_assert(
		quantity_tag_traits<position_tag>::semantic_kind == quantity_semantic_kind::absolute
	);
	static_assert(
		dimension_to_tag<dimi<1, 0, 0, 0>>::match_count == 1 && std::same_as<dimension_to_tag<dimi<1, 0, 0, 0>>::tag, length_tag>
	);
	static_assert(
		dimension_to_tag<dimi<2, -2, 1, 0>>::match_count == 1 && std::same_as<dimension_to_tag<dimi<2, -2, 1, 0>>::tag, energy_tag>
	);
	static_assert(
		std::same_as<decltype(std::declval<torque>() * std::declval<angle>()), energy>
	);
	static_assert(
		std::same_as<decltype(std::declval<inertia>() * std::declval<angular_acceleration>()), torque>
	);
	static_assert(
		std::same_as<decltype(std::declval<inertia>() * std::declval<angular_velocity>()), angular_impulse>
	);
	static_assert(
		std::same_as<decltype(std::declval<inverse_inertia>() * std::declval<torque>()), angular_acceleration>
	);
	static_assert(
		std::same_as<decltype(std::declval<torque>() * std::declval<angular_velocity>()), power>
	);
	static_assert(
		std::same_as<decltype(std::declval<force>() * std::declval<length>()), energy>
	);
	static_assert(
		std::same_as<decltype(std::declval<force>() / std::declval<displacement>()), stiffness>
	);
	static_assert(
		std::same_as<decltype(std::declval<force>() / std::declval<angle>()), linear_angular_stiffness>
	);
	static_assert(
		std::same_as<decltype(std::declval<torque>() / std::declval<displacement>()), linear_angular_stiffness>
	);
	static_assert(
		std::same_as<decltype(std::declval<torque>() / std::declval<angle>()), angular_stiffness>
	);
	static_assert(
		std::same_as<decltype(std::declval<length>() / std::declval<angle>()), angular_jacobian>
	);
	static_assert(
		has_unit_list<time_tag>
	);
	static_assert(
		has_unit_list<volume_tag>
	);
	static_assert(
		std::same_as<unit_list_t<force_tag>, unit_set<std::remove_cvref_t<decltype(newtons)>, std::remove_cvref_t<decltype(pounds_force)>>>
	);
}
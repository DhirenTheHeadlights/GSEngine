#pragma once

#include "Physics/Units/UnitTemplate.h"

// Velocity

namespace gse::units {
	struct velocity_tag {};

	constexpr char meters_per_second_units[] = "m/s";
	constexpr char kilometers_per_hour_units[] = "km/h";
	constexpr char miles_per_hour_units[] = "mph";
	constexpr char feet_per_second_units[] = "ft/s";

	using meters_per_second = unit<velocity_tag, 1.0f, meters_per_second_units>;
	using kilometers_per_hour = unit<velocity_tag, 0.27778f, kilometers_per_hour_units>;
	using miles_per_hour = unit<velocity_tag, 0.44704f, miles_per_hour_units>;
	using feet_per_second = unit<velocity_tag, 0.30480f, feet_per_second_units>;
}

namespace gse {
	using velocity_units = unit_list<
		units::meters_per_second,
		units::kilometers_per_hour,
		units::miles_per_hour,
		units::feet_per_second
	>;

	struct velocity : quantity<velocity, units::meters_per_second, velocity_units> {
		using quantity::quantity;
	};

	inline velocity meters_per_second(const float value) {
		return velocity::from<units::meters_per_second>(value);
	}

	inline velocity kilometers_per_hour(const float value) {
		return velocity::from<units::kilometers_per_hour>(value);
	}

	inline velocity miles_per_hour(const float value) {
		return velocity::from<units::miles_per_hour>(value);
	}

	inline velocity feet_per_second(const float value) {
		return velocity::from<units::feet_per_second>(value);
	}
}

// Acceleration

namespace gse::units {
	struct acceleration_tag {};

	constexpr char meters_pr_second_squared_units[] = "m/s^2";
	constexpr char kilometers_pr_hour_squared_units[] = "km/h^2";
	constexpr char miles_pr_hour_squared_units[] = "mph^2";
	constexpr char feet_pr_second_squared_units[] = "ft/s^2";

	using meters_per_second_squared = unit<acceleration_tag, 1.0f, meters_pr_second_squared_units>;
	using kilometers_per_hour_squared = unit<acceleration_tag, 0.00007716049382716f, kilometers_pr_hour_squared_units>;
	using miles_per_hour_squared = unit<acceleration_tag, 0.000124539617f, miles_pr_hour_squared_units>;
	using feet_per_second_squared = unit<acceleration_tag, 0.092903f, feet_pr_second_squared_units>;
}

namespace gse {
	using acceleration_units = unit_list<
		units::meters_per_second_squared,
		units::kilometers_per_hour_squared,
		units::miles_per_hour_squared,
		units::feet_per_second_squared
	>;

	struct acceleration : quantity<acceleration, units::meters_per_second_squared, acceleration_units> {
		using quantity::quantity;
	};

	inline acceleration meters_per_second_squared(const float value) {
		return acceleration::from<units::meters_per_second_squared>(value);
	}

	inline acceleration kilometers_per_hour_squared(const float value) {
		return acceleration::from<units::kilometers_per_hour_squared>(value);
	}

	inline acceleration miles_per_hour_squared(const float value) {
		return acceleration::from<units::miles_per_hour_squared>(value);
	}

	inline acceleration feet_per_second_squared(const float value) {
		return acceleration::from<units::feet_per_second_squared>(value);
	}
}
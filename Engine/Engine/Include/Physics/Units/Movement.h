#pragma once

#include "Physics/Units/UnitTemplate.h"

// Velocity

namespace gse {
	struct velocity_tag {};

	constexpr char meters_per_second_units[] = "m/s";
	constexpr char kilometers_per_hour_units[] = "km/h";
	constexpr char miles_per_hour_units[] = "mph";
	constexpr char feet_per_second_units[] = "ft/s";

	struct meters_per_second : unit<velocity_tag, 1.0f, meters_per_second_units> {};
	struct kilometers_per_hour : unit<velocity_tag, 0.277778f, kilometers_per_hour_units> {};
	struct miles_per_hour : unit<velocity_tag, 0.44704f, miles_per_hour_units> {};
	struct feet_per_second : unit<velocity_tag, 0.3048f, feet_per_second_units> {};
}

namespace gse {
	using velocity_units = unit_list<
		meters_per_second,
		kilometers_per_hour,
		miles_per_hour,
		feet_per_second
	>;

	struct velocity : quantity<velocity, meters_per_second, velocity_units> {
		using quantity::quantity;
	};

	inline velocity meters_per_second(const float value) {
		return velocity::from<struct meters_per_second>(value);
	}

	inline velocity kilometers_per_hour(const float value) {
		return velocity::from<struct kilometers_per_hour>(value);
	}

	inline velocity miles_per_hour(const float value) {
		return velocity::from<struct miles_per_hour>(value);
	}

	inline velocity feet_per_second(const float value) {
		return velocity::from<struct feet_per_second>(value);
	}
}

// Acceleration

namespace gse {
	struct acceleration_tag {};

	constexpr char meters_pr_second_squared_units[] = "m/s^2";
	constexpr char kilometers_pr_hour_squared_units[] = "km/h^2";
	constexpr char miles_pr_hour_squared_units[] = "mph^2";
	constexpr char feet_pr_second_squared_units[] = "ft/s^2";

	struct meters_per_second_squared : unit<acceleration_tag, 1.0f, meters_pr_second_squared_units> {};
	struct kilometers_per_hour_squared : unit<acceleration_tag, 0.00007716f, kilometers_pr_hour_squared_units> {};
	struct miles_per_hour_squared : unit<acceleration_tag, 0.00012451f, miles_pr_hour_squared_units> {};
	struct feet_per_second_squared : unit<acceleration_tag, 0.3048f, feet_pr_second_squared_units> {};
}

namespace gse {
	using acceleration_units = unit_list<
		meters_per_second_squared,
		kilometers_per_hour_squared,
		miles_per_hour_squared,
		feet_per_second_squared
	>;

	struct acceleration : quantity<acceleration, meters_per_second_squared, acceleration_units> {
		using quantity::quantity;
	};

	inline acceleration meters_per_second_squared(const float value) {
		return acceleration::from<struct meters_per_second_squared>(value);
	}

	inline acceleration kilometers_per_hour_squared(const float value) {
		return acceleration::from<struct kilometers_per_hour_squared>(value);
	}

	inline acceleration miles_per_hour_squared(const float value) {
		return acceleration::from<struct miles_per_hour_squared>(value);
	}

	inline acceleration feet_per_second_squared(const float value) {
		return acceleration::from<struct feet_per_second_squared>(value);
	}
}
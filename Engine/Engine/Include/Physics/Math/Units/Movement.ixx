export module gse.physics.math.units.movement;

import gse.physics.math.units.quantity;

// Velocity

export namespace gse::units {
	struct velocity_tag {};

	constexpr char meters_per_second_units[] = "m/s";
	constexpr char kilometers_per_hour_units[] = "km/h";
	constexpr char miles_per_hour_units[] = "mph";
	constexpr char feet_per_second_units[] = "ft/s";

	using meters_per_second = unit<velocity_tag, 1.0f, meters_per_second_units>;
	using kilometers_per_hour = unit<velocity_tag, 0.27778f, kilometers_per_hour_units>;
	using miles_per_hour = unit<velocity_tag, 0.44704f, miles_per_hour_units>;
	using feet_per_second = unit<velocity_tag, 0.30480f, feet_per_second_units>;

	struct angular_velocity_tag {};

	constexpr char radians_per_second_units[] = "rad/s";
	constexpr char degrees_per_second_units[] = "deg/s";

	using radians_per_second = unit<angular_velocity_tag, 1.0f, radians_per_second_units>;
	using degrees_per_second = unit<angular_velocity_tag, 0.0174533f, degrees_per_second_units>;
}

export namespace gse {
	using velocity_units = unit_list<
		units::meters_per_second,
		units::kilometers_per_hour,
		units::miles_per_hour,
		units::feet_per_second
	>;

	struct velocity : quantity<velocity, units::meters_per_second, velocity_units> {
		using quantity::quantity;
	};

	inline auto meters_per_second(const float value) -> velocity {
		return velocity::from<units::meters_per_second>(value);
	}

	inline auto kilometers_per_hour(const float value) -> velocity {
		return velocity::from<units::kilometers_per_hour>(value);
	}

	inline auto miles_per_hour(const float value) -> velocity {
		return velocity::from<units::miles_per_hour>(value);
	}

	inline auto feet_per_second(const float value) -> velocity {
		return velocity::from<units::feet_per_second>(value);
	}

	using angular_velocity_units = unit_list <
		units::radians_per_second,
		units::degrees_per_second
	>;

	struct angular_velocity : quantity<angular_velocity, units::radians_per_second, angular_velocity_units> {
		using quantity::quantity;
	};

	inline auto radians_per_second(const float value) -> angular_velocity {
		return angular_velocity::from<units::radians_per_second>(value);
	}

	inline auto degrees_per_second(const float value) -> angular_velocity {
		return angular_velocity::from<units::degrees_per_second>(value);
	}
}

// Acceleration

export namespace gse::units {
	struct acceleration_tag {};

	constexpr char meters_pr_second_squared_units[] = "m/s^2";
	constexpr char kilometers_pr_hour_squared_units[] = "km/h^2";
	constexpr char miles_pr_hour_squared_units[] = "mph^2";
	constexpr char feet_pr_second_squared_units[] = "ft/s^2";

	using meters_per_second_squared = unit<acceleration_tag, 1.0f, meters_pr_second_squared_units>;
	using kilometers_per_hour_squared = unit<acceleration_tag, 0.00007716049382716f, kilometers_pr_hour_squared_units>;
	using miles_per_hour_squared = unit<acceleration_tag, 0.000124539617f, miles_pr_hour_squared_units>;
	using feet_per_second_squared = unit<acceleration_tag, 0.092903f, feet_pr_second_squared_units>;

	struct angular_acceleration_tag {};

	constexpr char radians_per_second_squared_units[] = "rad/s^2";
	constexpr char degrees_per_second_squared_units[] = "deg/s^2";

	using radians_per_second_squared = unit<angular_acceleration_tag, 1.0f, radians_per_second_squared_units>;
	using degrees_per_second_squared = unit<angular_acceleration_tag, 0.0174533f, degrees_per_second_squared_units>;
}

export namespace gse {
	using acceleration_units = unit_list<
		units::meters_per_second_squared,
		units::kilometers_per_hour_squared,
		units::miles_per_hour_squared,
		units::feet_per_second_squared
	>;

	struct acceleration : quantity<acceleration, units::meters_per_second_squared, acceleration_units> {
		using quantity::quantity;
	};

	inline auto meters_per_second_squared(const float value) -> acceleration {
		return acceleration::from<units::meters_per_second_squared>(value);
	}

	inline auto kilometers_per_hour_squared(const float value) -> acceleration {
		return acceleration::from<units::kilometers_per_hour_squared>(value);
	}

	inline auto miles_per_hour_squared(const float value) -> acceleration {
		return acceleration::from<units::miles_per_hour_squared>(value);
	}

	inline auto feet_per_second_squared(const float value) -> acceleration {
		return acceleration::from<units::feet_per_second_squared>(value);
	}

	using angular_acceleration_units = unit_list <
		units::radians_per_second_squared,
		units::degrees_per_second_squared
	>;

	struct angular_acceleration : quantity<angular_acceleration, units::radians_per_second_squared, angular_acceleration_units> {
		using quantity::quantity;
	};

	inline auto radians_per_second_squared(const float value) -> angular_acceleration {
		return angular_acceleration::from<units::radians_per_second_squared>(value);
	}

	inline auto degrees_per_second_squared(const float value) -> angular_acceleration {
		return angular_acceleration::from<units::degrees_per_second_squared>(value);
	}
}
export module gse.physics.math.units.angle;

import gse.physics.math.units.quantity;

export namespace gse::units {
	constexpr char degrees_units[] = "deg";
	constexpr char radians_units[] = "rad";

	using degrees = unit<float, 1.0f, degrees_units>;
	using radians = unit<float, 1 / 0.01745329252f, radians_units>;
}

export namespace gse {
	using angle_units = unit_list<
		units::degrees,
		units::radians
	>;

	struct angle : quantity<angle, units::degrees, angle_units> {
		using quantity::quantity;
	};

	inline auto degrees(const float value) -> angle {
		return angle::from<units::degrees>(value);
	}

	inline auto radians(const float value) -> angle {
		return angle::from<units::radians>(value);
	}
}
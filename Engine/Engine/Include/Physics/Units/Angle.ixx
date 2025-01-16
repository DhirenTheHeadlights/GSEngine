export module gse.physics.units.angle;

import gse.physics.units.quantity;

export namespace gse::units {
	constexpr char degrees_units[] = "deg";
	constexpr char radians_units[] = "rad";

	using degrees = unit<float, 1.0f, degrees_units>;
	using radians = unit<float, 1 / 0.01745329252f, radians_units>;

	using angle_units = unit_list<
		units::degrees,
		units::radians
	>;
}

export namespace gse {
	struct angle : quantity<angle, units::degrees, units::angle_units> {
		using quantity::quantity;
	};

	angle degrees(const float value) {
		return angle::from<units::degrees>(value);
	}

	angle radians(const float value) {
		return angle::from<units::radians>(value);
	}
}
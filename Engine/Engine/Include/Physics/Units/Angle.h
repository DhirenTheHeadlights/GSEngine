#pragma once

#include "UnitTemplate.h"

namespace gse::units {
	constexpr char degrees_units[] = "deg";
	constexpr char radians_units[] = "rad";

	using degrees = unit<float, 1.0f, degrees_units>;
	using radians = unit<float, 1 / 0.01745329252f, radians_units>;
}

namespace gse {
	using angle_units = unit_list<
		units::degrees,
		units::radians
	>;

	struct angle : quantity<angle, units::degrees, angle_units> {
		using quantity::quantity;
	};

	inline angle degrees(const float value) {
		return angle::from<units::degrees>(value);
	}

	inline angle radians(const float value) {
		return angle::from<units::radians>(value);
	}
}
#pragma once

#include "UnitTemplate.h"

namespace Engine {
	constexpr char degreesUnits[] = "deg";
	constexpr char radiansUnits[] = "rad";

	using Degrees = Unit<float, 1.0f, degreesUnits>;
	using Radians = Unit<float, 0.01745329252f, radiansUnits>;
}

namespace Engine {
	using AngleUnits = UnitList<
		Degrees,
		Radians
	>;

	struct Angle : Quantity<Angle, Degrees, AngleUnits> {
		using Quantity::Quantity;
	};

	inline Angle degrees(const float value) {
		return Angle::from<Degrees>(value);
	}

	inline Angle radians(const float value) {
		return Angle::from<Radians>(value);
	}
}
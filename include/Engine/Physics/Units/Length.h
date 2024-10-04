#pragma once

#include "Engine/Physics/Units/UnitTemplate.h"

namespace Engine::Units {
	constexpr char kilometers[] = "km";
	constexpr char meters[] = "m";
	constexpr char centimeters[] = "cm";
	constexpr char millimeters[] = "mm";
	constexpr char yards[] = "yd";
	constexpr char feet[] = "ft";
	constexpr char inches[] = "in";

	// Define specific unit types
	using Kilometers = Unit<float, 1000.0f, kilometers>;
	using Meters = Unit<float, 1.0f, meters>;
	using Centimeters = Unit<float, 0.01f, centimeters>;
	using Millimeters = Unit<float, 0.001f, millimeters>;
	using Yards = Unit<float, 0.9144f, yards>;
	using Feet = Unit<float, 0.3048f, feet>;
	using Inches = Unit<float, 0.0254f, inches>;
}

namespace Engine {
	using LengthUnits = UnitList<
		Units::Kilometers,
		Units::Meters,
		Units::Centimeters,
		Units::Millimeters,
		Units::Yards,
		Units::Feet,
		Units::Inches
	>;
	struct Length : Quantity<LengthUnits> {
		using Quantity::Quantity;
	};
}

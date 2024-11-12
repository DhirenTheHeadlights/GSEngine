#pragma once

#include "Physics/Units/UnitTemplate.h"

namespace Engine {
	struct LengthTag {};

	constexpr char kilometers[] = "km";
	constexpr char meters[] = "m";
	constexpr char centimeters[] = "cm";
	constexpr char millimeters[] = "mm";
	constexpr char yards[] = "yd";
	constexpr char feet[] = "ft";
	constexpr char inches[] = "in";

	// Define specific unit types
	using Kilometers = Unit<LengthTag, 1000.0f, kilometers>;
	using Meters = Unit<LengthTag, 1.0f, meters>;
	using Centimeters = Unit<LengthTag, 0.01f, centimeters>;
	using Millimeters = Unit<LengthTag, 0.001f, millimeters>;
	using Yards = Unit<LengthTag, 0.9144f, yards>;
	using Feet = Unit<LengthTag, 0.3048f, feet>;
	using Inches = Unit<LengthTag, 0.0254f, inches>;
}

namespace Engine {
	using LengthUnits = UnitList<
		Kilometers,
		Meters,
		Centimeters,
		Millimeters,
		Yards,
		Feet,
		Inches
	>;
	struct Length : Quantity<Length, Meters, LengthUnits> {
		using Quantity::Quantity;
	};
}		

#pragma once

#include "Physics/Units/UnitTemplate.h"

namespace Engine {
	struct LengthTag {};

	constexpr char kilometersUnits[] = "km";
	constexpr char metersUnits[] = "m";
	constexpr char centimetersUnits[] = "cm";
	constexpr char millimetersUnits[] = "mm";
	constexpr char yardsUnits[] = "yd";
	constexpr char feetUnits[] = "ft";
	constexpr char inchesUnits[] = "in";

	using Kilometers = Unit<LengthTag, 1000.0f, kilometersUnits>;
	using Meters = Unit<LengthTag, 1.0f, metersUnits>;
	using Centimeters = Unit<LengthTag, 0.01f, centimetersUnits>;
	using Millimeters = Unit<LengthTag, 0.001f, millimetersUnits>;
	using Yards = Unit<LengthTag, 0.9144f, yardsUnits>;
	using Feet = Unit<LengthTag, 0.3048f, feetUnits>;
	using Inches = Unit<LengthTag, 0.0254f, inchesUnits>;
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

	inline Length kilometers(const float value) {
		return Length::from<Kilometers>(value);
	}

	inline Length meters(const float value) {
		return Length::from<Meters>(value);
	}

	inline Length centimeters(const float value) {
		return Length::from<Centimeters>(value);
	}

	inline Length millimeters(const float value) {
		return Length::from<Millimeters>(value);
	}

	inline Length yards(const float value) {
		return Length::from<Yards>(value);
	}

	inline Length feet(const float value) {
		return Length::from<Feet>(value);
	}

	inline Length inches(const float value) {
		return Length::from<Inches>(value);
	}
}		

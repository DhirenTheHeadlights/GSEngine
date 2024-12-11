#pragma once

#include "Physics/Units/UnitTemplate.h"

namespace gse::units {
	struct length_tag {};

	constexpr char kilometers_units[] = "km";
	constexpr char meters_units[] = "m";
	constexpr char centimeters_units[] = "cm";
	constexpr char millimeters_units[] = "mm";
	constexpr char yards_units[] = "yd";
	constexpr char feet_units[] = "ft";
	constexpr char inches_units[] = "in";

	using kilometers = unit<length_tag, 1000.0f, kilometers_units>;
	using meters = unit<length_tag, 1.0f, meters_units>;
	using centimeters = unit<length_tag, 0.01f, centimeters_units>;
	using millimeters = unit<length_tag, 0.001f, millimeters_units>;
	using yards = unit<length_tag, 0.9144f, yards_units>;
	using feet = unit<length_tag, 0.3048f, feet_units>;
	using inches = unit<length_tag, 0.0254f, inches_units>;
}

namespace gse {
	using length_units = unit_list<
		units::kilometers,
		units::meters,
		units::centimeters,
		units::millimeters,
		units::yards,
		units::feet,
		units::inches
	>;

	struct length : quantity<length, units::meters, length_units> {
		using quantity::quantity;
	};

	inline length kilometers(const float value) {
		return length::from<units::kilometers>(value);
	}

	inline length meters(const float value) {
		return length::from<units::meters>(value);
	}

	inline length centimeters(const float value) {
		return length::from<units::centimeters>(value);
	}

	inline length millimeters(const float value) {
		return length::from<units::millimeters>(value);
	}

	inline length yards(const float value) {
		return length::from<units::yards>(value);
	}

	inline length feet(const float value) {
		return length::from<units::feet>(value);
	}

	inline length inches(const float value) {
		return length::from<units::inches>(value);
	}
}		

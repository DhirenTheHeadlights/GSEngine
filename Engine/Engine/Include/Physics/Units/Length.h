#pragma once

#include "Physics/Units/UnitTemplate.h"

namespace gse {
	struct length_tag {};

	constexpr char kilometers_units[] = "km";
	constexpr char meters_units[] = "m";
	constexpr char centimeters_units[] = "cm";
	constexpr char millimeters_units[] = "mm";
	constexpr char yards_units[] = "yd";
	constexpr char feet_units[] = "ft";
	constexpr char inches_units[] = "in";

	struct kilometers : unit<length_tag, 1000.0f, kilometers_units> {};
	struct meters : unit<length_tag, 1.0f, meters_units> {};
	struct centimeters : unit<length_tag, 0.01f, centimeters_units> {};
	struct millimeters : unit<length_tag, 0.001f, millimeters_units> {};
	struct yards : unit<length_tag, 0.9144f, yards_units> {};
	struct feet : unit<length_tag, 0.3048f, feet_units> {};
	struct inches : unit<length_tag, 0.0254f, inches_units> {};
}

namespace gse {
	using length_units = unit_list<
		kilometers,
		meters,
		centimeters,
		millimeters,
		yards,
		feet,
		inches
	>;

	struct length : quantity<length, meters, length_units> {
		using quantity::quantity;
	};

	inline length kilometers(const float value) {
		return length::from<struct kilometers>(value);
	}

	inline length meters(const float value) {
		return length::from<struct meters>(value);
	}

	inline length centimeters(const float value) {
		return length::from<struct centimeters>(value);
	}

	inline length millimeters(const float value) {
		return length::from<struct millimeters>(value);
	}

	inline length yards(const float value) {
		return length::from<struct yards>(value);
	}

	inline length feet(const float value) {
		return length::from<Feet>(value);
	}

	inline length inches(const float value) {
		return length::from<Inches>(value);
	}
}		

#pragma once

#include "Physics/Units/UnitTemplate.h"

// Mass

namespace gse {
	struct mass_tag {};

	constexpr char kilograms_units[] = "kg";
	constexpr char grams_units[] = "g";
	constexpr char pounds_units[] = "lb";

	struct kilograms : unit<mass_tag, 1.0f, kilograms_units> {};
	struct grams : unit<mass_tag, 0.001f, grams_units> {};
	struct pounds : unit<mass_tag, 0.453592f, pounds_units> {};
}

namespace gse {
	using mass_units = unit_list<
		kilograms,
		grams,
		pounds
	>;

	struct mass : quantity<mass, kilograms, mass_units> {
		using quantity::quantity;
	};

	inline mass kilograms(const float value) {
		return mass::from<struct kilograms>(value);
	}

	inline mass grams(const float value) {
		return mass::from<struct grams>(value);
	}

	inline mass pounds(const float value) {
		return mass::from<struct pounds>(value);
	}
}

// Force

namespace gse {
	struct ForceTag {};

	constexpr char newtonsUnits[] = "N";
	constexpr char poundsForceUnits[] = "lbf";

	struct newtons : unit<ForceTag, 1.0f, newtonsUnits> {};
	struct pounds_force : unit<ForceTag, 4.44822f, poundsForceUnits> {};
}

namespace gse {
	using force_units = unit_list<
		newtons,
		pounds_force
	>;

	struct force : quantity<force, newtons, force_units> {
		using quantity::quantity;
	};

	inline force newtons(const float value) {
		return force::from<struct newtons>(value);
	}

	inline force pounds_force(const float value) {
		return force::from<struct pounds_force>(value);
	}
}
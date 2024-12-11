#pragma once

#include "Physics/Units/UnitTemplate.h"

// Mass

namespace gse::units {
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
		units::kilograms,
		units::grams,
		units::pounds
	>;

	struct mass : quantity<mass, units::kilograms, mass_units> {
		using quantity::quantity;
	};

	inline mass kilograms(const float value) {
		return mass::from<units::kilograms>(value);
	}

	inline mass grams(const float value) {
		return mass::from<units::grams>(value);
	}

	inline mass pounds(const float value) {
		return mass::from<units::pounds>(value);
	}
}

// Force

namespace gse::units {
	struct force_tag {};

	constexpr char newtons_units[] = "N";
	constexpr char pounds_force_units[] = "lbf";

	using newtons = unit<force_tag, 1.0f, newtons_units>;
	using pounds_force = unit<force_tag, 4.44822f, pounds_force_units>;
}

namespace gse {
	using force_units = unit_list<
		units::newtons,
		units::pounds_force
	>;

	struct force : quantity<force, units::newtons, force_units> {
		using quantity::quantity;
	};

	inline force newtons(const float value) {
		return force::from<units::newtons>(value);
	}

	inline force pounds_force(const float value) {
		return force::from<units::pounds_force>(value);
	}
}
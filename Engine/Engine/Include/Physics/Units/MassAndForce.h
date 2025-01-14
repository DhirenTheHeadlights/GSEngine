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

	inline auto kilograms(const float value) -> mass {
		return mass::from<units::kilograms>(value);
	}

	inline auto grams(const float value) -> mass {
		return mass::from<units::grams>(value);
	}

	inline auto pounds(const float value) -> mass {
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

	struct torque_tag {};

	constexpr char newton_meters_units[] = "N-m";
	constexpr char pound_feet_units[] = "lbf-ft";

	using newton_meters = unit<torque_tag, 1.0f, newton_meters_units>;
	using pound_feet = unit<torque_tag, 1.35582f, pound_feet_units>;
}

namespace gse {
	using force_units = unit_list<
		units::newtons,
		units::pounds_force
	>;

	struct force : quantity<force, units::newtons, force_units> {
		using quantity::quantity;
	};

	inline auto newtons(const float value) -> force {
		return force::from<units::newtons>(value);
	}

	inline auto pounds_force(const float value) -> force {
		return force::from<units::pounds_force>(value);
	}

	using torque_units = unit_list <
		units::newton_meters,
		units::pound_feet
	>;

	struct torque : quantity<torque, units::newton_meters, torque_units> {
		using quantity::quantity;
	};

	inline auto newton_meters(const float value) -> torque {
		return torque::from<units::newton_meters>(value);
	}

	inline auto pound_feet(const float value) -> torque {
		return torque::from<units::pound_feet>(value);
	}
}
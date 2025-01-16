export module gse.physics.units.mass_and_force;

import gse.physics.units.quantity;

// Mass

export namespace gse::units {
	struct mass_tag {};

	constexpr char kilograms_units[] = "kg";
	constexpr char grams_units[] = "g";
	constexpr char pounds_units[] = "lb";

	struct kilograms : unit<mass_tag, 1.0f, kilograms_units> {};
	struct grams : unit<mass_tag, 0.001f, grams_units> {};
	struct pounds : unit<mass_tag, 0.453592f, pounds_units> {};

	using mass_units = unit_list<
		units::kilograms,
		units::grams,
		units::pounds
	>;
}

export namespace gse {
	struct mass : quantity<mass, units::kilograms, units::mass_units> {
		using quantity::quantity;
	};

	auto kilograms(const float value) -> mass {
		return mass::from<units::kilograms>(value);
	}

	auto grams(const float value) -> mass {
		return mass::from<units::grams>(value);
	}

	auto pounds(const float value) -> mass {
		return mass::from<units::pounds>(value);
	}
}

// Force

namespace gse::units {
	struct force_tag {};

	constexpr char newtons_units[] = "N";
	constexpr char pounds_force_units[] = "lbf";

	export using newtons = unit<force_tag, 1.0f, newtons_units>;
	export using pounds_force = unit<force_tag, 4.44822f, pounds_force_units>;

	using force_units = unit_list<
		units::newtons,
		units::pounds_force
	>;

	struct torque_tag {};

	constexpr char newton_meters_units[] = "N-m";
	constexpr char pound_feet_units[] = "lbf-ft";

	export using newton_meters = unit<torque_tag, 1.0f, newton_meters_units>;
	export using pound_feet = unit<torque_tag, 1.35582f, pound_feet_units>;

	using torque_units = unit_list <
		units::newton_meters,
		units::pound_feet
	>;
}

export namespace gse {
	struct force : quantity<force, units::newtons, units::force_units> {
		using quantity::quantity;
	};

	auto newtons(const float value) -> force {
		return force::from<units::newtons>(value);
	}

	auto pounds_force(const float value) -> force {
		return force::from<units::pounds_force>(value);
	}

	struct torque : quantity<torque, units::newton_meters, units::torque_units> {
		using quantity::quantity;
	};

	auto newton_meters(const float value) -> torque {
		return torque::from<units::newton_meters>(value);
	}

	auto pound_feet(const float value) -> torque {
		return torque::from<units::pound_feet>(value);
	}
}
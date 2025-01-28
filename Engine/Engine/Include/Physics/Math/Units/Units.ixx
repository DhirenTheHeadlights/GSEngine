export module gse.physics.math.units;

export import gse.physics.math.units.angle;
export import gse.physics.math.units.duration;
export import gse.physics.math.units.energy_and_power;
export import gse.physics.math.units.length;
export import gse.physics.math.units.mass_and_force;
export import gse.physics.math.units.movement;

import gse.physics.math.units.quantity;

export namespace gse::internal {
	struct unitless_tag {};
	constexpr char unitless_units[] = "Unitless";
	struct unitless_unit : unit<unitless_tag, 1.0f, unitless_units> {};
}

export namespace gse {
	using unitless_units = unit_list<internal::unitless_unit>;

	struct unitless : quantity<unitless, internal::unitless_unit, unitless_units> {
		unitless() = default;

		unitless(const float value) : quantity(value) {}

		operator float() const {
			return m_val;
		}
	};
}

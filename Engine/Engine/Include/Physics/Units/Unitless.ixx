export module gse.physics.units.unitless;

import gse.physics.units.quantity;

export namespace gse::internal {
	struct unitless_tag {};
	constexpr char unitless_units_c[] = "Unitless";
	struct unitless_unit : unit<unitless_tag, 1.0f, unitless_units_c> {};
}

export namespace gse {
	using unitless_units = unit_list<internal::unitless_unit>;

	struct unitless : quantity<unitless, internal::unitless_unit, unitless_units> {
		using quantity::quantity;

		unitless(const float value) : quantity(value) {}

		operator float() const {
			return m_val;
		}
	};
}

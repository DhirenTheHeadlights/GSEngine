#pragma once

#include "Physics/Units/Angle.h"
#include "Physics/Units/Duration.h"
#include "Physics/Units/EnergyAndPower.h"
#include "Physics/Units/Length.h"
#include "Physics/Units/MassAndForce.h"
#include "Physics/Units/Movement.h"

namespace gse::internal {
	struct unitless_tag {};
	constexpr char unitless_units[] = "Unitless";
	struct unitless_unit : unit<unitless_tag, 1.0f, unitless_units> {};
}

namespace gse {
	using unitless_units = unit_list<internal::unitless_unit>;

	struct unitless : quantity<unitless, internal::unitless_unit, unitless_units> {
		using quantity::quantity;

		unitless(const float value) : quantity(value) {}

		operator float() const {
			return m_val;
		}
	};
}

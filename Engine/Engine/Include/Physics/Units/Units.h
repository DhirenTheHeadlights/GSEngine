#pragma once

#include "Physics/Units/Angle.h"
#include "Physics/Units/Duration.h"
#include "Physics/Units/EnergyAndPower.h"
#include "Physics/Units/Length.h"
#include "Physics/Units/MassAndForce.h"
#include "Physics/Units/Movement.h"

namespace gse {
	struct UnitlessTag {};
	constexpr char unitlessUnits[] = "Unitless";
	using UnitlessUnit = Unit<UnitlessTag, 1.0f, unitlessUnits>;
}

namespace gse {
	using UnitlessUnits = UnitList<UnitlessUnit>;

	struct Unitless : Quantity<Unitless, UnitlessUnit, UnitlessUnits> {
		using Quantity::Quantity;

		Unitless(const float value) : Quantity(value) {}

		template <IsUnit Unit>
		auto operator*(const Unit& other) const {
			return Unit(val * other.getValue());
		}

		template <IsUnit Unit>
		auto operator/(const Unit& other) const {
			return Unit(val / other.getValue());
		}

		template <IsUnit Unit>
		Quantity& operator*=(const Unit& other) {
			val *= other.getValue();
			return *this;
		}

		template <IsUnit Unit>
		Quantity& operator/=(const Unit& other) {
			val /= other.getValue()();
			return *this;
		}

		operator float() const {
			return val;
		}
	};

	inline Unitless unitless(const float value) {
		return value;
	}
}

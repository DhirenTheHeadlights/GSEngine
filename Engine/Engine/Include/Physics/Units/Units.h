#pragma once

#include "Physics/Units/Duration.h"
#include "Physics/Units/EnergyAndPower.h"
#include "Physics/Units/Length.h"
#include "Physics/Units/MassAndForce.h"
#include "Physics/Units/Movement.h"

namespace Engine {
	struct UnitlessTag {};
	constexpr char unitlessUnits[] = "Unitless";
	using UnitlessPlaceholder = Unit<UnitlessTag, 1.0f, unitlessUnits>;
}

namespace Engine {
	using UnitlessUnits = UnitList<UnitlessPlaceholder>;

	struct Unitless : Quantity<Unitless, UnitlessPlaceholder, UnitlessUnits> {
		using Quantity::Quantity;
		//static constexpr char Units[] = "Unitless";

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
	};

	inline Unitless unitless(const float value) {
		return value;
	}
}

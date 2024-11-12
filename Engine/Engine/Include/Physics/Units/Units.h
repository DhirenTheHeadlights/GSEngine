#pragma once

#include "Physics/Units/Duration.h"
#include "Physics/Units/EnergyAndPower.h"
#include "Physics/Units/Length.h"
#include "Physics/Units/MassAndForce.h"
#include "Physics/Units/Movement.h"

namespace Engine {
	struct UnitlessTag {};
	constexpr char unitless[] = "Unitless";
	using UnitlessPlaceholder = Unit<UnitlessTag, 1.0f, unitless>;
}

namespace Engine {
	using UnitlessUnits = UnitList<UnitlessPlaceholder>;
	struct Unitless : Quantity<Unitless, UnitlessPlaceholder, UnitlessUnits> {
		using Quantity::Quantity;

		Unitless(const float value) : Quantity(value) {}

		template <IsUnit Unit>
		auto operator*(const Unit& other) const {
			return Unit(value * other.getValue());
		}

		template <IsUnit Unit>
		auto operator/(const Unit& other) const {
			return Unit(value / other.getValue());
		}

		template <IsUnit Unit>
		Quantity& operator*=(const Unit& other) {
			value *= other.getValue();
			return *this;
		}

		template <IsUnit Unit>
		Quantity& operator/=(const Unit& other) {
			value /= other.getValue();
			return *this;
		}
	};
}

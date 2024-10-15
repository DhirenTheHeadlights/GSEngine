#pragma once

#include "Engine/Physics/Units/EnergyAndPower.h"
#include "Engine/Physics/Units/Length.h"
#include "Engine/Physics/Units/MassAndForce.h"
#include "Engine/Physics/Units/Movement.h"

namespace Engine::Units {
	struct PlaceHolderDoNotUseTag {};
	constexpr char placeHolderDoNotUse[] = "This should never show up";
	using PlaceHolderDoNotUse = Unit<PlaceHolderDoNotUseTag, 1.0f, placeHolderDoNotUse>;

	struct UnitlessTag {};
	constexpr char unitless[] = "Unitless";
	using Unitless = Unit<UnitlessTag, 1.0f, unitless>;
}

namespace Engine {
	using PlaceHolderUnits = UnitList<>;
	struct PlaceHolderDoNotUse : Quantity<PlaceHolderDoNotUse, Units::PlaceHolderDoNotUse, PlaceHolderUnits> {
		using Quantity::Quantity;
	};

	using UnitlessUnits = UnitList<Units::Unitless>;
	struct Unitless : Quantity<Unitless, Units::Unitless, UnitlessUnits> {
		using Quantity::Quantity;

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

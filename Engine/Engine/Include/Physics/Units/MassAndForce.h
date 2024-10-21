#pragma once

#include "Physics/Units/UnitTemplate.h"

// Mass

namespace Engine::Units {
	struct MassTag {};

	constexpr char kilograms[] = "kg";
	constexpr char grams[] = "g";
	constexpr char pounds[] = "lb";

	using Kilograms = Unit<MassTag, 1.0f, kilograms>;
	using Grams = Unit<MassTag, 0.001f, grams>;
	using Pounds = Unit<MassTag, 0.453592f, pounds>;
}

namespace Engine {
	using MassUnits = UnitList<
		Units::Kilograms,
		Units::Grams,
		Units::Pounds
	>;

	struct Mass : Quantity<Mass, Units::Kilograms, MassUnits> {
		using Quantity::Quantity;
	};
}

// Force

namespace Engine::Units {
	struct ForceTag {};

	constexpr char newtons[] = "N";
	constexpr char poundsForce[] = "lbf";

	using Newtons = Unit<ForceTag, 1.0f, newtons>;
	using PoundsForce = Unit<ForceTag, 4.44822f, poundsForce>;
}

namespace Engine {
	using ForceUnits = UnitList<
		Units::Newtons,
		Units::PoundsForce
	>;

	struct Force : Quantity<Force, Units::Newtons, ForceUnits> {
		using Quantity::Quantity;
	};
}
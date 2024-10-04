#pragma once

#include "Engine/Physics/Units/UnitTemplate.h"

// Mass

namespace Engine::Units {
	constexpr char kilograms[] = "kg";
	constexpr char grams[] = "g";
	constexpr char pounds[] = "lb";

	using Kilograms = Unit<float, 1.0f, kilograms>;
	using Grams = Unit<float, 0.001f, grams>;
	using Pounds = Unit<float, 0.453592f, pounds>;
}

namespace Engine {
	using MassUnits = UnitList<
		Units::Kilograms,
		Units::Grams,
		Units::Pounds
	>;

	struct Mass : Quantity<MassUnits> {
		using Quantity::Quantity;
	};
}

// Force

namespace Engine::Units {
	constexpr char newtons[] = "N";
	constexpr char poundsForce[] = "lbf";

	using Newtons = Unit<float, 1.0f, newtons>;
	using PoundsForce = Unit<float, 4.44822f, poundsForce>;
}

namespace Engine {
	using ForceUnits = UnitList<
		Units::Newtons,
		Units::PoundsForce
	>;

	struct Force : Quantity<ForceUnits> {
		using Quantity::Quantity;
	};
}
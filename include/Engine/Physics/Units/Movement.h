#pragma once

#include "Engine/Physics/Units/UnitTemplate.h"

// Velocity

namespace Engine::Units {
	using MetersPerSecond = Unit<float, 1.0f>;
	using KilometersPerHour = Unit<float, 0.277778f>;
	using MilesPerHour = Unit<float, 0.44704f>;
	using FeetPerSecond = Unit<float, 0.3048f>;
}

namespace Engine {
	struct Velocity : Quantity<Units::MetersPerSecond> {
		using Quantity::Quantity;
	};
}

// Acceleration

namespace Engine::Units {
	using MetersPerSecondSquared = Unit<float, 1.0f>;
	using KilometersPerHourSquared = Unit<float, 0.0000771605f>;
	using MilesPerHourSquared = Unit<float, 0.000124514f>;
	using FeetPerSecondSquared = Unit<float, 0.3048f>;
}

namespace Engine {
	struct Acceleration : Quantity<Units::MetersPerSecondSquared> {
		using Quantity::Quantity;
	};
}

#pragma once

#include "Engine/Physics/Units/UnitTemplate.h"

// Velocity

namespace Engine::Units {
	constexpr char metersPerSecond[] = "m/s";
	constexpr char kilometersPerHour[] = "km/h";
	constexpr char milesPerHour[] = "mph";
	constexpr char feetPerSecond[] = "ft/s";

	using MetersPerSecond = Unit<float, 1.0f, metersPerSecond>;
	using KilometersPerHour = Unit<float, 0.277778f, kilometersPerHour>;
	using MilesPerHour = Unit<float, 0.44704f, milesPerHour>;
	using FeetPerSecond = Unit<float, 0.3048f, feetPerSecond>;
}

namespace Engine {
	using VelocityUnits = UnitList<
		Units::MetersPerSecond,
		Units::KilometersPerHour,
		Units::MilesPerHour,
		Units::FeetPerSecond
	>;

	struct Velocity : Quantity<VelocityUnits> {
		using Quantity::Quantity;
	};
}

// Acceleration

namespace Engine::Units {
	constexpr char metersPrSecondSquared[] = "m/s^2";
	constexpr char kilometersPrHourSquared[] = "km/h^2";
	constexpr char milesPrHourSquared[] = "mph^2";
	constexpr char feetPrSecondSquared[] = "ft/s^2";

	using MetersPerSecondSquared = Unit<float, 1.0f, metersPrSecondSquared>;
	using KilometersPerHourSquared = Unit<float, 0.0000771605f, kilometersPrHourSquared>;
	using MilesPerHourSquared = Unit<float, 0.000124514f, milesPrHourSquared>;
	using FeetPerSecondSquared = Unit<float, 0.3048f, feetPrSecondSquared>;
}

namespace Engine {
	using AccelerationUnits = UnitList<
		Units::MetersPerSecondSquared,
		Units::KilometersPerHourSquared,
		Units::MilesPerHourSquared,
		Units::FeetPerSecondSquared
	>;

	struct Acceleration : Quantity<AccelerationUnits> {
		using Quantity::Quantity;
	};
}
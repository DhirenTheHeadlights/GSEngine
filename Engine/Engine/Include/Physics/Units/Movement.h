#pragma once

#include "Physics/Units/UnitTemplate.h"

// Velocity

namespace Engine {
	struct VelocityTag {};

	constexpr char metersPerSecond[] = "m/s";
	constexpr char kilometersPerHour[] = "km/h";
	constexpr char milesPerHour[] = "mph";
	constexpr char feetPerSecond[] = "ft/s";

	using MetersPerSecond = Unit<VelocityTag, 1.0f, metersPerSecond>;
	using KilometersPerHour = Unit<VelocityTag, 0.277778f, kilometersPerHour>;
	using MilesPerHour = Unit<VelocityTag, 0.44704f, milesPerHour>;
	using FeetPerSecond = Unit<VelocityTag, 0.3048f, feetPerSecond>;
}

namespace Engine {
	using VelocityUnits = UnitList<
		MetersPerSecond,
		KilometersPerHour,
		MilesPerHour,
		FeetPerSecond
	>;

	struct Velocity : Quantity<Velocity, MetersPerSecond, VelocityUnits> {
		using Quantity::Quantity;
	};
}

// Acceleration

namespace Engine {
	struct AccelerationTag {};

	constexpr char metersPrSecondSquared[] = "m/s^2";
	constexpr char kilometersPrHourSquared[] = "km/h^2";
	constexpr char milesPrHourSquared[] = "mph^2";
	constexpr char feetPrSecondSquared[] = "ft/s^2";

	using MetersPerSecondSquared = Unit<AccelerationTag, 1.0f, metersPrSecondSquared>;
	using KilometersPerHourSquared = Unit<AccelerationTag, 0.0000771605f, kilometersPrHourSquared>;
	using MilesPerHourSquared = Unit<AccelerationTag, 0.000124514f, milesPrHourSquared>;
	using FeetPerSecondSquared = Unit<AccelerationTag, 0.3048f, feetPrSecondSquared>;
}

namespace Engine {
	using AccelerationUnits = UnitList<
		MetersPerSecondSquared,
		KilometersPerHourSquared,
		MilesPerHourSquared,
		FeetPerSecondSquared
	>;

	struct Acceleration : Quantity<Acceleration, MetersPerSecondSquared, AccelerationUnits> {
		using Quantity::Quantity;
	};
}
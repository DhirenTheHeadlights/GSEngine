#pragma once

#include "Physics/Units/UnitTemplate.h"

// Velocity

namespace Engine {
	struct VelocityTag {};

	constexpr char metersPerSecondUnits[] = "m/s";
	constexpr char kilometersPerHourUnits[] = "km/h";
	constexpr char milesPerHourUnits[] = "mph";
	constexpr char feetPerSecondUnits[] = "ft/s";

	using MetersPerSecond = Unit<VelocityTag, 1.0f, metersPerSecondUnits>;
	using KilometersPerHour = Unit<VelocityTag, 0.277778f, kilometersPerHourUnits>;
	using MilesPerHour = Unit<VelocityTag, 0.44704f, milesPerHourUnits>;
	using FeetPerSecond = Unit<VelocityTag, 0.3048f, feetPerSecondUnits>;
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

	inline Velocity metersPerSecond(const float value) {
		return Velocity::from<MetersPerSecond>(value);
	}

	inline Velocity kilometersPerHour(const float value) {
		return Velocity::from<KilometersPerHour>(value);
	}

	inline Velocity milesPerHour(const float value) {
		return Velocity::from<MilesPerHour>(value);
	}

	inline Velocity feetPerSecond(const float value) {
		return Velocity::from<FeetPerSecond>(value);
	}
}

// Acceleration

namespace Engine {
	struct AccelerationTag {};

	constexpr char metersPrSecondSquaredUnits[] = "m/s^2";
	constexpr char kilometersPrHourSquaredUnits[] = "km/h^2";
	constexpr char milesPrHourSquaredUnits[] = "mph^2";
	constexpr char feetPrSecondSquaredUnits[] = "ft/s^2";

	using MetersPerSecondSquared = Unit<AccelerationTag, 1.0f, metersPrSecondSquaredUnits>;
	using KilometersPerHourSquared = Unit<AccelerationTag, 0.0000771605f, kilometersPrHourSquaredUnits>;
	using MilesPerHourSquared = Unit<AccelerationTag, 0.000124514f, milesPrHourSquaredUnits>;
	using FeetPerSecondSquared = Unit<AccelerationTag, 0.3048f, feetPrSecondSquaredUnits>;
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

	inline Acceleration metersPerSecondSquared(const float value) {
		return Acceleration::from<MetersPerSecondSquared>(value);
	}

	inline Acceleration kilometersPerHourSquared(const float value) {
		return Acceleration::from<KilometersPerHourSquared>(value);
	}

	inline Acceleration milesPerHourSquared(const float value) {
		return Acceleration::from<MilesPerHourSquared>(value);
	}

	inline Acceleration feetPerSecondSquared(const float value) {
		return Acceleration::from<FeetPerSecondSquared>(value);
	}
}
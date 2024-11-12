#pragma once

#include "Physics/Units/UnitTemplate.h"

// Energy

namespace Engine {
	struct EnergyTag {};

	constexpr char joules[] = "J";
	constexpr char kilojoules[] = "kJ";
	constexpr char megajoules[] = "MJ";
	constexpr char gigajoules[] = "GJ";
	constexpr char calories[] = "cal";
	constexpr char kilocalories[] = "kcal";

	using Joules = Unit<EnergyTag, 1.0f, joules>;
	using Kilojoules = Unit<EnergyTag, 1000.0f, kilojoules>;
	using Megajoules = Unit<EnergyTag, 1000000.0f, megajoules>;
	using Gigajoules = Unit<EnergyTag, 1000000000.0f, gigajoules>;
	using Calories = Unit<EnergyTag, 4184.0f, calories>;
	using Kilocalories = Unit<EnergyTag, 4184000.0f, kilocalories>;
}

namespace Engine {
	using EnergyUnits = UnitList<
		Joules,
		Kilojoules,
		Megajoules,
		Gigajoules,
		Calories,
		Kilocalories
	>;

	struct Energy : Quantity<Energy, Joules, EnergyUnits> {
		using Quantity::Quantity;
	};
}

// Power

namespace Engine {
	struct PowerTag {};

	constexpr char watts[] = "W";
	constexpr char kilowatts[] = "kW";
	constexpr char megawatts[] = "MW";
	constexpr char gigawatts[] = "GW";
	constexpr char horsepower[] = "hp";

	using Watts = Unit<PowerTag, 1.0f, watts>;
	using Kilowatts = Unit<PowerTag, 1000.0f, kilowatts>;
	using Megawatts = Unit<PowerTag, 1000000.0f, megawatts>;
	using Gigawatts = Unit<PowerTag, 1000000000.0f, gigawatts>;
	using Horsepower = Unit<PowerTag, 745.7f, horsepower>;
}

namespace Engine {
	using PowerUnits = UnitList<
		Watts,
		Kilowatts,
		Megawatts,
		Gigawatts,
		Horsepower
	>;
	struct Power : Quantity<Power, Watts, PowerUnits> {
		using Quantity::Quantity;
	};
}
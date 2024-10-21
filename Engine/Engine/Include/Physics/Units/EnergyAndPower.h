#pragma once

#include "Physics/Units/UnitTemplate.h"

// Energy

namespace Engine::Units {
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
		Units::Joules,
		Units::Kilojoules,
		Units::Megajoules,
		Units::Gigajoules,
		Units::Calories,
		Units::Kilocalories
	>;

	struct Energy : Quantity<Energy, Units::Joules, EnergyUnits> {
		using Quantity::Quantity;
	};
}

// Power

namespace Engine::Units {
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
		Units::Watts,
		Units::Kilowatts,
		Units::Megawatts,
		Units::Gigawatts,
		Units::Horsepower
	>;
	struct Power : Quantity<Power, Units::Watts, PowerUnits> {
		using Quantity::Quantity;
	};
}
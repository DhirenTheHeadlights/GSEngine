#pragma once

#include "Engine/Physics/Units/UnitTemplate.h"

// Energy

namespace Engine::Units {
	constexpr char joules[] = "J";
	constexpr char kilojoules[] = "kJ";
	constexpr char megajoules[] = "MJ";
	constexpr char gigajoules[] = "GJ";
	constexpr char calories[] = "cal";
	constexpr char kilocalories[] = "kcal";

	using Joules = Unit<float, 1.0f, joules>;
	using Kilojoules = Unit<float, 1000.0f, kilojoules>;
	using Megajoules = Unit<float, 1000000.0f, megajoules>;
	using Gigajoules = Unit<float, 1000000000.0f, gigajoules>;
	using Calories = Unit<float, 4184.0f, calories>;
	using Kilocalories = Unit<float, 4184000.0f, kilocalories>;
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

	struct Energy : Quantity<EnergyUnits> {
		using Quantity::Quantity;
	};
}

// Power

namespace Engine::Units {
	constexpr char watts[] = "W";
	constexpr char kilowatts[] = "kW";
	constexpr char megawatts[] = "MW";
	constexpr char gigawatts[] = "GW";
	constexpr char horsepower[] = "hp";

	using Watts = Unit<float, 1.0f, watts>;
	using Kilowatts = Unit<float, 1000.0f, kilowatts>;
	using Megawatts = Unit<float, 1000000.0f, megawatts>;
	using Gigawatts = Unit<float, 1000000000.0f, gigawatts>;
	using Horsepower = Unit<float, 745.7f, horsepower>;
}

namespace Engine {
	using PowerUnits = UnitList<
		Units::Watts,
		Units::Kilowatts,
		Units::Megawatts,
		Units::Gigawatts,
		Units::Horsepower
	>;
	struct Power : Quantity<PowerUnits> {
		using Quantity::Quantity;
	};
}
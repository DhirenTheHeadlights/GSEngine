#pragma once

#include "Physics/Units/UnitTemplate.h"

// Energy

namespace gse {
	struct EnergyTag {};

	constexpr char joulesUnits[] = "J";
	constexpr char kilojoulesUnits[] = "kJ";
	constexpr char megajoulesUnits[] = "MJ";
	constexpr char gigajoulesUnits[] = "GJ";
	constexpr char caloriesUnits[] = "cal";
	constexpr char kilocaloriesUnits[] = "kcal";

	using Joules = Unit<EnergyTag, 1.0f, joulesUnits>;
	using Kilojoules = Unit<EnergyTag, 1000.0f, kilojoulesUnits>;
	using Megajoules = Unit<EnergyTag, 1000000.0f, megajoulesUnits>;
	using Gigajoules = Unit<EnergyTag, 1000000000.0f, gigajoulesUnits>;
	using Calories = Unit<EnergyTag, 4184.0f, caloriesUnits>;
	using Kilocalories = Unit<EnergyTag, 4184000.0f, kilocaloriesUnits>;
}

namespace gse {
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

	inline Energy joules(const float value) {
		return Energy::from<Joules>(value);
	}

	inline Energy kilojoules(const float value) {
		return Energy::from<Kilojoules>(value);
	}

	inline Energy megajoules(const float value) {
		return Energy::from<Megajoules>(value);
	}

	inline Energy gigajoules(const float value) {
		return Energy::from<Gigajoules>(value);
	}

	inline Energy calories(const float value) {
		return Energy::from<Calories>(value);
	}

	inline Energy kilocalories(const float value) {
		return Energy::from<Kilocalories>(value);
	}
}

// Power

namespace gse {
	struct PowerTag {};

	constexpr char wattsUnits[] = "W";
	constexpr char kilowattsUnits[] = "kW";
	constexpr char megawattsUnits[] = "MW";
	constexpr char gigawattsUnits[] = "GW";
	constexpr char horsepowerUnits[] = "hp";

	using Watts = Unit<PowerTag, 1.0f, wattsUnits>;
	using Kilowatts = Unit<PowerTag, 1000.0f, kilowattsUnits>;
	using Megawatts = Unit<PowerTag, 1000000.0f, megawattsUnits>;
	using Gigawatts = Unit<PowerTag, 1000000000.0f, gigawattsUnits>;
	using Horsepower = Unit<PowerTag, 745.7f, horsepowerUnits>;
}

namespace gse {
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

	inline Power watts(const float value) {
		return Power::from<Watts>(value);
	}

	inline Power kilowatts(const float value) {
		return Power::from<Kilowatts>(value);
	}

	inline Power megawatts(const float value) {
		return Power::from<Megawatts>(value);
	}

	inline Power gigawatts(const float value) {
		return Power::from<Gigawatts>(value);
	}

	inline Power horsepower(const float value) {
		return Power::from<Horsepower>(value);
	}
}
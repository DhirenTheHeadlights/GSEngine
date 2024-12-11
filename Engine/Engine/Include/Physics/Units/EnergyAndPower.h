#pragma once

#include "Physics/Units/UnitTemplate.h"

// Energy

namespace gse::units {
	struct energy_tag {};

	constexpr char joules_units[] = "J";
	constexpr char kilojoules_units[] = "kJ";
	constexpr char megajoules_units[] = "MJ";
	constexpr char gigajoules_units[] = "GJ";
	constexpr char calories_units[] = "cal";
	constexpr char kilocalories_units[] = "kcal";

	using joules = unit<energy_tag, 1.0f, joules_units>;
	using kilojoules = unit<energy_tag, 1000.0f, kilojoules_units>;
	using megajoules = unit<energy_tag, 1000000.0f, megajoules_units>;
	using gigajoules = unit<energy_tag, 1000000000.0f, gigajoules_units>;
	using calories = unit<energy_tag, 4.184f, calories_units>;
	using kilocalories = unit<energy_tag, 4184.0f, kilocalories_units>;
}

namespace gse {
	using energy_units = unit_list<
		units::joules,
		units::kilojoules,
		units::megajoules,
		units::gigajoules,
		units::calories,
		units::kilocalories
	>;

	struct energy : quantity<energy, units::joules, energy_units> {
		using quantity::quantity;
	};

	inline energy joules(const float value) {
		return energy::from<units::joules>(value);
	}

	inline energy kilojoules(const float value) {
		return energy::from<units::kilojoules>(value);
	}

	inline energy megajoules(const float value) {
		return energy::from<units::megajoules>(value);
	}

	inline energy gigajoules(const float value) {
		return energy::from<units::gigajoules>(value);
	}

	inline energy calories(const float value) {
		return energy::from<units::calories>(value);
	}

	inline energy kilocalories(const float value) {
		return energy::from<units::kilocalories>(value);
	}
}

// Power

namespace gse::units {
	struct power_tag {};

	constexpr char watts_units[] = "W";
	constexpr char kilowatts_units[] = "kW";
	constexpr char megawatts_units[] = "MW";
	constexpr char gigawatts_units[] = "GW";
	constexpr char horsepower_units[] = "hp";

	using watts = unit<power_tag, 1.0f, watts_units>;
	using kilowatts = unit<power_tag, 1000.0f, kilowatts_units>;
	using megawatts = unit<power_tag, 1000000.0f, megawatts_units>;
	using gigawatts = unit<power_tag, 1000000000.0f, gigawatts_units>;
	using horsepower = unit<power_tag, 745.7f, horsepower_units>;
}

namespace gse {
	using power_units = unit_list<
		units::watts,
		units::kilowatts,
		units::megawatts,
		units::gigawatts,
		units::horsepower
	>;
	struct power : quantity<power, units::watts, power_units> {
		using quantity::quantity;
	};

	inline power watts(const float value) {
		return power::from<units::watts>(value);
	}

	inline power kilowatts(const float value) {
		return power::from<units::kilowatts>(value);
	}

	inline power megawatts(const float value) {
		return power::from<units::megawatts>(value);
	}

	inline power gigawatts(const float value) {
		return power::from<units::gigawatts>(value);
	}

	inline power horsepower(const float value) {
		return power::from<units::horsepower>(value);
	}
}
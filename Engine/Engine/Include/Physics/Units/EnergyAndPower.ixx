export module gse.physics.units.energy_and_power;

import gse.physics.units.quantity;

// Energy

namespace gse::units {
	struct energy_tag {};

	constexpr char joules_units[] = "J";
	constexpr char kilojoules_units[] = "kJ";
	constexpr char megajoules_units[] = "MJ";
	constexpr char gigajoules_units[] = "GJ";
	constexpr char calories_units[] = "cal";
	constexpr char kilocalories_units[] = "kcal";

	export using joules = unit<energy_tag, 1.0f, joules_units>;
	export using kilojoules = unit<energy_tag, 1000.0f, kilojoules_units>;
	export using megajoules = unit<energy_tag, 1000000.0f, megajoules_units>;
	export using gigajoules = unit<energy_tag, 1000000000.0f, gigajoules_units>;
	export using calories = unit<energy_tag, 4.184f, calories_units>;
	export using kilocalories = unit<energy_tag, 4184.0f, kilocalories_units>;

	using energy_units = unit_list<
		units::joules,
		units::kilojoules,
		units::megajoules,
		units::gigajoules,
		units::calories,
		units::kilocalories
	>;
}

export namespace gse {
	struct energy : quantity<energy, units::joules, units::energy_units> {
		using quantity::quantity;
	};

	energy joules(const float value) {
		return energy::from<units::joules>(value);
	}

	energy kilojoules(const float value) {
		return energy::from<units::kilojoules>(value);
	}

	energy megajoules(const float value) {
		return energy::from<units::megajoules>(value);
	}

	energy gigajoules(const float value) {
		return energy::from<units::gigajoules>(value);
	}

	energy calories(const float value) {
		return energy::from<units::calories>(value);
	}

	energy kilocalories(const float value) {
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

	export using watts = unit<power_tag, 1.0f, watts_units>;
	export using kilowatts = unit<power_tag, 1000.0f, kilowatts_units>;
	export using megawatts = unit<power_tag, 1000000.0f, megawatts_units>;
	export using gigawatts = unit<power_tag, 1000000000.0f, gigawatts_units>;
	export using horsepower = unit<power_tag, 745.7f, horsepower_units>;

	using power_units = unit_list<
		units::watts,
		units::kilowatts,
		units::megawatts,
		units::gigawatts,
		units::horsepower
	>;
}

export namespace gse {
	struct power : quantity<power, units::watts, units::power_units> {
		using quantity::quantity;
	};

	power watts(const float value) {
		return power::from<units::watts>(value);
	}

	power kilowatts(const float value) {
		return power::from<units::kilowatts>(value);
	}

	power megawatts(const float value) {
		return power::from<units::megawatts>(value);
	}

	power gigawatts(const float value) {
		return power::from<units::gigawatts>(value);
	}

	power horsepower(const float value) {
		return power::from<units::horsepower>(value);
	}
}
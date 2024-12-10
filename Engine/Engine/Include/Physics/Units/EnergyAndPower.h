#pragma once

#include "Physics/Units/UnitTemplate.h"

// Energy

namespace gse {
	struct energy_tag {};

	constexpr char joules_units[] = "J";
	constexpr char kilojoules_units[] = "kJ";
	constexpr char megajoules_units[] = "MJ";
	constexpr char gigajoules_units[] = "GJ";
	constexpr char calories_units[] = "cal";
	constexpr char kilocalories_units[] = "kcal";

	struct joules : unit<energy_tag, 1.0f, joules_units> {};
	struct kilojoules : unit<energy_tag, 1000.0f, kilojoules_units> {};
	struct megajoules : unit<energy_tag, 1000000.0f, megajoules_units> {};
	struct gigajoules : unit<energy_tag, 1000000000.0f, gigajoules_units> {};
	struct calories : unit<energy_tag, 4.184f, calories_units> {};
	struct kilocalories : unit<energy_tag, 4184.0f, kilocalories_units> {};
}

namespace gse {
	using energy_units = unit_list<
		joules,
		kilojoules,
		megajoules,
		gigajoules,
		calories,
		kilocalories
	>;

	struct energy : quantity<energy, joules, energy_units> {
		using quantity::quantity;
	};

	inline energy joules(const float value) {
		return energy::from<struct joules>(value);
	}

	inline energy kilojoules(const float value) {
		return energy::from<struct kilojoules>(value);
	}

	inline energy megajoules(const float value) {
		return energy::from<struct megajoules>(value);
	}

	inline energy gigajoules(const float value) {
		return energy::from<struct gigajoules>(value);
	}

	inline energy calories(const float value) {
		return energy::from<struct calories>(value);
	}

	inline energy kilocalories(const float value) {
		return energy::from<struct kilocalories>(value);
	}
}

// Power

namespace gse {
	struct power_tag {};

	constexpr char watts_units[] = "W";
	constexpr char kilowatts_units[] = "kW";
	constexpr char megawatts_units[] = "MW";
	constexpr char gigawatts_units[] = "GW";
	constexpr char horsepower_units[] = "hp";

	struct watts : unit<power_tag, 1.0f, watts_units> {};
	struct kilowatts : unit<power_tag, 1000.0f, kilowatts_units> {};
	struct megawatts : unit<power_tag, 1000000.0f, megawatts_units> {};
	struct gigawatts : unit<power_tag, 1000000000.0f, gigawatts_units> {};
	struct horsepower : unit<power_tag, 745.7f, horsepower_units> { };
}

namespace gse {
	using power_units = unit_list<
		watts,
		kilowatts,
		megawatts,
		gigawatts,
		horsepower
	>;
	struct power : quantity<power, watts, power_units> {
		using quantity::quantity;
	};

	inline power watts(const float value) {
		return power::from<struct watts>(value);
	}

	inline power kilowatts(const float value) {
		return power::from<struct kilowatts>(value);
	}

	inline power megawatts(const float value) {
		return power::from<struct megawatts>(value);
	}

	inline power gigawatts(const float value) {
		return power::from<struct gigawatts>(value);
	}

	inline power horsepower(const float value) {
		return power::from<struct horsepower>(value);
	}
}
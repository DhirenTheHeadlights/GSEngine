#pragma once

#include "Physics/Units/UnitTemplate.h"

namespace gse::units {
	struct time_tag {};

	constexpr char milliseconds_units[] = "ms";
	constexpr char seconds_units[] = "s";
	constexpr char minutes_units[] = "m";
	constexpr char hours_units[] = "h";

	using milliseconds = unit<time_tag, 0.001f, milliseconds_units>;
	using seconds = unit<time_tag, 1.0f, seconds_units>;
	using minutes = unit<time_tag, 60.0f, minutes_units>;
	using hours = unit<time_tag, 3600.0f, hours_units>;
}

namespace gse {
	using time_units = unit_list<
		units::milliseconds,
		units::seconds,
		units::minutes,
		units::hours
	>;

	struct time : quantity<time, units::seconds, time_units> {
		using quantity::quantity;
	};

	inline time milliseconds(const float value) {
		return time::from<units::milliseconds>(value);
	}
	 
	inline time seconds(const float value) {
		return time::from<units::seconds>(value);
	}

	inline time minutes(const float value) {
		return time::from<units::minutes>(value);
	}

	inline time hours(const float value) {
		return time::from<units::hours>(value);
	}
}
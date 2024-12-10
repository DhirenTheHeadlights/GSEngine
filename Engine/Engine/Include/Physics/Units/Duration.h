#pragma once

#include "Physics/Units/UnitTemplate.h"

namespace gse {
	struct time_tag {};

	constexpr char milliseconds_units[] = "ms";
	constexpr char seconds_units[] = "s";
	constexpr char minutes_units[] = "m";
	constexpr char hours_units[] = "h";

	struct milliseconds : unit<time_tag, 0.001f, milliseconds_units> {};
	struct seconds : unit<time_tag, 1.0f, seconds_units> {};
	struct minutes : unit<time_tag, 60.0f, minutes_units> {};
	struct hours : unit<time_tag, 3600.0f, hours_units> {};
}

namespace gse {
	using time_units = unit_list<
		milliseconds,
		seconds,
		minutes,
		hours
	>;

	struct time : quantity<time, seconds, time_units> {
		using quantity::quantity;
	};

	inline time milliseconds(const float value) {
		return time::from<struct milliseconds>(value);
	}
	 
	inline time seconds(const float value) {
		return time::from<struct seconds>(value);
	}

	inline time minutes(const float value) {
		return time::from<struct minutes>(value);
	}

	inline time hours(const float value) {
		return time::from<struct hours>(value);
	}
}
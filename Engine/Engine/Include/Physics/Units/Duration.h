#pragma once

#include "Physics/Units/UnitTemplate.h"

namespace gse {
	constexpr char millisecondsUnits[] = "ms";
	constexpr char secondsUnits[] = "s";
	constexpr char minutesUnits[] = "m";
	constexpr char hoursUnits[] = "h";

	using Milliseconds = Unit<float, 0.001f, millisecondsUnits>;
	using Seconds = Unit<float, 1.0f, secondsUnits>;
	using Minutes = Unit<float, 60.0f, minutesUnits>;
	using Hours = Unit<float, 3600.0f, hoursUnits>;
}

namespace gse {
	using TimeUnits = UnitList<
		Milliseconds,
		Seconds,
		Minutes,
		Hours
	>;

	struct Time : Quantity<Time, Seconds, TimeUnits> {
		using Quantity::Quantity;
	};

	inline Time milliseconds(const float value) {
		return Time::from<Milliseconds>(value);
	}

	inline Time seconds(const float value) {
		return Time::from<Seconds>(value);
	}

	inline Time minutes(const float value) {
		return Time::from<Minutes>(value);
	}

	inline Time hours(const float value) {
		return Time::from<Hours>(value);
	}
}
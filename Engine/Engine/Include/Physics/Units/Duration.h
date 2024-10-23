#pragma once

#include "Physics/Units/UnitTemplate.h"

namespace Engine::Units {
	constexpr char milliseconds[] = "ms";
	constexpr char seconds[] = "s";
	constexpr char minutes[] = "m";
	constexpr char hours[] = "h";

	using Milliseconds = Unit<float, 0.001f, milliseconds>;
	using Seconds = Unit<float, 1.0f, seconds>;
	using Minutes = Unit<float, 60.0f, minutes>;
	using Hours = Unit<float, 3600.0f, hours>;
}

namespace Engine {
	using TimeUnits = UnitList<
		Units::Milliseconds,
		Units::Seconds,
		Units::Minutes,
		Units::Hours
	>;

	struct Time : Quantity<Time, Units::Seconds, TimeUnits> {
		using Quantity::Quantity;
	};
}
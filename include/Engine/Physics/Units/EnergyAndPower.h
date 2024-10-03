#pragma once

#include "Engine/Physics/Units/UnitTemplate.h"

// Energy

namespace Engine::Units {
	using Joules = Unit<float, 1.0f>;
	using Kilojoules = Unit<float, 1000.0f>;
	using Megajoules = Unit<float, 1000000.0f>;
	using Gigajoules = Unit<float, 1000000000.0f>;
	using Calories = Unit<float, 4184.0f>;
	using Kilocalories = Unit<float, 4184000.0f>;
}

namespace Engine {
	struct Energy : Quantity<Units::Joules> {
		using Quantity::Quantity;
	};
}

// Power

namespace Engine::Units {
	using Watts = Unit<float, 1.0f>;
	using Kilowatts = Unit<float, 1000.0f>;
	using Megawatts = Unit<float, 1000000.0f>;
	using Gigawatts = Unit<float, 1000000000.0f>;
	using Horsepower = Unit<float, 745.7f>;
}

namespace Engine {
	struct Power : Quantity<Units::Watts> {
		using Quantity::Quantity;
	};
}
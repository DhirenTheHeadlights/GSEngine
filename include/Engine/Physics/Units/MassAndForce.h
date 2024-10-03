#pragma once

#include "Engine/Physics/Units/UnitTemplate.h"

// Mass

namespace Engine::Units {
	using Kilograms = Unit<float, 1.0f>;
	using Grams = Unit<float, 0.001f>;
	using Pounds = Unit<float, 0.453592f>;
}

namespace Engine {
	struct Mass : Quantity<Units::Kilograms> {
		using Quantity::Quantity;
	};
}

// Force

namespace Engine::Units {
	using Newtons = Unit<float, 1.0f>;
	using PoundsForce = Unit<float, 4.44822f>;
}

namespace Engine {
	struct Force : Quantity<Units::Newtons> {
		using Quantity::Quantity;
	};
}
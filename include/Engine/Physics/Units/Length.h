#pragma once

#include "Engine/Physics/Units/UnitTemplate.h"

namespace Engine::Length {
	// Define specific unit types
	using Kilometers = Unit<float, 1000.0f>;
	using Meters = Unit<float, 1.0f>;
	using Centimeters = Unit<float, 0.01f>;
	using Millimeters = Unit<float, 0.001f>;
	using Yards = Unit<float, 0.9144f>;
	using Feet = Unit<float, 0.3048f>;
	using Inches = Unit<float, 0.0254f>;
}

namespace Engine {
	struct Length : public Quantity<Meters> {
		using Quantity::Quantity;
	};
}

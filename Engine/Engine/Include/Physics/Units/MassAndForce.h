#pragma once

#include "Physics/Units/UnitTemplate.h"

// Mass

namespace gse {
	struct MassTag {};

	constexpr char kilogramsUnits[] = "kg";
	constexpr char gramsUnits[] = "g";
	constexpr char poundsUnits[] = "lb";

	using Kilograms = Unit<MassTag, 1.0f, kilogramsUnits>;
	using Grams = Unit<MassTag, 0.001f, gramsUnits>;
	using Pounds = Unit<MassTag, 0.453592f, poundsUnits>;
}

namespace gse {
	using MassUnits = UnitList<
		Kilograms,
		Grams,
		Pounds
	>;

	struct Mass : Quantity<Mass, Kilograms, MassUnits> {
		using Quantity::Quantity;
	};

	inline Mass kilograms(const float value) {
		return Mass::from<Kilograms>(value);
	}

	inline Mass grams(const float value) {
		return Mass::from<Grams>(value);
	}

	inline Mass pounds(const float value) {
		return Mass::from<Pounds>(value);
	}
}

// Force

namespace gse {
	struct ForceTag {};

	constexpr char newtonsUnits[] = "N";
	constexpr char poundsForceUnits[] = "lbf";

	using Newtons = Unit<ForceTag, 1.0f, newtonsUnits>;
	using PoundsForce = Unit<ForceTag, 4.44822f, poundsForceUnits>;
}

namespace gse {
	using ForceUnits = UnitList<
		Newtons,
		PoundsForce
	>;

	struct Force : Quantity<Force, Newtons, ForceUnits> {
		using Quantity::Quantity;
	};

	inline Force newtons(const float value) {
		return Force::from<Newtons>(value);
	}

	inline Force poundsForce(const float value) {
		return Force::from<PoundsForce>(value);
	}
}
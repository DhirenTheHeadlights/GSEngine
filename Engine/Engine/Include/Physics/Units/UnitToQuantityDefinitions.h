#pragma once

#include "Physics/Units/UnitTemplate.h"
#include "Physics/Units/Units.h"

// Default case: Assume the type is already a Quantity
template <typename t>
struct unit_to_quantity {
	using type = t;  
};

// Macro to define the relationship between a unit and a quantity
// Example: DEFINE_UNIT_TO_QUANTITY(Meters, Length);
#define DEFINE_UNIT_TO_QUANTITY(unit_type, quantity_type)		 \
	    template <>                                          \
	    struct unit_to_quantity<unit_type> {					 \
	        using type = quantity_type;                       \
	    }; 													 \

////////////////////////////////////////////////////////////////////////
/// Length
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(struct gse::kilometers, gse::length);
DEFINE_UNIT_TO_QUANTITY(struct gse::meters, gse::length);
DEFINE_UNIT_TO_QUANTITY(struct gse::centimeters, gse::length);
DEFINE_UNIT_TO_QUANTITY(struct gse::millimeters, gse::length);
DEFINE_UNIT_TO_QUANTITY(struct gse::yards, gse::length);
DEFINE_UNIT_TO_QUANTITY(struct gse::feet, gse::length);
DEFINE_UNIT_TO_QUANTITY(struct gse::inches, gse::length);
////////////////////////////////////////////////////////////////////////
/// Mass
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(struct gse::kilograms, gse::mass);
DEFINE_UNIT_TO_QUANTITY(struct gse::grams, gse::mass);
DEFINE_UNIT_TO_QUANTITY(struct gse::pounds, gse::mass);
////////////////////////////////////////////////////////////////////////
/// Force
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(struct gse::newtons, gse::force);
DEFINE_UNIT_TO_QUANTITY(struct gse::pounds_force, gse::force);
////////////////////////////////////////////////////////////////////////
/// Energy
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(struct gse::joules, gse::energy);
DEFINE_UNIT_TO_QUANTITY(struct gse::kilojoules, gse::energy);
DEFINE_UNIT_TO_QUANTITY(struct gse::megajoules, gse::energy);
DEFINE_UNIT_TO_QUANTITY(struct gse::gigajoules, gse::energy);
DEFINE_UNIT_TO_QUANTITY(struct gse::calories, gse::energy);
DEFINE_UNIT_TO_QUANTITY(struct gse::kilocalories, gse::energy);
////////////////////////////////////////////////////////////////////////
/// Power
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(struct gse::watts, gse::power);
DEFINE_UNIT_TO_QUANTITY(struct gse::kilowatts, gse::power);
DEFINE_UNIT_TO_QUANTITY(struct gse::megawatts, gse::power);
DEFINE_UNIT_TO_QUANTITY(struct gse::gigawatts, gse::power);
DEFINE_UNIT_TO_QUANTITY(struct gse::horsepower, gse::power);
////////////////////////////////////////////////////////////////////////
/// Velocity
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(struct gse::meters_per_second, gse::velocity);
DEFINE_UNIT_TO_QUANTITY(struct gse::kilometers_per_hour, gse::velocity);
DEFINE_UNIT_TO_QUANTITY(struct gse::miles_per_hour, gse::velocity);
DEFINE_UNIT_TO_QUANTITY(struct gse::feet_per_second, gse::velocity);
////////////////////////////////////////////////////////////////////////
/// Acceleration
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(struct gse::meters_per_second_squared, gse::acceleration);
DEFINE_UNIT_TO_QUANTITY(struct gse::kilometers_per_hour_squared, gse::acceleration);
DEFINE_UNIT_TO_QUANTITY(struct gse::miles_per_hour_squared, gse::acceleration);
DEFINE_UNIT_TO_QUANTITY(struct gse::feet_per_second_squared, gse::acceleration);
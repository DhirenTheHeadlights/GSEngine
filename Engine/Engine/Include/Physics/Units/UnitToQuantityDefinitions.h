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
DEFINE_UNIT_TO_QUANTITY(gse::units::kilometers, gse::length);
DEFINE_UNIT_TO_QUANTITY(gse::units::meters, gse::length);
DEFINE_UNIT_TO_QUANTITY(gse::units::centimeters, gse::length);
DEFINE_UNIT_TO_QUANTITY(gse::units::millimeters, gse::length);
DEFINE_UNIT_TO_QUANTITY(gse::units::yards, gse::length);
DEFINE_UNIT_TO_QUANTITY(gse::units::feet, gse::length);
DEFINE_UNIT_TO_QUANTITY(gse::units::inches, gse::length);
////////////////////////////////////////////////////////////////////////
/// Mass
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(gse::units::kilograms, gse::mass);
DEFINE_UNIT_TO_QUANTITY(gse::units::grams, gse::mass);
DEFINE_UNIT_TO_QUANTITY(gse::units::pounds, gse::mass);
////////////////////////////////////////////////////////////////////////
/// Force
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(gse::units::newtons, gse::force);
DEFINE_UNIT_TO_QUANTITY(gse::units::pounds_force, gse::force);
////////////////////////////////////////////////////////////////////////
/// Energy
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(gse::units::joules, gse::energy);
DEFINE_UNIT_TO_QUANTITY(gse::units::kilojoules, gse::energy);
DEFINE_UNIT_TO_QUANTITY(gse::units::megajoules, gse::energy);
DEFINE_UNIT_TO_QUANTITY(gse::units::gigajoules, gse::energy);
DEFINE_UNIT_TO_QUANTITY(gse::units::calories, gse::energy);
DEFINE_UNIT_TO_QUANTITY(gse::units::kilocalories, gse::energy);
////////////////////////////////////////////////////////////////////////
/// Power
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(gse::units::watts, gse::power);
DEFINE_UNIT_TO_QUANTITY(gse::units::kilowatts, gse::power);
DEFINE_UNIT_TO_QUANTITY(gse::units::megawatts, gse::power);
DEFINE_UNIT_TO_QUANTITY(gse::units::gigawatts, gse::power);
DEFINE_UNIT_TO_QUANTITY(gse::units::horsepower, gse::power);
////////////////////////////////////////////////////////////////////////
/// Velocity
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(gse::units::meters_per_second, gse::velocity);
DEFINE_UNIT_TO_QUANTITY(gse::units::kilometers_per_hour, gse::velocity);
DEFINE_UNIT_TO_QUANTITY(gse::units::miles_per_hour, gse::velocity);
DEFINE_UNIT_TO_QUANTITY(gse::units::feet_per_second, gse::velocity);
////////////////////////////////////////////////////////////////////////
/// Acceleration
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(gse::units::meters_per_second_squared, gse::acceleration);
DEFINE_UNIT_TO_QUANTITY(gse::units::kilometers_per_hour_squared, gse::acceleration);
DEFINE_UNIT_TO_QUANTITY(gse::units::miles_per_hour_squared, gse::acceleration);
DEFINE_UNIT_TO_QUANTITY(gse::units::feet_per_second_squared, gse::acceleration);
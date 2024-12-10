#pragma once

#include "Physics/Units/UnitTemplate.h"
#include "Physics/Units/Units.h"

// Default case: Assume the type is already a Quantity
template <typename T>
struct UnitToQuantity {
	using Type = T;  
};

// Macro to define the relationship between a unit and a quantity
// Example: DEFINE_UNIT_TO_QUANTITY(Meters, Length);
#define DEFINE_UNIT_TO_QUANTITY(UnitType, QuantityType)		 \
	    template <>                                          \
	    struct UnitToQuantity<UnitType> {					 \
	        using Type = QuantityType;                       \
	    }; 													 \

////////////////////////////////////////////////////////////////////////
/// Length
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(gse::Kilometers, gse::Length);
DEFINE_UNIT_TO_QUANTITY(gse::Meters, gse::Length);
DEFINE_UNIT_TO_QUANTITY(gse::Centimeters, gse::Length);
DEFINE_UNIT_TO_QUANTITY(gse::Millimeters, gse::Length);
DEFINE_UNIT_TO_QUANTITY(gse::Yards, gse::Length);
DEFINE_UNIT_TO_QUANTITY(gse::Feet, gse::Length);
DEFINE_UNIT_TO_QUANTITY(gse::Inches, gse::Length);
////////////////////////////////////////////////////////////////////////
/// Mass
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(gse::Kilograms, gse::Mass);
DEFINE_UNIT_TO_QUANTITY(gse::Grams, gse::Mass);
DEFINE_UNIT_TO_QUANTITY(gse::Pounds, gse::Mass);
////////////////////////////////////////////////////////////////////////
/// Force
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(gse::Newtons, gse::Force);
DEFINE_UNIT_TO_QUANTITY(gse::PoundsForce, gse::Force);
////////////////////////////////////////////////////////////////////////
/// Energy
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(gse::Joules, gse::Energy);
DEFINE_UNIT_TO_QUANTITY(gse::Kilojoules, gse::Energy);
DEFINE_UNIT_TO_QUANTITY(gse::Megajoules, gse::Energy);
DEFINE_UNIT_TO_QUANTITY(gse::Gigajoules, gse::Energy);
DEFINE_UNIT_TO_QUANTITY(gse::Calories, gse::Energy);
DEFINE_UNIT_TO_QUANTITY(gse::Kilocalories, gse::Energy);
////////////////////////////////////////////////////////////////////////
/// Power
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(gse::Watts, gse::Power);
DEFINE_UNIT_TO_QUANTITY(gse::Kilowatts, gse::Power);
DEFINE_UNIT_TO_QUANTITY(gse::Megawatts, gse::Power);
DEFINE_UNIT_TO_QUANTITY(gse::Gigawatts, gse::Power);
DEFINE_UNIT_TO_QUANTITY(gse::Horsepower, gse::Power);
////////////////////////////////////////////////////////////////////////
/// Velocity
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(gse::MetersPerSecond, gse::Velocity);
DEFINE_UNIT_TO_QUANTITY(gse::KilometersPerHour, gse::Velocity);
DEFINE_UNIT_TO_QUANTITY(gse::MilesPerHour, gse::Velocity);
DEFINE_UNIT_TO_QUANTITY(gse::FeetPerSecond, gse::Velocity);
////////////////////////////////////////////////////////////////////////
/// Acceleration
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(gse::MetersPerSecondSquared, gse::Acceleration);
DEFINE_UNIT_TO_QUANTITY(gse::KilometersPerHourSquared, gse::Acceleration);
DEFINE_UNIT_TO_QUANTITY(gse::MilesPerHourSquared, gse::Acceleration);
DEFINE_UNIT_TO_QUANTITY(gse::FeetPerSecondSquared, gse::Acceleration);
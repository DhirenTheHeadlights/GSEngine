#pragma once

#include "Engine/Physics/Units/UnitTemplate.h"
#include "Engine/Physics/Units/Units.h"

// Default case: Assume the type is already a Quantity
template <typename T>
struct UnitToQuantity {
	using Type = T;  
};

// Macro to define the relationship between a unit and a quantity
// Example: DEFINE_UNIT_TO_QUANTITY(Units::Meters, Length);
#define DEFINE_UNIT_TO_QUANTITY(UnitType, QuantityType)		 \
	    template <>                                          \
	    struct UnitToQuantity<UnitType> {					 \
	        using Type = QuantityType;                       \
	    }; 													 \

////////////////////////////////////////////////////////////////////////
/// Length
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(Engine::Units::Kilometers, Engine::Length);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::Meters, Engine::Length);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::Centimeters, Engine::Length);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::Millimeters, Engine::Length);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::Yards, Engine::Length);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::Feet, Engine::Length);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::Inches, Engine::Length);
////////////////////////////////////////////////////////////////////////
/// Mass
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(Engine::Units::Kilograms, Engine::Mass);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::Grams, Engine::Mass);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::Pounds, Engine::Mass);
////////////////////////////////////////////////////////////////////////
/// Force
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(Engine::Units::Newtons, Engine::Force);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::PoundsForce, Engine::Force);
////////////////////////////////////////////////////////////////////////
/// Energy
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(Engine::Units::Joules, Engine::Energy);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::Kilojoules, Engine::Energy);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::Megajoules, Engine::Energy);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::Gigajoules, Engine::Energy);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::Calories, Engine::Energy);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::Kilocalories, Engine::Energy);
////////////////////////////////////////////////////////////////////////
/// Power
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(Engine::Units::Watts, Engine::Power);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::Kilowatts, Engine::Power);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::Megawatts, Engine::Power);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::Gigawatts, Engine::Power);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::Horsepower, Engine::Power);
////////////////////////////////////////////////////////////////////////
/// Velocity
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(Engine::Units::MetersPerSecond, Engine::Velocity);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::KilometersPerHour, Engine::Velocity);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::MilesPerHour, Engine::Velocity);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::FeetPerSecond, Engine::Velocity);
////////////////////////////////////////////////////////////////////////
/// Acceleration
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(Engine::Units::MetersPerSecondSquared, Engine::Acceleration);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::KilometersPerHourSquared, Engine::Acceleration);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::MilesPerHourSquared, Engine::Acceleration);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::FeetPerSecondSquared, Engine::Acceleration);
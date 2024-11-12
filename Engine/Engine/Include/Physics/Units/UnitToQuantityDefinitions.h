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
DEFINE_UNIT_TO_QUANTITY(Engine::Kilometers, Engine::Length);
DEFINE_UNIT_TO_QUANTITY(Engine::Meters, Engine::Length);
DEFINE_UNIT_TO_QUANTITY(Engine::Centimeters, Engine::Length);
DEFINE_UNIT_TO_QUANTITY(Engine::Millimeters, Engine::Length);
DEFINE_UNIT_TO_QUANTITY(Engine::Yards, Engine::Length);
DEFINE_UNIT_TO_QUANTITY(Engine::Feet, Engine::Length);
DEFINE_UNIT_TO_QUANTITY(Engine::Inches, Engine::Length);
////////////////////////////////////////////////////////////////////////
/// Mass
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(Engine::Kilograms, Engine::Mass);
DEFINE_UNIT_TO_QUANTITY(Engine::Grams, Engine::Mass);
DEFINE_UNIT_TO_QUANTITY(Engine::Pounds, Engine::Mass);
////////////////////////////////////////////////////////////////////////
/// Force
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(Engine::Newtons, Engine::Force);
DEFINE_UNIT_TO_QUANTITY(Engine::PoundsForce, Engine::Force);
////////////////////////////////////////////////////////////////////////
/// Energy
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(Engine::Joules, Engine::Energy);
DEFINE_UNIT_TO_QUANTITY(Engine::Kilojoules, Engine::Energy);
DEFINE_UNIT_TO_QUANTITY(Engine::Megajoules, Engine::Energy);
DEFINE_UNIT_TO_QUANTITY(Engine::Gigajoules, Engine::Energy);
DEFINE_UNIT_TO_QUANTITY(Engine::Calories, Engine::Energy);
DEFINE_UNIT_TO_QUANTITY(Engine::Kilocalories, Engine::Energy);
////////////////////////////////////////////////////////////////////////
/// Power
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(Engine::Watts, Engine::Power);
DEFINE_UNIT_TO_QUANTITY(Engine::Kilowatts, Engine::Power);
DEFINE_UNIT_TO_QUANTITY(Engine::Megawatts, Engine::Power);
DEFINE_UNIT_TO_QUANTITY(Engine::Gigawatts, Engine::Power);
DEFINE_UNIT_TO_QUANTITY(Engine::Horsepower, Engine::Power);
////////////////////////////////////////////////////////////////////////
/// Velocity
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(Engine::MetersPerSecond, Engine::Velocity);
DEFINE_UNIT_TO_QUANTITY(Engine::KilometersPerHour, Engine::Velocity);
DEFINE_UNIT_TO_QUANTITY(Engine::MilesPerHour, Engine::Velocity);
DEFINE_UNIT_TO_QUANTITY(Engine::FeetPerSecond, Engine::Velocity);
////////////////////////////////////////////////////////////////////////
/// Acceleration
////////////////////////////////////////////////////////////////////////
DEFINE_UNIT_TO_QUANTITY(Engine::MetersPerSecondSquared, Engine::Acceleration);
DEFINE_UNIT_TO_QUANTITY(Engine::KilometersPerHourSquared, Engine::Acceleration);
DEFINE_UNIT_TO_QUANTITY(Engine::MilesPerHourSquared, Engine::Acceleration);
DEFINE_UNIT_TO_QUANTITY(Engine::FeetPerSecondSquared, Engine::Acceleration);
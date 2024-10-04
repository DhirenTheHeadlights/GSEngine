#include "Engine/Physics/Units/Movement.h"

/// Define reverse-mapping from unit to quantity

DEFINE_UNIT_TO_QUANTITY(Engine::Units::MetersPerSecond, Engine::Velocity);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::KilometersPerHour, Engine::Velocity);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::MilesPerHour, Engine::Velocity);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::FeetPerSecond, Engine::Velocity);

DEFINE_UNIT_TO_QUANTITY(Engine::Units::MetersPerSecondSquared, Engine::Acceleration);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::KilometersPerHourSquared, Engine::Acceleration);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::MilesPerHourSquared, Engine::Acceleration);
DEFINE_UNIT_TO_QUANTITY(Engine::Units::FeetPerSecondSquared, Engine::Acceleration);

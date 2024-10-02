#pragma once

namespace Engine {
	template <typename T, float ConversionFactor>
	struct Unit {
		explicit Unit(const float value) : value(value) {}
		float getValue() const { return value * ConversionFactor; }
	private:
		float value;
	};

	template <typename BaseUnitType>
	struct Quantity {
		Quantity() : value(0.0f) {}

		template <typename UnitType>
		explicit Quantity(const UnitType& unit) : value(unit.getValue()) {}

		// Conversion function to convert to any other unit type
		template <typename UnitType>
		float as() const {
			return value / UnitType(1.0f).getValue();
		}

		// Arithmetic operators
		Quantity operator+(const Quantity& other) const { return Quantity(value + other.value); }
		Quantity operator-(const Quantity& other) const { return Quantity(value - other.value); }
		Quantity operator*(const float scalar) const { return Quantity(value * scalar); }
		Quantity operator/(const float scalar) const { return Quantity(value / scalar); }
		Quantity& operator+=(const Quantity& other) { value += other.value; return *this; }
		Quantity& operator-=(const Quantity& other) { value -= other.value; return *this; }
		Quantity& operator*=(const float scalar) { value *= scalar; return *this; }
		Quantity& operator/=(const float scalar) { value /= scalar; return *this; }
	private:
		float value;  // Stored in base units
	};

}
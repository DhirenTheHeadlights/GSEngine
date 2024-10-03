#pragma once

namespace Engine {
	template <typename T, float ConversionFactor>
	struct Unit {
		explicit Unit(const float value) : value(value) {}
		float getValue() const { return value * ConversionFactor; }
	private:
		float value;
	};

	template <typename T>
	concept IsUnit = requires(T t) {
		{ t.getValue() } -> std::convertible_to<float>;
	};

	template <typename UnitType>
	struct Quantity {
		Quantity() : value(0.0f) {}

		template <IsUnit Unit>
		explicit Quantity(const Unit& unit) : value(unit.getValue()) {}

		// Conversion function to convert to any other unit type
		template <IsUnit Unit>
		float as() const {
			return value / Unit(1.0f).getValue();
		}

		// Assignment operator overload
		template <IsUnit Unit>
		Quantity& operator=(const Unit& unit) {
			value = unit.getValue();
			return *this;
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
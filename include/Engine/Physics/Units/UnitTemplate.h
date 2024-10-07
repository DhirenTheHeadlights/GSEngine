#pragma once
#include <concepts>
#include <sstream>
#include <type_traits>
#include <tuple>
#include <string>

namespace Engine {
	template<typename... Units>
	struct UnitList {
		using Type = std::tuple<Units...>;
	};

	template <typename QuantityTag, float ConversionFactor, const char* UnitName>
	struct Unit {
		Unit() = default;											// Default constructor: Equivalent to 0.0f
		explicit Unit(const float value) : value(value) {}			// Constructor with value

		using QuantityTagType = QuantityTag;

		// Access the value in the unit
		[[nodiscard]] float getValue() const { return value * ConversionFactor; }	

		// Light weight, compile time string representation of the unit
		static constexpr const char* units() { return UnitName; }

		// Explicit string representation of the value and unit
		// Example: "5.0 m"
		// Caution: Can be expensive if used in performance critical code
		[[nodiscard]] std::string toString() const {
			std::ostringstream oss;
			oss << value << " " << UnitName;
			return oss.str();
		}

		friend std::ostream& operator<<(std::ostream& os, const Unit& unit) {
			os << unit.toString();
			return os;
		}
	private:
		float value = 0.0f;
	};

	// Concept to check if a type is a valid unit
	template <typename T>
	concept IsUnit = requires {
		typename T::QuantityTagType;							// Units have QuantityTagType
		{ T::units() } -> std::convertible_to<const char*>;		// Optional: further ensures T is a Unit
	}&& requires(T t) {
		{ t.getValue() } -> std::convertible_to<float>;
	};

	template <typename UnitType, typename ValidUnits>
	constexpr bool isValidUnitForQuantity() {
		return std::apply([]<typename... Units>(Units... u) {
			return ((std::is_same_v<typename UnitType::QuantityTagType, typename Units::QuantityTagType>) || ...);
		}, typename ValidUnits::Type{});
	}

	template <IsUnit Default, typename ValidUnits>
	struct Quantity {
		using Units = ValidUnits;

		using DefaultUnit = Default;

		Quantity() : value(0.0f) {}

		// Constructs a quantity from a value - default unit is assumed
		explicit Quantity(const float value) : value(value) {}

		explicit Quantity(const DefaultUnit& unit) : value(unit.getValue()) {
			// We skip validity check since DefaultUnit is inherently valid
		}

		template <IsUnit Unit>
		explicit Quantity(const Unit& unit) : value(unit.getValue()) {
			static_assert(isValidUnitForQuantity<Unit, ValidUnits>(), "Invalid unit type for this quantity");
		}

		// Conversion function to convert to any other valid unit
		template <IsUnit Unit>
		float as() const {
			static_assert(isValidUnitForQuantity<Unit, ValidUnits>(), "Invalid unit type for conversion");
			return Unit(value.getValue()).getValue();
		}

		// Assignment operator overload
		template <IsUnit Unit>
		Quantity& operator=(const Unit& unit) {
			static_assert(isValidUnitForQuantity<Unit, ValidUnits>(), "Invalid unit type for assignment");
			value = unit.getValue();
			return *this;
		}

		// Arithmetic operators
		Quantity operator+(const Quantity& other) const {
			return Quantity(value + other.value);
		}

		Quantity operator-(const Quantity& other) const {
			return Quantity(value - other.value);
		}

		Quantity operator*(const float scalar) const {
			return Quantity(value * scalar);
		}

		glm::vec3 operator*(const glm::vec3& vec) const {
			return { value * vec.x, value * vec.y, value * vec.z };
		}

		Quantity operator/(const float scalar) const {
			return Quantity(value / scalar);
		}

		Quantity& operator+=(const Quantity& other) {
			value = DefaultUnit(value.getValue() + other.value.getValue());
			return *this;
		}

		Quantity& operator-=(const Quantity& other) {
			value = DefaultUnit(value.getValue() - other.value.getValue());
			return *this;
		}

		Quantity& operator*=(const float scalar) {
			value = DefaultUnit(value.getValue() * scalar);
			return *this;
		}

		Quantity& operator/=(const float scalar) {
			value = DefaultUnit(value.getValue() / scalar);
			return *this;
		}

	private:
		DefaultUnit value = 0;  // Stored in base units
	};
}



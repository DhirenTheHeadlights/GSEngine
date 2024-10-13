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
		Unit() = default;                                            // Default constructor: Equivalent to 0.0f
		Unit(const float value) : value(value) {}					 // Constructor with value

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

		Unit& operator=(const float newValue) {
			value = newValue;
			return *this;
		}

		// Arithmetic operators
		Unit operator+(const Unit& other) const { return Unit(value + other.value); }
		Unit operator-(const Unit& other) const { return Unit(value - other.value); }
		Unit operator*(const float scalar) const { return Unit(value * scalar); }
		Unit operator/(const float scalar) const { return Unit(value / scalar); }

		// Compound assignment operators
		Unit& operator+=(const Unit& other) { value += other.value; return *this; }
		Unit& operator-=(const Unit& other) { value -= other.value; return *this; }
		Unit& operator*=(const float scalar) { value *= scalar; return *this; }
		Unit& operator/=(const float scalar) { value /= scalar; return *this; }

		operator float() const { return getValue(); } // Implicit conversion to float
	private:
		float value = 0.0f;
	};

	// Concept to check if a type is a valid unit
	template <typename T>
	concept IsUnit = requires {
		typename T::QuantityTagType;                            // Units have QuantityTagType
		{ T::units() } -> std::convertible_to<const char*>;     // Optional: further ensures T is a Unit
	}&& requires(T t) {
		{ t.getValue() } -> std::convertible_to<float>;
	};

	template <typename UnitType, typename ValidUnits>
	constexpr bool isValidUnitForQuantity() {
		return std::apply([]<typename... Units>(Units... u) {
			return ((std::is_same_v<typename UnitType::QuantityTagType, typename Units::QuantityTagType>) || ...);
		}, typename ValidUnits::Type{});
	}

	template <typename Derived, IsUnit Default, typename ValidUnits>
	struct Quantity {
		using Units = ValidUnits;
		using DefaultUnit = Default;

		// Constructors
		Quantity() = default;
		explicit Quantity(const float value) : value(value) {}

		template <IsUnit Unit>
		Quantity(const Unit& unit) {
			static_assert(isValidUnitForQuantity<Unit, ValidUnits>(), "Invalid unit type for this quantity");
			value = unit.getValue();  // Store in base unit
		}

		// Conversion function to convert to any other valid unit
		template <IsUnit Unit>
		float as() const {
			static_assert(isValidUnitForQuantity<Unit, ValidUnits>(), "Invalid unit type for conversion");
			return Unit(value).getValue();
		}

		DefaultUnit asDefaultUnit() const {
			return DefaultUnit(value);
		}

		// Assignment operator overload
		template <IsUnit Unit>
		Derived& operator=(const Unit& unit) {
			static_assert(isValidUnitForQuantity<Unit, ValidUnits>(), "Invalid unit type for assignment");
			value = unit.getValue();
			return static_cast<Derived&>(*this);
		}

		/// Arithmetic operators
		Derived operator+(const Derived& other) const {
			return Derived(value + other.value);
		}

		Derived operator-(const Derived& other) const {
			return Derived(value - other.value);
		}

		Derived operator*(const float scalar) const {
			return Derived(value * scalar);
		}

		Derived operator/(const float scalar) const {
			return Derived(value / scalar);
		}

		/// Compound assignment operators
		Derived& operator+=(const Derived& other) {
			value += other.value;
			return static_cast<Derived&>(*this);
		}

		Derived& operator-=(const Derived& other) {
			value -= other.value;
			return static_cast<Derived&>(*this);
		}

		Derived& operator*=(const float scalar) {
			value *= scalar;
			return static_cast<Derived&>(*this);
		}

		Derived& operator/=(const float scalar) {
			value /= scalar;
			return static_cast<Derived&>(*this);
		}

		// Comparison operators
		bool operator==(const Derived& other) const {
			return value == other.value;
		}

		bool operator!=(const Derived& other) const {
			return value != other.value;
		}

		bool operator<(const Derived& other) const {
			return value < other.value;
		}

		bool operator>(const Derived& other) const {
			return value > other.value;
		}

		bool operator<=(const Derived& other) const {
			return value <= other.value;
		}

		bool operator>=(const Derived& other) const {
			return value >= other.value;
		}
	protected:
		float value = 0.0f;  // Stored in base units
	};
}

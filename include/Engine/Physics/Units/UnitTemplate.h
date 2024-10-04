#pragma once
#include <concepts>
#include <sstream>
#include <type_traits>
#include <tuple>
#include <string>

template <typename T>
struct UnitToQuantity;

// Macro to define the relationship between a unit and a quantity
// Example: DEFINE_UNIT_TO_QUANTITY(Units::Meters, Length);

#define DEFINE_UNIT_TO_QUANTITY(UnitType, QuantityType)		 \
	    template <>                                          \
	    struct UnitToQuantity<UnitType> {					 \
	        using Type = QuantityType;                       \
	    };

namespace Engine {
	template<typename... Units>
	struct UnitList {
		using Type = std::tuple<Units...>;
	};

	template <typename T, float ConversionFactor, const char* UnitName>
	struct Unit {
		Unit() = default;											// Default constructor: Equivalent to 0.0f
		explicit Unit(const float value) : value(value) {}			// Constructor with value

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
	concept IsUnit = requires(T t) {
		{ t.getValue() } -> std::convertible_to<float>;
	};

	// Concept to check if a type is a valid quantity
	template <typename Unit, typename Tuple>
	struct is_in_tuple;

	// Helper to check if a type is in a tuple
	template <typename Unit, typename... Types>
	struct is_in_tuple<Unit, std::tuple<Types...>> : std::disjunction<std::is_same<Unit, Types>...> {};

	template <typename ValidUnits>
	struct Quantity {
		using Units = typename ValidUnits::Type;

		Quantity() : value(0.0f) {}

		template <IsUnit Unit>
		explicit Quantity(const Unit& unit) : value(unit.getValue()) {
			static_assert(isValidUnit<Unit>(), "Invalid unit type for this quantity");
		}

		// Conversion function to convert to any other unit type
		template <typename Unit>
		float as() const {
			static_assert(isValidUnit<Unit>(), "Invalid unit type for conversion");
			return value / Unit(1.0f).getValue();
		}

		// Assignment operator overload
		template <IsUnit Unit>
		Quantity& operator=(const Unit& unit) {
			static_assert(isValidUnit<Unit>(), "Invalid unit type for assignment");
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

		Quantity operator/(const float scalar) const {
			return Quantity(value / scalar);
		}

		Quantity& operator+=(const Quantity& other) {
			value += other.value;
			return *this;
		}

		Quantity& operator-=(const Quantity& other) {
			value -= other.value;
			return *this;
		}

		Quantity& operator*=(const float scalar) {
			value *= scalar;
			return *this;
		}

		Quantity& operator/=(const float scalar) {
			value /= scalar;
			return *this;
		}

	private:
		float value;  // Stored in base units

		explicit Quantity(const float value) : value(value) {}  // Private constructor for internal arithmetic

		// Helper to check if Unit is valid for this Quantity
		template <typename Unit>
		static constexpr bool isValidUnit() {
			return is_in_tuple<Unit, Units>::value;
		}
	};
}



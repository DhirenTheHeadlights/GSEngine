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

	template <typename QuantityTagType, float ConversionFactorType, const char UnitNameType[]>
	struct Unit {
		using QuantityTag = QuantityTagType;
		static constexpr float ConversionFactor = ConversionFactorType;
		static constexpr const char* UnitName = UnitNameType;
	};

	template <typename T>
	concept IsUnit = requires {
		typename std::remove_cvref_t<T>::QuantityTag;
		{ std::remove_cvref_t<T>::UnitName } -> std::convertible_to<const char*>;
		{ std::remove_cvref_t<T>::ConversionFactor } -> std::convertible_to<float>;
	};

	template <typename UnitType, typename ValidUnits>
	constexpr bool isValidUnitForQuantity() {
		return std::apply([]<typename... Units>(Units... u) {
			return ((std::is_same_v<typename UnitType::QuantityTag, typename Units::QuantityTag>) || ...);
		}, typename ValidUnits::Type{});
	}

	template <typename Derived, IsUnit Default, typename ValidUnits>
	struct Quantity {
		using Units = ValidUnits;
		using DefaultUnit = Default;

		Quantity() = default;

		template <IsUnit Unit>
		void set(const float value) {
			static_assert(isValidUnitForQuantity<Unit, ValidUnits>(), "Invalid unit type for assignment");
			val = getConvertedValue<Unit>(value);
		}

		// Convert to any other valid unit
		template <IsUnit Unit>
		float as() const {
			static_assert(isValidUnitForQuantity<Unit, ValidUnits>(), "Invalid unit type for conversion");
			return val / Unit::ConversionFactor;
		}

		float asDefaultUnit() const {
			return val;
		}

		// Assignment operator overload
		template <IsUnit Unit>
		Derived& operator=(const float value) {
			static_assert(isValidUnitForQuantity<Unit, ValidUnits>(), "Invalid unit type for assignment");
			val = getConvertedValue<Unit>(value);
			return static_cast<Derived&>(*this);
		}

		/// Arithmetic operators
		Derived operator+(const Derived& other) const {
			return Derived(val + other.val);
		}

		Derived operator-(const Derived& other) const {
			return Derived(val - other.val);
		}

		Derived operator*(const float scalar) const {
			return Derived(val * scalar);
		}

		Derived operator/(const float scalar) const {
			return Derived(val / scalar);
		}

		/// Compound assignment operators
		Derived& operator+=(const Derived& other) {
			val += other.val;
			return static_cast<Derived&>(*this);
		}

		Derived& operator-=(const Derived& other) {
			val -= other.val;
			return static_cast<Derived&>(*this);
		}

		Derived& operator*=(const float scalar) {
			val *= scalar;
			return static_cast<Derived&>(*this);
		}

		Derived& operator/=(const float scalar) {
			val /= scalar;
			return static_cast<Derived&>(*this);
		}

		// Comparison operators
		bool operator==(const Derived& other) const {
			return val == other.val;
		}

		bool operator!=(const Derived& other) const {
			return val != other.val;
		}

		bool operator<(const Derived& other) const {
			return val < other.val;
		}

		bool operator>(const Derived& other) const {
			return val > other.val;
		}

		bool operator<=(const Derived& other) const {
			return val <= other.val;
		}

		bool operator>=(const Derived& other) const {
			return val >= other.val;
		}

		// Other operators
		Derived operator-() const {
			return Derived(-val);
		}

		template <IsUnit Unit>
		static Derived from(const float value) {
			static_assert(isValidUnitForQuantity<Unit, ValidUnits>(), "Invalid unit type for conversion");
			Derived result;
			result.val = result.template getConvertedValue<Unit>(value);
			return result;
		}
	protected:
		float val = 0.0f;  // Stored in base units

		template <IsUnit T>
		float getConvertedValue(const float value) {
			return value * T::ConversionFactor / Default::ConversionFactor;
		}

		explicit Quantity(const float value) : val(value) {}
	};
}

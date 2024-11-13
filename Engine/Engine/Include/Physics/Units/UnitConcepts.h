#pragma once

namespace Engine {
	template <typename T>
	concept IsUnit = requires {
		typename T::QuantityTag;								   // Units have QuantityTagType
		{ T::UnitName } -> std::convertible_to<const char*>;       // Optional: further ensures T is a Unit
		{ T::ConversionFactor } -> std::convertible_to<float>;     // Optional: further ensures T is a Unit
	};

	template <typename Derived, IsUnit Default, typename ValidUnits>
	struct Quantity;

	template <typename T>
	concept IsQuantity = std::is_base_of_v<Quantity<T, typename T::DefaultUnit, typename T::Units>, T>;

	template <typename T>
	concept IsQuantityOrUnit = IsQuantity<T> || IsUnit<T>;
}
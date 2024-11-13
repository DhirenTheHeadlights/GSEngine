#pragma once

namespace Engine {
	template <typename T>
	concept IsUnit = requires {
		typename T::QuantityTag;								   // Units have QuantityTagType
		{ T::UnitName } -> std::convertible_to<const char*>;       // Optional: further ensures T is a Unit
		{ T::ConversionFactor } -> std::convertible_to<float>;     // Optional: further ensures T is a Unit
	};
}
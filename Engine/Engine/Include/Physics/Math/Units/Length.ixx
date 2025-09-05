export module gse.physics.math:length;

import std;

import :dimension;
import :quant;

namespace gse {
	struct length_tag {};

	export inline constexpr internal::unit<length_tag, 1000.0f, "km"> kilometers;
	export inline constexpr internal::unit<length_tag, 1.0f, "m"> meters;
	export inline constexpr internal::unit<length_tag, 0.01f, "cm"> centimeters;
	export inline constexpr internal::unit<length_tag, 0.001f, "mm"> millimeters;
	export inline constexpr internal::unit<length_tag, 0.9144f, "yd"> yards;
	export inline constexpr internal::unit<length_tag, 0.3048f, "ft"> feet;
	export inline constexpr internal::unit<length_tag, 0.0254f, "in"> inches;
}

export template <>
struct gse::internal::dimension_traits<gse::internal::dim<1, 0, 0>> {
	using tag = length_tag;
	using default_unit = decltype(meters);
};

export namespace gse {
	template <typename T = float>
	using length_t = internal::quantity<T, internal::dim<1, 0, 0>>;
	using length = length_t<>;
}

export template <>
struct gse::internal::quantity_traits<gse::length_tag> {
	template <typename T>
	using type = length_t<T>;
};
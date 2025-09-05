export module gse.physics.math:angle;

import std;
import :dimension;
import :quant;

namespace gse {
	struct angle_tag {};

	export inline constexpr internal::unit<angle_tag, 1.0F, "deg"> degrees;
	export inline constexpr internal::unit<angle_tag, 180.0F / std::numbers::pi, "rad"> radians;
}
export template <>
struct gse::internal::dimension_traits<gse::internal::dim<0, 0, 0>> {
	using tag = angle_tag;
	using default_unit = decltype(degrees);
};

export namespace gse {
	template <typename T = float>
	using angle_t = internal::quantity<T, internal::dim<0, 0, 0>>;
	using angle = angle_t<>;
}

export template <>
struct gse::internal::quantity_traits<gse::angle_tag> {
	template <typename T>
	using type = angle_t<T>;
};
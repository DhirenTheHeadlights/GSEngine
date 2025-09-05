export module gse.physics.math:duration;

import std;
import :dimension;
import :quant;

namespace gse {
	struct time_tag {};

	export inline constexpr internal::unit<time_tag, 3600.0F, "hrs"> hours;
	export inline constexpr internal::unit<time_tag, 60.0F, "min"> minutes;
	export inline constexpr internal::unit<time_tag, 1.0F, "s"> seconds;
	export inline constexpr internal::unit<time_tag, 1e-3F, "ms"> milliseconds;
	export inline constexpr internal::unit<time_tag, 1e-6F, "us"> microseconds;
	export inline constexpr internal::unit<time_tag, 1e-9F, "ns"> nanoseconds;
}

export template <>
struct gse::internal::dimension_traits<gse::internal::dim<0, 1, 0>> {
	using tag = time_tag;
	using default_unit = decltype(seconds);
};

export namespace gse {
	template <typename T = float>
	using time_t = internal::quantity<T, internal::dim<0, 1, 0>>;
	using time = time_t<>;
}

export template <>
struct gse::internal::quantity_traits<gse::time_tag> {
	template <typename T>
	using type = time_t<T>;
};
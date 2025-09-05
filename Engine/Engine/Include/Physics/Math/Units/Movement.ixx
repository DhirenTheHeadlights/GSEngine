export module gse.physics.math:movement;

import std;
import :dimension;
import :quant;

namespace gse {
	struct velocity_tag {};

	export inline constexpr internal::unit<velocity_tag, 1.0F, "m/s"> meters_per_second;
	export inline constexpr internal::unit<velocity_tag, 0.277777778F, "km/h"> kilometers_per_hour;
	export inline constexpr internal::unit<velocity_tag, 0.44704F, "mph"> miles_per_hour;

	struct acceleration_tag {};

	export inline constexpr internal::unit<acceleration_tag, 1.0F, "m/s^2"> meters_per_second_squared;

	struct angular_velocity_tag {};

	export inline constexpr internal::unit<angular_velocity_tag, 180.0F / std::numbers::pi, "deg/s"> degrees_per_second;
	export inline constexpr internal::unit<angular_velocity_tag, 1.0F, "rad/s"> radians_per_second;

	struct angular_acceleration_tag {};

	export inline constexpr internal::unit<angular_acceleration_tag, 180.0F / std::numbers::pi, "deg/s^2"> degrees_per_second_squared;
	export inline constexpr internal::unit<angular_acceleration_tag, 1.0F, "rad/s^2"> radians_per_second_squared;
}
export template <>
struct gse::internal::dimension_traits<gse::internal::dim<1, -1, 0>> {
	using tag = velocity_tag;
	using default_unit = decltype(meters_per_second);
};

export template <>
struct gse::internal::dimension_traits<gse::internal::dim<1, -2, 0>> {
	using tag = acceleration_tag;
	using default_unit = decltype(meters_per_second_squared);
};

export template <>
struct gse::internal::dimension_traits<gse::internal::dim<0, -1, 0>> {
	using tag = angular_velocity_tag;
	using default_unit = decltype(radians_per_second);
};

export template <>
struct gse::internal::dimension_traits<gse::internal::dim<0, -2, 0>> {
	using tag = angular_acceleration_tag;
	using default_unit = decltype(radians_per_second_squared);
};

export namespace gse {
	template <typename T = float>
	using velocity_t = internal::quantity<T, internal::dim<1, -1, 0>>;
	using velocity = velocity_t<>;

	template <typename T = float>
	using acceleration_t = internal::quantity<T, internal::dim<1, -2, 0>>;
	using acceleration = acceleration_t<>;

	template <typename T = float>
	using angular_velocity_t = internal::quantity<T, internal::dim<0, -1, 0>>;
	using angular_velocity = angular_velocity_t<>;

	template <typename T = float>
	using angular_acceleration_t = internal::quantity<T, internal::dim<0, -2, 0>>;
	using angular_acceleration = angular_acceleration_t<>;
}

export template <>
struct gse::internal::quantity_traits<gse::velocity_tag> {
	template <typename T>
	using type = velocity_t<T>;
};

export template <>
struct gse::internal::quantity_traits<gse::acceleration_tag> {
	template <typename T>
	using type = acceleration_t<T>;
};

export template <>
struct gse::internal::quantity_traits<gse::angular_velocity_tag> {
	template <typename T>
	using type = angular_velocity_t<T>;
};

export template <>
struct gse::internal::quantity_traits<gse::angular_acceleration_tag> {
	template <typename T>
	using type = angular_acceleration_t<T>;
};
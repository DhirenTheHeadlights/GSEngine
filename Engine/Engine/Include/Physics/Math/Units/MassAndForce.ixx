export module gse.physics.math:mass_and_force;

import std;
import :dimension;
import :quant;

namespace gse {
	struct mass_tag {};

	export inline constexpr internal::unit<mass_tag, 1.0F, "kg"> kilograms;
	export inline constexpr internal::unit<mass_tag, 1e-3F, "g"> grams;
	export inline constexpr internal::unit<mass_tag, 1e-6F, "mg"> milligrams;
	export inline constexpr internal::unit<mass_tag, 0.45359237F, "lb"> pounds;
	export inline constexpr internal::unit<mass_tag, 0.0283495231F, "oz"> ounces;

	struct force_tag {};

	export inline constexpr internal::unit<force_tag, 1.0F, "N"> newtons;
	export inline constexpr internal::unit<force_tag, 4.44822F, "lbf"> pounds_force;

	struct torque_tag {};

	export inline constexpr internal::unit<torque_tag, 1.0F, "N-m"> newton_meters;
	export inline constexpr internal::unit<torque_tag, 1.35582F, "lbf-ft"> pound_feet;

	struct inertia_tag {};

	export inline constexpr internal::unit<inertia_tag, 1.0F, "kg-m^2"> kilograms_meters_squared;
	export inline constexpr internal::unit<inertia_tag, 0.04214011F, "lb-ft^2"> pounds_feet_squared;
}
export template <>
struct gse::internal::dimension_traits<gse::internal::dim<0, 0, 1>> {
	using tag = mass_tag;
	using default_unit = decltype(kilograms);
};

export template <>
struct gse::internal::dimension_traits<gse::internal::dim<1, -2, 1>> {
	using tag = force_tag;
	using default_unit = decltype(newtons);
};

export template <>
struct gse::internal::dimension_traits<gse::internal::dim<2, -2, 1>> {
	using tag = torque_tag; 
	using default_unit = decltype(newton_meters);
};

export template <>
struct gse::internal::dimension_traits<gse::internal::dim<2, 0, 1>> {
	using tag = inertia_tag;
	using default_unit = decltype(kilograms_meters_squared);
};

export namespace gse {
	template <typename T = float>
	using mass_t = internal::quantity<T, internal::dim<0, 0, 1>>;
	using mass = mass_t<>;

	using inverse_mass = internal::quantity<float, internal::dim<0, 0, -1>>;

	template <typename T = float>
	using force_t = internal::quantity<T, internal::dim<1, -2, 1>>;
	using force = force_t<>;

	template <typename T = float>
	using torque_t = internal::quantity<T, internal::dim<2, -2, 1>>;
	using torque = torque_t<>;

	template <typename T = float>
	using inertia_t = internal::quantity<T, internal::dim<2, 0, 1>>;
	using inertia = inertia_t<>;
}

export template <>
struct gse::internal::quantity_traits<gse::mass_tag> {
	template <typename T>
	using type = mass_t<T>;
};

export template <>
struct gse::internal::quantity_traits<gse::torque_tag> {
	template <typename T>
	using type = torque_t<T>;
};

export template <>
struct gse::internal::quantity_traits<gse::force_tag> {
	template <typename T>
	using type = force_t<T>;
};

export template <>
struct gse::internal::quantity_traits<gse::inertia_tag> {
	template <typename T>
	using type = inertia_t<T>;
};
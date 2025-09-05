export module gse.physics.math:energy_and_power;

import std;
import :dimension;
import :quant;

namespace gse {
	struct energy_tag {};

	export inline constexpr internal::unit<energy_tag, 1.0F, "J"> joules;
	export inline constexpr internal::unit<energy_tag, 1e3F, "kJ"> kilojoules;
	export inline constexpr internal::unit<energy_tag, 1e6F, "MJ"> megajoules;
	export inline constexpr internal::unit<energy_tag, 1e9F, "GJ"> gigajoules;
	export inline constexpr internal::unit<energy_tag, 4.184F, "cal"> calories;
	export inline constexpr internal::unit<energy_tag, 4184.0F, "kcal"> kilocalories;

	struct power_tag {};

	export inline constexpr internal::unit<power_tag, 1.0F, "W"> watts;
	export inline constexpr internal::unit<power_tag, 1e3F, "kW"> kilowatts;
	export inline constexpr internal::unit<power_tag, 1e6F, "MW"> megawatts;
	export inline constexpr internal::unit<power_tag, 1e9F, "GW"> gigawatts;
	export inline constexpr internal::unit<power_tag, 745.7F, "hp"> horsepower;
}
export template <>
struct gse::internal::dimension_traits<gse::internal::dim<2, -2, 1>> {
	using tag = energy_tag;
	using default_unit = decltype(joules);
};

export template <>
struct gse::internal::dimension_traits<gse::internal::dim<2, -3, 1>> {
	using tag = power_tag;
	using default_unit = decltype(watts);
};

export namespace gse {
	template <typename T = float>
	using energy_t = internal::quantity<T, internal::dim<2, -2, 1>>;
	using energy = energy_t<>;

	template <typename T = float>
	using power_t = internal::quantity<T, internal::dim<2, -3, 1>>;
	using power = power_t<>;
}

export template <>
struct gse::internal::quantity_traits<gse::energy_tag> {
	template <typename T>
	using type = energy_t<T>;
};

export template <>
struct gse::internal::quantity_traits<gse::power_tag> {
	template <typename T>
	using type = power_t<T>;
};
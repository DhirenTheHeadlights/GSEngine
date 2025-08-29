export module gse.physics.math:energy_and_power;

import std;

import :dimension;
import :quant;
import :vector;

namespace gse::units {
	struct energy_tag {};
	
	constexpr char joules_units[] = "J";
	constexpr char kilojoules_units[] = "kJ";
	constexpr char megajoules_units[] = "MJ";
	constexpr char gigajoules_units[] = "GJ";
	constexpr char calories_units[] = "cal";
	constexpr char kilocalories_units[] = "kcal";
	
	export using joules = internal::unit<energy_tag, 1.0f, joules_units>;
	export using kilojoules = internal::unit<energy_tag, 1000.0f, kilojoules_units>;
	export using megajoules = internal::unit<energy_tag, 1000000.0f, megajoules_units>;
	export using gigajoules = internal::unit<energy_tag, 1000000000.0f, gigajoules_units>;
	export using calories = internal::unit<energy_tag, 4.184f, calories_units>;
	export using kilocalories = internal::unit<energy_tag, 4184.0f, kilocalories_units>;
	
	using energy_units = internal::unit_list<
		joules,
		kilojoules,
		megajoules,
		gigajoules,
		calories,
		kilocalories
	>;
}

export template <>
struct gse::internal::dimension_traits<gse::internal::dim<2, -2, 1>> {
	using tag = units::energy_tag;
	using default_unit = units::joules;
	using valid_units = units::energy_units;
};

export namespace gse {
	template <typename T = float>
	using energy_t = internal::quantity<T, internal::dim<2, -2, 1>>;

	using energy = energy_t<>;

	template <typename T> constexpr auto joules_t(T value) -> energy_t<T>;
	template <typename T> constexpr auto kilojoules_t(T value) -> energy_t<T>;
	template <typename T> constexpr auto megajoules_t(T value) -> energy_t<T>;
	template <typename T> constexpr auto gigajoules_t(T value) -> energy_t<T>;
	template <typename T> constexpr auto calories_t(T value) -> energy_t<T>;
	template <typename T> constexpr auto kilocalories_t(T value) -> energy_t<T>;

	constexpr auto joules(float value) -> energy;
	constexpr auto kilojoules(float value) -> energy;
	constexpr auto megajoules(float value) -> energy;
	constexpr auto gigajoules(float value) -> energy;
	constexpr auto calories(float value) -> energy;
	constexpr auto kilocalories(float value) -> energy;
}

export namespace gse::vec {
	template <internal::is_unit U, typename... Args>
	constexpr auto energy(Args&&... args) -> vec_t<energy_t<std::common_type_t<Args...>>, sizeof...(Args)>;

	template <typename... Args> constexpr auto joules(Args&&... args) -> vec_t<energy_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto kilojoules(Args&&... args) -> vec_t<energy_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto megajoules(Args&&... args) -> vec_t<energy_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto gigajoules(Args&&... args) -> vec_t<energy_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto calories(Args&&... args) -> vec_t<energy_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto kilocalories(Args&&... args) -> vec_t<energy_t<std::common_type_t<Args...>>, sizeof...(Args)>;
}

template <typename T>
constexpr auto gse::joules_t(const T value) -> energy_t<T> {
	return energy_t<T>::template from<units::joules>(value);
}

template <typename T>
constexpr auto gse::kilojoules_t(const T value) -> energy_t<T> {
	return energy_t<T>::template from<units::kilojoules>(value);
}

template <typename T>
constexpr auto gse::megajoules_t(const T value) -> energy_t<T> {
	return energy_t<T>::template from<units::megajoules>(value);
}

template <typename T>
constexpr auto gse::gigajoules_t(const T value) -> energy_t<T> {
	return energy_t<T>::template from<units::gigajoules>(value);
}

template <typename T>
constexpr auto gse::calories_t(const T value) -> energy_t<T> {
	return energy_t<T>::template from<units::calories>(value);
}

template <typename T>
constexpr auto gse::kilocalories_t(const T value) -> energy_t<T> {
	return energy_t<T>::template from<units::kilocalories>(value);
}

constexpr auto gse::joules(const float value) -> energy {
	return joules_t<float>(value);
}

constexpr auto gse::kilojoules(const float value) -> energy {
	return kilojoules_t<float>(value);
}

constexpr auto gse::megajoules(const float value) -> energy {
	return megajoules_t<float>(value);
}

constexpr auto gse::gigajoules(const float value) -> energy {
	return gigajoules_t<float>(value);
}

constexpr auto gse::calories(const float value) -> energy {
	return calories_t<float>(value);
}

constexpr auto gse::kilocalories(const float value) -> energy {
	return kilocalories_t<float>(value);
}

template <gse::internal::is_unit U, typename... Args>
constexpr auto gse::vec::energy(Args&&... args) -> vec_t<energy_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { energy_t<std::common_type_t<Args...>>::template from<U>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::joules(Args&&... args) -> vec_t<energy_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { energy_t<std::common_type_t<Args...>>::template from<units::joules>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::kilojoules(Args&&... args) -> vec_t<energy_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { energy_t<std::common_type_t<Args...>>::template from<units::kilojoules>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::megajoules(Args&&... args) -> vec_t<energy_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { energy_t<std::common_type_t<Args...>>::template from<units::megajoules>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::gigajoules(Args&&... args) -> vec_t<energy_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { energy_t<std::common_type_t<Args...>>::template from<units::gigajoules>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::calories(Args&&... args) -> vec_t<energy_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { energy_t<std::common_type_t<Args...>>::template from<units::calories>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::kilocalories(Args&&... args) -> vec_t<energy_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { energy_t<std::common_type_t<Args...>>::template from<units::kilocalories>(args)... };
}

namespace gse::units {
	struct power_tag {};

	constexpr char watts_units[] = "W";
	constexpr char kilowatts_units[] = "kW";
	constexpr char megawatts_units[] = "MW";
	constexpr char gigawatts_units[] = "GW";
	constexpr char horsepower_units[] = "hp";

	export using watts = internal::unit<power_tag, 1.0f, watts_units>;
	export using kilowatts = internal::unit<power_tag, 1000.0f, kilowatts_units>;
	export using megawatts = internal::unit<power_tag, 1000000.0f, megawatts_units>;
	export using gigawatts = internal::unit<power_tag, 1000000000.0f, gigawatts_units>;
	export using horsepower = internal::unit<power_tag, 745.7f, horsepower_units>;

	using power_units = internal::unit_list<
		watts,
		kilowatts,
		megawatts,
		gigawatts,
		horsepower
	>;
}

export template <>
struct gse::internal::dimension_traits<gse::internal::dim<2, -3, 1>> {
	using tag = units::power_tag;
	using default_unit = units::watts;
	using valid_units = units::power_units;
};

export namespace gse {
	template <typename T = float>
	using power_t = internal::quantity<T, internal::dim<2, -3, 1>>;
	
	using power = power_t<>;
	
	template <typename T> constexpr auto watts_t(T value) -> power_t<T>;
	template <typename T> constexpr auto kilowatts_t(T value) -> power_t<T>;
	template <typename T> constexpr auto megawatts_t(T value) -> power_t<T>;
	template <typename T> constexpr auto gigawatts_t(T value) -> power_t<T>;
	template <typename T> constexpr auto horsepower_t(T value) -> power_t<T>;

	constexpr auto watts(float value) -> power;
	constexpr auto kilowatts(float value) -> power;
	constexpr auto megawatts(float value) -> power;
	constexpr auto gigawatts(float value) -> power;
	constexpr auto horsepower(float value) -> power;
}

export namespace gse::vec {
	template <internal::is_unit U, typename... Args>
	constexpr auto power(Args&&... args) -> vec_t<power_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	
	template <typename... Args> constexpr auto watts(Args&&... args) -> vec_t<power_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto kilowatts(Args&&... args) -> vec_t<power_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto megawatts(Args&&... args) -> vec_t<power_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto gigawatts(Args&&... args) -> vec_t<power_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto horsepower(Args&&... args) -> vec_t<power_t<std::common_type_t<Args...>>, sizeof...(Args)>;
}

template <typename T>
constexpr auto gse::watts_t(const T value) -> power_t<T> {
	return power_t<T>::template from<units::watts>(value);
}

template <typename T>
constexpr auto gse::kilowatts_t(const T value) -> power_t<T> {
	return power_t<T>::template from<units::kilowatts>(value);
}

template <typename T>
constexpr auto gse::megawatts_t(const T value) -> power_t<T> {
	return power_t<T>::template from<units::megawatts>(value);
}

template <typename T>
constexpr auto gse::gigawatts_t(const T value) -> power_t<T> {
	return power_t<T>::template from<units::gigawatts>(value);
}

template <typename T>
constexpr auto gse::horsepower_t(const T value) -> power_t<T> {
	return power_t<T>::template from<units::horsepower>(value);
}

constexpr auto gse::watts(const float value) -> power {
	return watts_t<float>(value);
}

constexpr auto gse::kilowatts(const float value) -> power {
	return kilowatts_t<float>(value);
}

constexpr auto gse::megawatts(const float value) -> power {
	return megawatts_t<float>(value);
}

constexpr auto gse::gigawatts(const float value) -> power {
	return gigawatts_t<float>(value);
}

constexpr auto gse::horsepower(const float value) -> power {
	return horsepower_t<float>(value);
}

template <gse::internal::is_unit U, typename... Args>
constexpr auto gse::vec::power(Args&&... args) -> vec_t<power_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { power_t<std::common_type_t<Args...>>::template from<U>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::watts(Args&&... args) -> vec_t<power_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { power_t<std::common_type_t<Args...>>::template from<units::watts>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::kilowatts(Args&&... args) -> vec_t<power_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { power_t<std::common_type_t<Args...>>::template from<units::kilowatts>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::megawatts(Args&&... args) -> vec_t<power_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { power_t<std::common_type_t<Args...>>::template from<units::megawatts>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::gigawatts(Args&&... args) -> vec_t<power_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { power_t<std::common_type_t<Args...>>::template from<units::gigawatts>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::horsepower(Args&&... args) -> vec_t<power_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { power_t<std::common_type_t<Args...>>::template from<units::horsepower>(args)... };
}
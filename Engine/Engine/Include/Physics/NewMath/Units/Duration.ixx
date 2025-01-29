export module gse.physics.math.units.dur;

import std;

import gse.physics.math.units.quant;
import gse.physics.math.unit_vec;

namespace gse::units {
	struct duration_tag {};

	constexpr char hours_units[] = "hrs";
	constexpr char minutes_units[] = "min";
	constexpr char seconds_units[] = "s";
	constexpr char milliseconds_units[] = "ms";
	constexpr char microseconds_units[] = "us";
	constexpr char nanoseconds_units[] = "ns";

	export using hours = internal::unit<duration_tag, 3600.0f, hours_units>;
	export using minutes = internal::unit<duration_tag, 60.0f, minutes_units>;
	export using seconds = internal::unit<duration_tag, 1.0f, seconds_units>;
	export using milliseconds = internal::unit<duration_tag, 0.001f, milliseconds_units>;
	export using microseconds = internal::unit<duration_tag, 0.000001f, microseconds_units>;
	export using nanoseconds = internal::unit<duration_tag, 0.000000001f, nanoseconds_units>;

	using duration_units = internal::unit_list <
		hours,
		minutes,
		seconds,
		milliseconds,
		microseconds,
		nanoseconds
	>;
}

export namespace gse {
	template <typename T = float> using duration_t = internal::quantity<T, units::duration_tag, units::seconds, units::duration_units>;
	
	using duration = duration_t<>;
	
	template <typename T> constexpr auto hours_t(T value) -> duration_t<T>;
	template <typename T> constexpr auto minutes_t(T value) -> duration_t<T>;
	template <typename T> constexpr auto seconds_t(T value) -> duration_t<T>;
	template <typename T> constexpr auto milliseconds_t(T value) -> duration_t<T>;
	template <typename T> constexpr auto microseconds_t(T value) -> duration_t<T>;
	template <typename T> constexpr auto nanoseconds_t(T value) -> duration_t<T>;

	constexpr auto hours(float value) -> duration;
	constexpr auto minutes(float value) -> duration;
	constexpr auto seconds(float value) -> duration;
	constexpr auto milliseconds(float value) -> duration;
	constexpr auto microseconds(float value) -> duration;
	constexpr auto nanoseconds(float value) -> duration;
}

export namespace gse::vec {
	template <internal::is_unit U, typename... Args>
	constexpr auto duration(Args&&... args) -> vec_t<duration_t<std::common_type_t<Args...>>, sizeof...(Args)>;

	template <typename... Args> constexpr auto hours(Args&&... args) -> vec_t<duration_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto minutes(Args&&... args) -> vec_t<duration_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto seconds(Args&&... args) -> vec_t<duration_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto milliseconds(Args&&... args) -> vec_t<duration_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto microseconds(Args&&... args) -> vec_t<duration_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto nanoseconds(Args&&... args) -> vec_t<duration_t<std::common_type_t<Args...>>, sizeof...(Args)>;
}

template <typename T>
constexpr auto gse::hours_t(const T value) -> duration_t<T> {
	return duration_t<T>::template from<units::hours>(value);
}

template <typename T>
constexpr auto gse::minutes_t(const T value) -> duration_t<T> {
	return duration_t<T>::template from<units::minutes>(value);
}

template <typename T>
constexpr auto gse::seconds_t(const T value) -> duration_t<T> {
	return duration_t<T>::template from<units::seconds>(value);
}

template <typename T>
constexpr auto gse::milliseconds_t(const T value) -> duration_t<T> {
	return duration_t<T>::template from<units::milliseconds>(value);
}

template <typename T>
constexpr auto gse::microseconds_t(const T value) -> duration_t<T> {
	return duration_t<T>::template from<units::microseconds>(value);
}

template <typename T>
constexpr auto gse::nanoseconds_t(const T value) -> duration_t<T> {
	return duration_t<T>::template from<units::nanoseconds>(value);
}

constexpr auto gse::hours(const float value) -> duration {
	return hours_t<float>(value);
}

constexpr auto gse::minutes(const float value) -> duration {
	return minutes_t<float>(value);
}

constexpr auto gse::seconds(const float value) -> duration {
	return seconds_t<float>(value);
}

constexpr auto gse::milliseconds(const float value) -> duration {
	return milliseconds_t<float>(value);
}

constexpr auto gse::microseconds(const float value) -> duration {
	return microseconds_t<float>(value);
}

constexpr auto gse::nanoseconds(const float value) -> duration {
	return nanoseconds_t<float>(value);
}

template <gse::internal::is_unit U, typename... Args>
constexpr auto gse::vec::duration(Args&&... args) -> vec_t<duration_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { duration_t<std::common_type_t<Args...>>::template from<U>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::hours(Args&&... args) -> vec_t<duration_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { duration_t<std::common_type_t<Args...>>::template from<units::hours>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::minutes(Args&&... args) -> vec_t<duration_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { duration_t<std::common_type_t<Args...>>::template from<units::minutes>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::seconds(Args&&... args) -> vec_t<duration_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { duration_t<std::common_type_t<Args...>>::template from<units::seconds>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::milliseconds(Args&&... args) -> vec_t<duration_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { duration_t<std::common_type_t<Args...>>::template from<units::milliseconds>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::microseconds(Args&&... args) -> vec_t<duration_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { duration_t<std::common_type_t<Args...>>::template from<units::microseconds>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::nanoseconds(Args&&... args) -> vec_t<duration_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { duration_t<std::common_type_t<Args...>>::template from<units::nanoseconds>(args)... };
}
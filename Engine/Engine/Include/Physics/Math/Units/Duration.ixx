export module gse.physics.math:duration;

import std;

import :dimension;
import :quant;
import :vector;

namespace gse::units {
	struct time_tag {};

	constexpr char hours_units[] = "hrs";
	constexpr char minutes_units[] = "min";
	constexpr char seconds_units[] = "s";
	constexpr char milliseconds_units[] = "ms";
	constexpr char microseconds_units[] = "us";
	constexpr char nanoseconds_units[] = "ns";

	export using hours = internal::unit<time_tag, 3600.0f, hours_units>;
	export using minutes = internal::unit<time_tag, 60.0f, minutes_units>;
	export using seconds = internal::unit<time_tag, 1.0f, seconds_units>;
	export using milliseconds = internal::unit<time_tag, 0.001f, milliseconds_units>;
	export using microseconds = internal::unit<time_tag, 0.000001f, microseconds_units>;
	export using nanoseconds = internal::unit<time_tag, 0.000000001f, nanoseconds_units>;

	using time_units = internal::unit_list <
		hours,
		minutes,
		seconds,
		milliseconds,
		microseconds,
		nanoseconds
	>;
}

export template <>
struct gse::internal::dimension_traits<gse::internal::dim<0, 1, 0>> {
	using tag = units::time_tag;
	using default_unit = units::seconds;
	using valid_units = units::time_units;
};

export namespace gse {
	template <typename T = float>
	using time_t = internal::quantity<T, internal::dim<0, 1, 0>>;
	
	using time = time_t<>;
	
	template <typename T> constexpr auto hours_t(T value) -> time_t<T>;
	template <typename T> constexpr auto minutes_t(T value) -> time_t<T>;
	template <typename T> constexpr auto seconds_t(T value) -> time_t<T>;
	template <typename T> constexpr auto milliseconds_t(T value) -> time_t<T>;
	template <typename T> constexpr auto microseconds_t(T value) -> time_t<T>;
	template <typename T> constexpr auto nanoseconds_t(T value) -> time_t<T>;

	constexpr auto hours(float value) -> time;
	constexpr auto minutes(float value) -> time;
	constexpr auto seconds(float value) -> time;
	constexpr auto milliseconds(float value) -> time;
	constexpr auto microseconds(float value) -> time;
	constexpr auto nanoseconds(float value) -> time;
}

export namespace gse::vec {
	template <internal::is_unit U, typename... Args>
	constexpr auto time(Args&&... args) -> vec_t<time_t<std::common_type_t<Args...>>, sizeof...(Args)>;

	template <typename... Args> constexpr auto hours(Args&&... args) -> vec_t<time_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto minutes(Args&&... args) -> vec_t<time_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto seconds(Args&&... args) -> vec_t<time_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto milliseconds(Args&&... args) -> vec_t<time_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto microseconds(Args&&... args) -> vec_t<time_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto nanoseconds(Args&&... args) -> vec_t<time_t<std::common_type_t<Args...>>, sizeof...(Args)>;
}

template <typename T>
constexpr auto gse::hours_t(const T value) -> time_t<T> {
	return time_t<T>::template from<units::hours>(value);
}

template <typename T>
constexpr auto gse::minutes_t(const T value) -> time_t<T> {
	return time_t<T>::template from<units::minutes>(value);
}

template <typename T>
constexpr auto gse::seconds_t(const T value) -> time_t<T> {
	return time_t<T>::template from<units::seconds>(value);
}

template <typename T>
constexpr auto gse::milliseconds_t(const T value) -> time_t<T> {
	return time_t<T>::template from<units::milliseconds>(value);
}

template <typename T>
constexpr auto gse::microseconds_t(const T value) -> time_t<T> {
	return time_t<T>::template from<units::microseconds>(value);
}

template <typename T>
constexpr auto gse::nanoseconds_t(const T value) -> time_t<T> {
	return time_t<T>::template from<units::nanoseconds>(value);
}

constexpr auto gse::hours(const float value) -> time {
	return hours_t<float>(value);
}

constexpr auto gse::minutes(const float value) -> time {
	return minutes_t<float>(value);
}

constexpr auto gse::seconds(const float value) -> time {
	return seconds_t<float>(value);
}

constexpr auto gse::milliseconds(const float value) -> time {
	return milliseconds_t<float>(value);
}

constexpr auto gse::microseconds(const float value) -> time {
	return microseconds_t<float>(value);
}

constexpr auto gse::nanoseconds(const float value) -> time {
	return nanoseconds_t<float>(value);
}

template <gse::internal::is_unit U, typename... Args>
constexpr auto gse::vec::time(Args&&... args) -> vec_t<time_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { time_t<std::common_type_t<Args...>>::template from<U>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::hours(Args&&... args) -> vec_t<time_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { time_t<std::common_type_t<Args...>>::template from<units::hours>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::minutes(Args&&... args) -> vec_t<time_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { time_t<std::common_type_t<Args...>>::template from<units::minutes>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::seconds(Args&&... args) -> vec_t<time_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { time_t<std::common_type_t<Args...>>::template from<units::seconds>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::milliseconds(Args&&... args) -> vec_t<time_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { time_t<std::common_type_t<Args...>>::template from<units::milliseconds>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::microseconds(Args&&... args) -> vec_t<time_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { time_t<std::common_type_t<Args...>>::template from<units::microseconds>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::nanoseconds(Args&&... args) -> vec_t<time_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { time_t<std::common_type_t<Args...>>::template from<units::nanoseconds>(args)... };
}
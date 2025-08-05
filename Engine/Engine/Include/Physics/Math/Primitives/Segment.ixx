export module gse.physics.math:segment;

import std;

import :primitive_math_shared;
import :angle;
import :unit_vec_math;
import :vec_math;

export namespace gse {
	template <is_vec2 T>
	struct segment_t {
		T start;
		T end;

		constexpr auto length() const -> typename T::value_type;
		constexpr auto midpoint() const -> T;
		constexpr auto angle() const -> angle_t<typename T::value_type>;
	};
}

template <gse::is_vec2 T>
constexpr auto gse::segment_t<T>::length() const -> typename T::value_type {
	return magnitude(end - start);
}

template <gse::is_vec2 T>
constexpr auto gse::segment_t<T>::midpoint() const -> T {
	return (start + end) / static_cast<typename T::value_type>(2);
}

template <gse::is_vec2 T>
constexpr auto gse::segment_t<T>::angle() const -> angle_t<typename T::value_type> {
	const T diff = end - start;
	return radians_t<typename T::value_type>(std::atan2(diff.y, diff.x));
}
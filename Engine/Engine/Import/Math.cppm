export module gse.physics.math;

import std;

export import :dimension;
export import :matrix;
export import :matrix_math;
export import :quat;
export import :quat_math;
export import :quant;
export import :rectangle;
export import :circle;
export import :segment;
export import :units;
export import :vector;
export import :vector_math;

export namespace gse {
	template <typename T>
	concept is_arithmetic = internal::is_arithmetic<T>;

	template <typename T>
	concept is_unit = internal::is_unit<T>;

	template <typename T>
	concept is_quantity = internal::is_quantity<T>;

	template <is_arithmetic NumberType>
	auto random_value(
		const NumberType& min,
		const NumberType& max
	) -> NumberType;

	template <is_arithmetic NumberType>
	auto random_value(
		const NumberType& max
	) -> NumberType;
}

namespace gse {
	std::random_device random_device;
	std::mt19937 generator(random_device());
}

template <gse::is_arithmetic NumberType>
auto gse::random_value(const NumberType& min, const NumberType& max) -> NumberType {
	if constexpr (std::is_floating_point_v<NumberType>) {
		std::uniform_real_distribution<NumberType> dis(min, max);
		return dis(generator);
	}
	else {
		std::uniform_int_distribution<NumberType> dis(min, max);
		return dis(generator);
	}
}

template <gse::is_arithmetic NumberType>
auto gse::random_value(const NumberType& max) -> NumberType {
	if constexpr (std::is_floating_point_v<NumberType>) {
		std::uniform_real_distribution<NumberType> dis(0, max);
		return dis(generator);
	}
	else {
		std::uniform_int_distribution<NumberType> dis(0, max);
		return dis(generator);
	}
}

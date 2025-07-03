export module gse.physics.math;

import std;

export import :angle;
export import :duration;
export import :energy_and_power;
export import :length;
export import :mass_and_force;
export import :movement;
export import :matrix;
export import :matrix_math;
export import :quat;
export import :quat_math;
export import :unitless_vec;
export import :unit_vec;
export import :vec_math;
export import :unit_vec_math;

import :base_vec;

std::random_device g_rd;
std::mt19937 g_gen(g_rd());

export namespace gse {
	using raw2i = vec::storage<int, 2>;
	using raw3i = vec::storage<int, 3>;
	using raw4i = vec::storage<int, 4>;

	using raw2f = vec::storage<float, 2>;
	using raw3f = vec::storage<float, 3>;
	using raw4f = vec::storage<float, 4>;

	using raw2d = vec::storage<double, 2>;
	using raw3d = vec::storage<double, 3>;
	using raw4d = vec::storage<double, 4>;

	template <typename T>
	concept is_arithmetic = internal::is_arithmetic<T>;

	template <typename T>
	concept is_unit = internal::is_unit<T>;

	template <typename T>
	concept is_quantity = internal::is_quantity<T>;

	template <typename NumberType>
		requires std::is_arithmetic_v<NumberType>
	auto random_value(const NumberType& min, const NumberType& max) -> NumberType {
		if constexpr (std::is_floating_point_v<NumberType>) {
			std::uniform_real_distribution<NumberType> dis(min, max);
			return dis(g_gen);
		}
		else {
			std::uniform_int_distribution<NumberType> dis(min, max);
			return dis(g_gen);
		}
	}

	template <typename NumberType>
		requires std::is_arithmetic_v<NumberType>
	auto random_value(const NumberType& max) -> NumberType {
		if constexpr (std::is_floating_point_v<NumberType>) {
			std::uniform_real_distribution<NumberType> dis(0, max);
			return dis(g_gen);
		}
		else {
			std::uniform_int_distribution<NumberType> dis(0, max);
			return dis(g_gen);
		}
	}

	template <typename T>
	auto get_pointers(const std::vector<std::unique_ptr<T>>& unique_ptrs) -> std::vector<T*> {
		std::vector<T*> pointers;
		pointers.reserve(unique_ptrs.size());
		for (const auto& unique_ptr : unique_ptrs) {
			pointers.push_back(unique_ptr.get());
		}
		return pointers;
	}


	template <typename T> requires internal::is_quantity<T> auto abs(const T& value) -> T;
	template <typename T> requires internal::is_quantity<T> auto min(const T& a, const T& b) -> T;
	template <typename T> requires internal::is_quantity<T> auto max(const T& a, const T& b) -> T;
}

template <typename T> requires gse::internal::is_quantity<T>
auto gse::abs(const T& value) -> T {
	return value >= T() ? value : -value;
}

template <typename T> requires gse::internal::is_quantity<T>
auto gse::min(const T& a, const T& b) -> T {
	return a < b ? a : b;
}

template <typename T> requires gse::internal::is_quantity<T>
auto gse::max(const T& a, const T& b) -> T {
	return a > b ? a : b;
}
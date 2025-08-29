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
export import :rectangle;
export import :circle;
export import :segment;
export import :vector;
export import :vector_math;

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

	template <typename T1, typename T2>
	constexpr auto max(const T1& a, const T2& b) -> std::common_type_t<T1, T2> {
		using common_type = std::common_type_t<T1, T2>;

		if (static_cast<common_type>(a) < static_cast<common_type>(b)) {
			return static_cast<common_type>(b);
		}
		return static_cast<common_type>(a);
	}

	template <is_quantity Q>
	constexpr auto abs(const Q& value) -> Q {
		return std::abs(value.as_default_unit());
	}
}
export module gse.physics.math;

import std;

export import gse.physics.math.units;
export import gse.physics.math.matrix;
export import gse.physics.math.matrix_math;
export import gse.physics.math.quat;
export import gse.physics.math.quat_math;
export import gse.physics.math.base_vec;
export import gse.physics.math.unitless_vec;
export import gse.physics.math.unit_vec;
export import gse.physics.math.vec_math;
export import gse.physics.math.unit_vec_math;

std::random_device g_rd;
std::mt19937 g_gen(g_rd());

export namespace gse {
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
}
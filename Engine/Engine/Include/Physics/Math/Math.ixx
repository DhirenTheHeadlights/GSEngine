export module gse.physics.math;

import std;
import glm;

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

	template <internal::is_quantity T, int N>
	constexpr auto to_glm_vec(const vec_t<T, N>& v) -> glm::vec<N, typename vec_t<T, N>::value_type> {
		using value_type = typename vec_t<T, N>::value_type;
		if constexpr (N == 2) {
			return glm::vec<2, value_type>(v.x.as_default_unit(), v.y.as_default_unit());
		}
		else if constexpr (N == 3) {
			return glm::vec<3, value_type>(v.x.as_default_unit(), v.y.as_default_unit(), v.z.as_default_unit());
		}
		else if constexpr (N == 4) {
			return glm::vec<4, value_type>(v.x.as_default_unit(), v.y.as_default_unit(), v.z.as_default_unit(), v.w.as_default_unit());
		}
	}

	template <typename T, int N>
	constexpr auto to_glm_vec(const unitless::vec_t<T, N> v) -> glm::vec<N, T> {
		if constexpr (N == 2) {
			return glm::vec<2, T>(v.x, v.y);
		}
		else if constexpr (N == 3) {
			return glm::vec<3, T>(v.x, v.y, v.z);
		}
		else if constexpr (N == 4) {
			return glm::vec<4, T>(v.x, v.y, v.z, v.w);
		}
	}

	template <typename T, int N>
	constexpr auto to_unitless_vec(const glm::vec<N, T>& v) -> unitless::vec_t<T, N> {
		if constexpr (N == 2) {
			return { v.x, v.y };
		}
		else if constexpr (N == 3) {
			return { v.x, v.y, v.z };
		}
		else if constexpr (N == 4) {
			return { v.x, v.y, v.z, v.w };
		}
	}

	template <typename T, int N>
	constexpr auto to_glm_vec(const vec::storage<T, N>& storage) -> glm::vec<N, T> {
		if constexpr (N == 2) {
			return glm::vec<2, T>(storage.data[0], storage.data[1]);
		}
		else if constexpr (N == 3) {
			return glm::vec<3, T>(storage.data[0], storage.data[1], storage.data[2]);
		}
		else if constexpr (N == 4) {
			return glm::vec<4, T>(storage.data[0], storage.data[1], storage.data[2], storage.data[3]);
		}
	}

	template <typename T, int Cols, int Rows>
	constexpr auto to_glm_mat(const mat_t<T, Cols, Rows>& m) -> glm::mat<Rows, Cols, T> {
		glm::mat<Rows, Cols, T> result;
		for (int i = 0; i < Rows; ++i) {
			for (int j = 0; j < Cols; ++j) {
				result[i][j] = m[i][j];
			}
		}
		return result;
	}

	template <typename T>
	constexpr auto to_glm_quat(const quat_t<T>& q) -> glm::quat {
		return glm::quat(q.s, q.x, q.y, q.z);
	}
}
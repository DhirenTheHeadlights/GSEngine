export module gse.physics.math.vector_math;

import std;
import glm;

import gse.physics.math.vector;

export namespace gse {
	std::random_device g_rd;
	std::mt19937 g_gen(g_rd());

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

export namespace gse {
	enum : std::uint8_t {
		x, y, z 
	};

	template <is_unit_or_quantity quantity_or_unit_type>
	auto print(const char* message, const vec3<quantity_or_unit_type>& a) -> void {
		std::cout << message << ": " << a.as_default_units().x << ", " << a.as_default_units().y << ", " << a.as_default_units().z << std::endl;
	}

	template <is_unit_or_quantity quantity_or_unit_type_a, is_unit_or_quantity quantity_or_unit_type_b>
	auto max(const vec3<quantity_or_unit_type_a>& a, const vec3<quantity_or_unit_type_b>& b) -> vec3<quantity_or_unit_type_a> {
		if (glm::max(a.as_default_units(), b.as_default_units()) == a.as_default_units()) {
			return a;
		}
		return b;
	}

	template <is_unit_or_quantity quantity_or_unit_type>
	auto min(const vec3<quantity_or_unit_type>& a, const vec3<quantity_or_unit_type>& b) -> vec3<quantity_or_unit_type> {
		if (glm::min(a.as_default_units(), b.as_default_units()) == a.as_default_units()) {
			return a;
		}
		return b;
	}

	template <is_unit_or_quantity quantity_or_unit_type_a, is_unit_or_quantity quantity_or_unit_type_b>
		requires has_same_quantity_tag<quantity_or_unit_type_a, quantity_or_unit_type_b>
	auto max(const vec3<quantity_or_unit_type_a>& a, const vec3<quantity_or_unit_type_b>& b, const int index) -> typename unit_to_quantity<quantity_or_unit_type_a>::type {
		float max = a.as_default_units()[index];
		if (b.as_default_units()[index] > a.as_default_units()[index]) {
			max = b.as_default_units()[index];
		}

		return convert_value_to_quantity<quantity_or_unit_type_a>(max);
	}

	template <is_unit_or_quantity quantity_or_unit_type_a, is_unit_or_quantity quantity_or_unit_type_b>
		requires has_same_quantity_tag<quantity_or_unit_type_a, quantity_or_unit_type_b>
	auto min(const vec3<quantity_or_unit_type_a>& a, const vec3<quantity_or_unit_type_b>& b, const int index) -> typename unit_to_quantity<quantity_or_unit_type_a>::type {
		float min = a.as_default_units()[index];

		if (b.as_default_units()[index] < a.as_default_units()[index]) {
			min = b.as_default_units()[index];
		}

		return convert_value_to_quantity<quantity_or_unit_type_a>(min);
	}

	template <is_unit_or_quantity quantity_or_unit_type_a, is_unit_or_quantity quantity_or_unit_type_b>
	auto dot(const vec3<quantity_or_unit_type_a>& a, const vec3<quantity_or_unit_type_b>& b) -> float {
		return glm::dot(a.as_default_units(), b.as_default_units());
	}

	template <is_unit_or_quantity quantity_or_unit_type_a, is_unit_or_quantity quantity_or_unit_type_b>
		requires has_same_quantity_tag<quantity_or_unit_type_a, quantity_or_unit_type_b>
	auto epsilon_equal(const vec3<quantity_or_unit_type_a>& a, const vec3<quantity_or_unit_type_b>& b, const float epsilon = 0.00001f) -> bool {
		return a.as_default_units().x - b.as_default_units().x < epsilon &&
			   a.as_default_units().y - b.as_default_units().y < epsilon && 
			   a.as_default_units().z - b.as_default_units().z < epsilon;
	}

	template <is_unit_or_quantity quantity_or_unit_type_a, is_unit_or_quantity quantity_or_unit_type_b>
		requires has_same_quantity_tag<quantity_or_unit_type_a, quantity_or_unit_type_b>
	auto epsilon_equal_index(const vec3<quantity_or_unit_type_a>& a, const vec3<quantity_or_unit_type_b>& b, const int index, const float epsilon = 0.00001f) -> bool {
		return a.as_default_units()[index] - b.as_default_units()[index] < epsilon;
	}

	template <is_unit_or_quantity quantity_or_unit_type>
	auto is_zero(const vec3<quantity_or_unit_type>& a) -> bool {
		return a.as_default_units().x < 0.00001f && a.as_default_units().y < 0.00001f && a.as_default_units().z < 0.00001f;
	}

	template <is_unit_or_quantity quantity_or_unit_type>
	auto magnitude(const vec3<quantity_or_unit_type>& a) -> typename unit_to_quantity<quantity_or_unit_type>::type {
		if (is_zero(a)) return typename unit_to_quantity<quantity_or_unit_type>::type();
		return convert_value_to_quantity<quantity_or_unit_type>(glm::length(a.as_default_units()));
	}

	template <is_unit_or_quantity quantity_or_unit_type>
	auto normalize(const vec3<quantity_or_unit_type>& a) -> vec3<quantity_or_unit_type> {
		if (is_zero(a)) return vec3<quantity_or_unit_type>(glm::vec3(0.0f));
		return vec3<quantity_or_unit_type>(glm::normalize(a.as_default_units()));
	}

	template <is_unit_or_quantity quantity_or_unit_type_a, is_unit_or_quantity quantity_or_unit_type_b>
		requires has_same_quantity_tag<quantity_or_unit_type_a, quantity_or_unit_type_b>
	auto cross(const vec3<quantity_or_unit_type_a>& a, const vec3<quantity_or_unit_type_b>& b) -> vec3<quantity_or_unit_type_a> {
		return vec3<quantity_or_unit_type_a>(glm::cross(a.as_default_units(), b.as_default_units()));
	}
}
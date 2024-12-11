#pragma once

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>

#include "Physics/Vector/Vec3.h"

namespace gse {
	static std::random_device rd;
	static std::mt19937 gen(rd());

	template <typename number_type>
		requires std::is_floating_point_v<number_type> || std::is_integral_v<number_type>
	number_type random_value(number_type min, number_type max) {
		std::uniform_real_distribution<number_type> dis(min, max);
		return dis(gen);
	}
}

namespace gse {
	enum : std::uint8_t {
		x, y, z 
	};

	template <is_unit_or_quantity quantity_or_unit_type>
	void print(const char* message, const vec3<quantity_or_unit_type>& a) {
		std::cout << message << ": " << a.as_default_units().x << ", " << a.as_default_units().y << ", " << a.as_default_units().z << std::endl;
	}

	template <is_unit_or_quantity quantity_or_unit_type_a, is_unit_or_quantity quantity_or_unit_type_b>
	vec3<quantity_or_unit_type_a> max(const vec3<quantity_or_unit_type_a>& a, const vec3<quantity_or_unit_type_b>& b) {
		if (glm::max(a.as_default_units(), b.as_default_units()) == a.as_default_units()) {
			return a;
		}
		return b;
	}

	template <is_unit_or_quantity quantity_or_unit_type>
	vec3<quantity_or_unit_type> min(const vec3<quantity_or_unit_type>& a, const vec3<quantity_or_unit_type>& b) {
		if (glm::min(a.as_default_units(), b.as_default_units()) == a.as_default_units()) {
			return a;
		}
		return b;
	}

	template <is_unit_or_quantity quantity_or_unit_type_a, is_unit_or_quantity quantity_or_unit_type_b>
		requires has_same_quantity_tag<quantity_or_unit_type_a, quantity_or_unit_type_b>
	typename unit_to_quantity<quantity_or_unit_type_a>::type max(const vec3<quantity_or_unit_type_a>& a, const vec3<quantity_or_unit_type_b>& b, const int index) {
		float max = a.as_default_units()[index];
		if (b.as_default_units()[index] > a.as_default_units()[index]) {
			max = b.as_default_units()[index];
		}

		return convert_value_to_quantity<quantity_or_unit_type_a>(max);
	}

	template <is_unit_or_quantity quantity_or_unit_type_a, is_unit_or_quantity quantity_or_unit_type_b>
		requires has_same_quantity_tag<quantity_or_unit_type_a, quantity_or_unit_type_b>
	typename unit_to_quantity<quantity_or_unit_type_a>::type min(const vec3<quantity_or_unit_type_a>& a, const vec3<quantity_or_unit_type_b>& b, const int index) {
		float min = a.as_default_units()[index];

		if (b.as_default_units()[index] < a.as_default_units()[index]) {
			min = b.as_default_units()[index];
		}

		return convert_value_to_quantity<quantity_or_unit_type_a>(min);
	}

	template <is_unit_or_quantity quantity_or_unit_type_a, is_unit_or_quantity quantity_or_unit_type_b>
	float dot(const vec3<quantity_or_unit_type_a>& a, const vec3<quantity_or_unit_type_b>& b) {
		return glm::dot(a.as_default_units(), b.as_default_units());
	}

	template <is_unit_or_quantity quantity_or_unit_type_a, is_unit_or_quantity quantity_or_unit_type_b>
		requires has_same_quantity_tag<quantity_or_unit_type_a, quantity_or_unit_type_b>
	bool epsilon_equal(const vec3<quantity_or_unit_type_a>& a, const vec3<quantity_or_unit_type_b>& b, const float epsilon = 0.00001f) {
        return glm::all(glm::epsilonEqual(a.as_default_units(), b.as_default_units(), epsilon));
	}

	template <is_unit_or_quantity quantity_or_unit_type_a, is_unit_or_quantity quantity_or_unit_type_b>
		requires has_same_quantity_tag<quantity_or_unit_type_a, quantity_or_unit_type_b>
	bool epsilon_equal_index(const vec3<quantity_or_unit_type_a>& a, const vec3<quantity_or_unit_type_b>& b, const int index, const float epsilon = 0.00001f) {
		return glm::epsilonEqual(a.as_default_units()[index], b.as_default_units()[index], epsilon);
	}

	template <is_unit_or_quantity quantity_or_unit_type>
	bool is_zero(const vec3<quantity_or_unit_type>& a) {
		return glm::all(glm::epsilonEqual(a.as_default_units(), vec3<quantity_or_unit_type>(0.0f).as_default_units(), 0.00001f));
	}

	template <is_unit_or_quantity quantity_or_unit_type>
	typename unit_to_quantity<quantity_or_unit_type>::type magnitude(const vec3<quantity_or_unit_type>& a) {
		if (is_zero(a)) return typename unit_to_quantity<quantity_or_unit_type>::type();
		return convert_value_to_quantity<quantity_or_unit_type>(glm::length(a.as_default_units()));
	}

	template <is_unit_or_quantity quantity_or_unit_type>
	vec3<quantity_or_unit_type> normalize(const vec3<quantity_or_unit_type>& a) {
		if (is_zero(a)) return vec3<quantity_or_unit_type>(glm::vec3(0.0f));
		return vec3<quantity_or_unit_type>(glm::normalize(a.as_default_units()));
	}

	template <is_unit_or_quantity quantity_or_unit_type_a, is_unit_or_quantity quantity_or_unit_type_b>
		requires has_same_quantity_tag<quantity_or_unit_type_a, quantity_or_unit_type_b>
	vec3<quantity_or_unit_type_a> cross(const vec3<quantity_or_unit_type_a>& a, const vec3<quantity_or_unit_type_b>& b) {
		return vec3<quantity_or_unit_type_a>(glm::cross(a.as_default_units(), b.as_default_units()));
	}
}
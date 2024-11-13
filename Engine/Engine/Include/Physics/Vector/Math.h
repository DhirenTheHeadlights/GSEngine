#pragma once

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>

#include "Physics/Vector/Vec3.h"

namespace Engine {
	static std::random_device rd;
	static std::mt19937 gen(rd());

	template <typename T>
		requires std::is_floating_point_v<T> || std::is_integral_v<T>
	T randomValue(T min, T max) {
		std::uniform_real_distribution<T> dis(min, max);
		return dis(gen);
	}
}

namespace Engine {
	template <typename T>
	void print(const char* message, const Vec3<T>& a) {
		std::cout << message << ": " << a.asDefaultUnits().x << ", " << a.asDefaultUnits().y << ", " << a.asDefaultUnits().z << std::endl;
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
	Vec3<T> max(const Vec3<T>& a, const Vec3<U>& b) {
		if (glm::max(a.asDefaultUnits(), b.asDefaultUnits()) == a.asDefaultUnits()) {
			return a;
		}
		return b;
	}

	template <IsQuantityOrUnit T>
	Vec3<T> min(const Vec3<T>& a, const Vec3<T>& b) {
		if (glm::min(a.asDefaultUnits(), b.asDefaultUnits()) == a.asDefaultUnits()) {
			return a;
		}
		return b;
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
		requires IsSameQuantityTag<T, U>
	typename UnitToQuantity<T>::Type max(const Vec3<T>& a, const Vec3<U>& b, const int index) {
		float max = a.asDefaultUnits()[index];
		if (b.asDefaultUnits()[index] > a.asDefaultUnits()[index]) {
			max = b.asDefaultUnits()[index];
		}

		if constexpr (IsUnit<T>) {
			return T(max);
		}
		else if constexpr (IsQuantity<T>) {
			return typename UnitToQuantity<T>::Type(UnitToQuantity<T>::Type::DefaultUnit(max));
		}

		return typename UnitToQuantity<T>::Type(UnitToQuantity<T>::Type::DefaultUnit(max));
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
		requires IsSameQuantityTag<T, U>
	typename UnitToQuantity<T>::Type min(const Vec3<T>& a, const Vec3<U>& b, const int index) {
		float min = a.asDefaultUnits()[index];

		if (b.asDefaultUnits()[index] < a.asDefaultUnits()[index]) {
			min = b.asDefaultUnits()[index];
		}

		if constexpr (IsUnit<T>) {
			return T(min);
		}
		else if constexpr (IsQuantity<T>) {
			return convertValueToQuantity<T>(min);
		}

		return convertValueToQuantity<T>(min);
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
	float dot(const Vec3<T>& a, const Vec3<U>& b) {
		return glm::dot(a.asDefaultUnits(), b.asDefaultUnits());
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
		requires IsSameQuantityTag<T, U>
	bool epsilonEqual(const Vec3<T>& a, const Vec3<U>& b, const float epsilon = 0.00001f) {
        return glm::all(glm::epsilonEqual(a.asDefaultUnits(), b.asDefaultUnits(), epsilon));
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
		requires IsSameQuantityTag<T, U>
	bool epsilonEqualIndex(const Vec3<T>& a, const Vec3<U>& b, const int index, const float epsilon = 0.00001f) {
		return glm::epsilonEqual(a.asDefaultUnits()[index], b.asDefaultUnits()[index], epsilon);
	}

	template <IsQuantityOrUnit T>
	bool isZero(const Vec3<T>& a) {
		return glm::all(glm::epsilonEqual(a.asDefaultUnits(), Vec3<T>(0.0f).asDefaultUnits(), 0.00001f));
	}

	template <IsQuantityOrUnit T>
	typename UnitToQuantity<T>::Type magnitude(const Vec3<T>& a) {
		if (isZero(a)) return typename UnitToQuantity<T>::Type();
		return convertValueToQuantity<T>(glm::length(a.asDefaultUnits()));
	}

	template <IsQuantityOrUnit T>
	Vec3<T> normalize(const Vec3<T>& a) {
		if (isZero(a)) return Vec3<T>(glm::vec3(0.0f));
		return Vec3<T>(glm::normalize(a.asDefaultUnits()));
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
		requires IsSameQuantityTag<T, U>
	Vec3<T> cross(const Vec3<T>& a, const Vec3<U>& b) {
		return Vec3<T>(glm::cross(a.asDefaultUnits(), b.asDefaultUnits()));
	}
}
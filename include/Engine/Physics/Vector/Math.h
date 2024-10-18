#pragma once

#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>

#include "Engine/Physics/Vector/Vec3.h"

namespace Engine {
	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
	Vec3<T> max(const Vec3<T>& a, const Vec3<U>& b) {
		if (glm::max(a.rawVec3(), b.rawVec3()) == a.rawVec3()) {
			return a;
		}
		return b;
	}

	template <IsQuantityOrUnit T>
	Vec3<T> min(const Vec3<T>& a, const Vec3<T>& b) {
		if (glm::min(a.rawVec3(), b.rawVec3()) == a.rawVec3()) {
			return a;
		}
		return b;
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
		requires IsSameQuantityTag<T, U>
	typename UnitToQuantity<T>::Type max(const Vec3<T>& a, const Vec3<U>& b, const int index) {
		float max = a.rawVec3()[index];
		if (b.rawVec3()[index] > a.rawVec3()[index]) {
			max = b.rawVec3()[index];
		}

		if constexpr (IsUnit<T>) {
			return T(max);
		}
		else if constexpr (IsQuantity<T>) {
			return typename UnitToQuantity<T>::Type(max);
		}

		return typename UnitToQuantity<T>::Type(max);
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
		requires IsSameQuantityTag<T, U>
	typename UnitToQuantity<T>::Type min(const Vec3<T>& a, const Vec3<U>& b, const int index) {
		float min = a.rawVec3()[index];

		if (b.rawVec3()[index] < a.rawVec3()[index]) {
			min = b.rawVec3()[index];
		}

		if constexpr (IsUnit<T>) {
			return T(min);
		}
		else if constexpr (IsQuantity<T>) {
			return typename UnitToQuantity<T>::Type(min);
		}

		return typename UnitToQuantity<T>::Type(min);
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
	float dot(const Vec3<T>& a, const Vec3<U>& b) {
		return glm::dot(a.rawVec3(), b.rawVec3());
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
		requires IsSameQuantityTag<T, U>
	bool epsilonEqual(const Vec3<T>& a, const Vec3<U>& b, const float epsilon = 0.00001f) {
        return glm::all(glm::epsilonEqual(a.rawVec3(), b.rawVec3(), epsilon));
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
		requires IsSameQuantityTag<T, U>
	bool epsilonEqualIndex(const Vec3<T>& a, const Vec3<U>& b, const int index, const float epsilon = 0.00001f) {
		return glm::epsilonEqual(a.rawVec3()[index], b.rawVec3()[index], epsilon);
	}

	template <IsQuantityOrUnit T>
	bool isZero(const Vec3<T>& a) {
		return glm::all(glm::epsilonEqual(a.rawVec3(), Vec3<T>(0.0f).rawVec3(), 0.00001f));
	}

	template <IsQuantityOrUnit T>
	typename UnitToQuantity<T>::Type magnitude(const Vec3<T>& a) {
		if (isZero(a)) return typename UnitToQuantity<T>::Type(0.0f);
		return typename UnitToQuantity<T>::Type(glm::length(a.rawVec3()));
	}

	template <IsQuantityOrUnit T>
	Vec3<T> normalize(const Vec3<T>& a) {
		if (isZero(a)) return Vec3<T>(glm::vec3(0.0f));
		return Vec3<T>(glm::normalize(a.rawVec3()));
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
		requires IsSameQuantityTag<T, U>
	Vec3<T> cross(const Vec3<T>& a, const Vec3<U>& b) {
		return Vec3<T>(glm::cross(a.rawVec3(), b.rawVec3()));
	}
}
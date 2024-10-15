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
}
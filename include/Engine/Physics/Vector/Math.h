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

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
	Vec3<T> min(const Vec3<T>& a, const Vec3<U>& b) {
		if (glm::min(a.rawVec3(), b.rawVec3()) == a.rawVec3()) {
			return a;
		}
		return b;
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
	float max(const Vec3<T>& a, const Vec3<U>& b, const int index) {
		if (a.rawVec3()[index] > b.rawVec3()[index]) {
			return a.rawVec3()[index];
		}
		return b.rawVec3()[index];
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
	float min(const Vec3<T>& a, const Vec3<U>& b, const int index) {
		if (a.rawVec3()[index] < b.rawVec3()[index]) {
			return a.rawVec3()[index];
		}
		return b.rawVec3()[index];
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
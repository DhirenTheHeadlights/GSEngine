#pragma once

#include <cmath>
#include <glm/glm.hpp>

#include "Engine/Physics/Vector/Vec3.h"

namespace Engine {

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
	Vec3<T> max(const Vec3<T>& a, const Vec3<U>& b) {
		return glm::max(a.rawVec3(), b.rawVec3());
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
	Vec3<T> min(const Vec3<T>& a, const Vec3<U>& b) {
		return glm::min(a.rawVec3(), b.rawVec3());
	}

}
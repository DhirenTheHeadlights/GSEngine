#pragma once

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>
#include "Physics/Units/UnitToQuantityDefinitions.h"

namespace Engine {
	template <typename T>
	struct GetQuantityTagType;

	template <typename T>
	struct GetQuantityTagType {
		using Type = typename UnitToQuantity<T>::Type;
	};

	template <typename T, typename U>
	concept IsSameQuantityTag = std::is_same_v<typename GetQuantityTagType<T>::Type, typename GetQuantityTagType<U>::Type>;

	template <typename T>
	concept IsQuantity = std::is_base_of_v<Quantity<T, typename T::DefaultUnit, typename T::Units>, T>;

	template <typename T>
	concept IsQuantityOrUnit = IsQuantity<T> || IsUnit<T>;

	template <typename T, typename... Args>
	concept AreValidVectorArgs = ((std::is_convertible_v<Args, T> || std::is_same_v<Args, float> || IsQuantityOrUnit<Args>) && ...);

	template <typename T>
	concept IsUnitless = std::is_same_v<T, Unitless> || std::is_same_v<T, Unitless>;

	template <typename T, typename U>
	[[nodiscard]] static float getValue(const U& argument) {
		if constexpr (IsQuantity<U>) {
			return argument.asDefaultUnit().getValue();
		}
		else if constexpr (IsUnit<U>) {
			return argument.getValue();
		}
		else if constexpr (std::is_convertible_v<U, float>) {
			float value = static_cast<float>(argument);
			if constexpr (IsUnit<T>) {
				return T(value).getValue();
			}

			return value; // Assume value is in default units
		}
		return 0.0f;
	}

	template <typename T, typename... Args>
	[[nodiscard]] static glm::vec3 createVec(Args&&... args) {
		if constexpr (sizeof...(Args) == 0) {
			return glm::vec3(0.0f);
		}
		else if constexpr (sizeof...(Args) == 1) {
			const float value = getValue<T>(std::forward<Args>(args)...);
			return glm::vec3(value);
		}
		else {
			return glm::vec3(getValue<T>(std::forward<Args>(args))...);
		}
	}

	template <IsQuantityOrUnit T>
	struct Vec3 {
		using QuantityType = typename UnitToQuantity<T>::Type;

		Vec3() = default;

		template <typename... Args>
			requires ((sizeof...(Args) == 0 || sizeof...(Args) == 1 || sizeof...(Args) == 3) && AreValidVectorArgs<T, Args...>)
		Vec3(Args&&... args)
			: vec(createVec<T>(std::forward<Args>(args)...)) {
		}

		Vec3(const glm::vec3& vec3) {
			if constexpr (IsUnit<T>) {
				vec = glm::vec3(T(vec3.x).getValue(), T(vec3.y).getValue(), T(vec3.z).getValue());
			}
			else if constexpr (IsQuantity<T>) {
				vec = vec3; // Assume vec3 is in default units
			}
		}

		template <IsUnit Unit>
			requires IsSameQuantityTag<T, Unit>
		[[nodiscard]] glm::vec3 as() const {
			const float convertedMagnitude = length(vec) * Unit(1.0f).getValue();
			if (const auto zero = glm::vec3(0.0f); vec == zero) {
				return zero;
			}
			return normalize(vec) * convertedMagnitude;
		}

		// Converter between Vec3<Unit> and Vec3<Quantity>
		template <IsQuantityOrUnit U>
			requires IsSameQuantityTag<T, U>
		Vec3(const Vec3<U>& other)
			: vec(other.rawVec3()) {}

		// Converter between Vec3<Unitless> and Vec3<T>
		template <IsUnitless U>
		Vec3(const Vec3<U>& other)
			: vec(other.rawVec3()) {}

		/// Arithmetic operators

		template <typename U>
			requires IsSameQuantityTag<T, U>
		Vec3 operator+(const Vec3<U>& rhs) const {
			return Vec3(vec + rhs.rawVec3());
		}

		template <typename U>
			requires IsSameQuantityTag<T, U>
		Vec3 operator-(const Vec3<U>& rhs) const {
			return Vec3(vec - rhs.rawVec3());
		}

		Vec3 operator*(const float scalar) const {
			return Vec3(vec * scalar);
		}

		Vec3 operator/(const float scalar) const {
			return Vec3(vec / scalar);
		}

		/// Compound arithmetic operators

		template <IsQuantityOrUnit U>
			requires IsSameQuantityTag<T, U>
		Vec3& operator+=(const Vec3<U>& other) {
			vec += other.rawVec3();
			return *this;
		}

		template <IsQuantityOrUnit U>
			requires IsSameQuantityTag<T, U>
		Vec3& operator-=(const Vec3<U>& other) {
			vec -= other.rawVec3();
			return *this;
		}

		Vec3& operator*=(const float scalar) {
			vec *= scalar;
			return *this;
		}

		Vec3& operator/=(const float scalar) {
			vec /= scalar;
			return *this;
		}

		/// Casts
		Vec3& operator-() {
			vec = -vec;
			return *this;
		}

		/// Comparison Operators

		bool operator==(const Vec3& other) const {
			return vec == other.vec;
		}

		bool operator!=(const Vec3& other) const {
			return !(*this == other);
		}

		// Non-const getter for the raw vector
		// Caution: Not unit safe
		[[nodiscard]] glm::vec3& rawVec3() {
			return vec;
		}

		// Const getter for the raw vector
		// Caution: Not unit safe
		[[nodiscard]] const glm::vec3& rawVec3() const {
			return vec;
		}

	protected:
		glm::vec3 vec = glm::vec3(0.0f);
	};

	/// Unitless arithmetic overloads

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
	auto operator*(const Vec3<T>& lhs, const Vec3<U>& rhs) {
		if constexpr (IsUnitless<T> && !IsUnitless<U>) {
			return Vec3<U>(lhs.rawVec3() * rhs.rawVec3());
		}
		else if constexpr (!IsUnitless<T> && IsUnitless<U>) {
			return Vec3<T>(lhs.rawVec3() * rhs.rawVec3());
		}
		else {
			return Vec3<Unitless>(lhs.rawVec3() * rhs.rawVec3());
		}
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
	auto operator/(const Vec3<T>& lhs, const Vec3<U>& rhs) {
		if constexpr (IsUnitless<T> && !IsUnitless<U>) {
			return Vec3<U>(lhs.rawVec3() / rhs.rawVec3());
		}
		else if constexpr (!IsUnitless<T> && IsUnitless<U>) {
			return Vec3<T>(lhs.rawVec3() / rhs.rawVec3());
		}
		else {
			return Vec3<Unitless>(lhs.rawVec3() / rhs.rawVec3());
		}
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
	auto operator*(const Vec3<T>& lhs, const U& rhs) {
		if constexpr (IsUnitless<T> && !IsUnitless<U>) {
			return Vec3<U>(lhs.rawVec3() * getValue<U>(rhs));
		}
		else if constexpr (IsUnitless<U> && !IsUnitless<T>) {
			return Vec3<T>(lhs.rawVec3() * getValue<T>(rhs));
		}
		else {
			return Vec3<Unitless>(lhs.rawVec3() * getValue<Unitless>(rhs));
		}
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
	auto operator*(const T& lhs, const Vec3<U>& rhs) {
		if constexpr (IsUnitless<T> && !IsUnitless<U>) {
			return Vec3<U>(getValue<U>(lhs) * rhs.rawVec3());
		}
		else if constexpr (IsUnitless<U> && !IsUnitless<T>) {
			return Vec3<T>(getValue<T>(lhs) * rhs.rawVec3());
		}
		else {
			return Vec3<Unitless>(getValue<Unitless>(lhs) * rhs.rawVec3());
		}
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
	auto operator/(const Vec3<T>& lhs, const U& rhs) {
		if constexpr (IsUnitless<T> && !IsUnitless<U>) {
			return Vec3<U>(lhs.rawVec3() / getValue<U>(rhs));
		}
		else if constexpr (IsUnitless<U> && !IsUnitless<T>) {
			return Vec3<T>(lhs.rawVec3() / getValue<T>(rhs));
		}
		else {
			return Vec3<Unitless>(lhs.rawVec3() / getValue<Unitless>(rhs));
		}
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
	auto operator/(const T& lhs, const Vec3<U> rhs) {
		if constexpr (IsUnitless<T> && !IsUnitless<U>) {
			return Vec3<U>(getValue<U>(lhs) / rhs.rawVec3());
		}
		else if constexpr (IsUnitless<U> && !IsUnitless<T>) {
			return Vec3<T>(getValue<T>(lhs) / rhs.rawVec3());
		}
		else {
			return Vec3<Unitless>(getValue<Unitless>(lhs) / rhs.rawVec3());
		}
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
	auto operator*(const T& lhs, const U& rhs) {
		if constexpr (IsUnitless<T> && !IsUnitless<U>) {
			return U(getValue<U>(lhs) * getValue<U>(rhs));
		}
		else if constexpr (IsUnitless<U> && !IsUnitless<T>) {
			return T(getValue<T>(lhs) * getValue<T>(rhs));
		}
		else {
			return Unitless(getValue<Unitless>(lhs) * getValue<Unitless>(rhs));
		}
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
	auto operator/(const T& lhs, const U& rhs) {
		if constexpr (IsUnitless<T> && !IsUnitless<U>) {
			return U(getValue<U>(lhs) / getValue<U>(rhs));
		}
		else if constexpr (IsUnitless<U> && !IsUnitless<T>) {
			return T(getValue<T>(lhs) / getValue<T>(rhs));
		}
		else {
			return Unitless(getValue<Unitless>(lhs) / getValue<Unitless>(rhs));
		}
	}
}

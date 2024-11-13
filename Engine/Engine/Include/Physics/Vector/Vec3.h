#pragma once

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>
#include "Physics/Units/UnitToQuantityDefinitions.h"

namespace Engine {
	template <typename T>
	concept IsQuantity = std::is_base_of_v<QuantityBase, T>;

	template <typename T>
	concept IsQuantityOrUnit = IsQuantity<T> || IsUnit<T>;

	template <typename T>
	struct GetQuantityTagType;

	template <typename T>
	struct GetQuantityTagType {
		using Type = typename UnitToQuantity<T>::Type;
	};

	template <typename T, typename U>
	concept IsSameQuantityTag = std::is_same_v<typename GetQuantityTagType<T>::Type, typename GetQuantityTagType<U>::Type>;

	template <typename T, typename... Args>
	concept AreValidVectorArgs = ((IsSameQuantityTag<Args, T> || std::is_same_v<Args, float> || IsQuantityOrUnit<Args>) && ...);

	template <typename T>
	concept IsUnitless = std::is_same_v<T, Unitless>;

	template <IsQuantityOrUnit T>
	float convertValueToDefaultUnit(const float value) {
		using QuantityType = typename UnitToQuantity<T>::Type;
		return QuantityType::template from<typename QuantityType::DefaultUnit>(value).asDefaultUnit();
	}

	template <IsQuantityOrUnit T>
	typename UnitToQuantity<T>::Type convertValueToQuantity(const float value) {
		using QuantityType = typename UnitToQuantity<T>::Type;
		return QuantityType::template from<typename QuantityType::DefaultUnit>(value);
	}

	template <typename T, typename U>
	[[nodiscard]] static float getValue(const U& argument) {
		if constexpr (IsQuantity<U>) {
			return argument.asDefaultUnit();
		}
		else if constexpr (std::is_convertible_v<U, float>) {
			const float value = static_cast<float>(argument);
			if constexpr (IsUnit<T>) {
				return convertValueToDefaultUnit<T>(value);
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
		Vec3(Args&&... args) : vec(createVec<T>(std::forward<Args>(args)...)) {}

		Vec3(const glm::vec3& vec3) {
			if constexpr (IsUnit<T>) {
				vec = { QuantityType<T>(vec3.x).asDefaultUnit(), QuantityType<T>(vec3.y).asDefaultUnit(), QuantityType<T>(vec3.z).asDefaultUnit() };
			}
			else if constexpr (IsQuantity<T>) {
				vec = vec3; // Assume vec3 is in default units
			}
		}

		template <IsUnit Unit>
			requires IsSameQuantityTag<T, Unit>
		[[nodiscard]] glm::vec3 as() const {
			const float convertedMagnitude = length(vec) * Unit::ConversionFactor;
			if (constexpr auto zero = glm::vec3(0.0f); vec == zero) {
				return zero;
			}
			return normalize(vec) * convertedMagnitude;
		}

		// Converter between Vec3<Unit> and Vec3<Quantity>
		template <IsQuantityOrUnit U>
			requires IsSameQuantityTag<T, U>
		Vec3(const Vec3<U>& other) : vec(other.asDefaultUnits()) {}

		// Converter between Vec3<Unitless> and Vec3<T>
		template <IsUnitless U>
		Vec3(const Vec3<U>& other) : vec(other.asDefaultUnits()) {}

		/// Arithmetic operators

		template <typename U>
			requires IsSameQuantityTag<T, U>
		Vec3 operator+(const Vec3<U>& rhs) const {
			return Vec3(vec + rhs.asDefaultUnits());
		}

		template <typename U>
			requires IsSameQuantityTag<T, U>
		Vec3 operator-(const Vec3<U>& rhs) const {
			return Vec3(vec - rhs.asDefaultUnits());
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
			vec += other.asDefaultUnits();
			return *this;
		}

		template <IsQuantityOrUnit U>
			requires IsSameQuantityTag<T, U>
		Vec3& operator-=(const Vec3<U>& other) {
			vec -= other.asDefaultUnits();
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

		[[nodiscard]] glm::vec3& asDefaultUnits() {
			return vec;
		}

		[[nodiscard]] const glm::vec3& asDefaultUnits() const {
			return vec;
		}

	protected:
		glm::vec3 vec = glm::vec3(0.0f);
	};

	/// Unitless arithmetic overloads

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
		requires IsUnitless<T> || IsUnitless<U>
	auto operator*(const Vec3<T>& lhs, const Vec3<U>& rhs) {
		if constexpr (IsUnitless<T> && !IsUnitless<U>) {
			return Vec3<U>(lhs.asDefaultUnits() * rhs.asDefaultUnits());
		}
		else if constexpr (!IsUnitless<T> && IsUnitless<U>) {
			return Vec3<T>(lhs.asDefaultUnits() * rhs.asDefaultUnits());
		}
		else {
			return Vec3<Unitless>(lhs.asDefaultUnits() * rhs.asDefaultUnits());
		}
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
		requires IsUnitless<T> || IsUnitless<U>
	auto operator/(const Vec3<T>& lhs, const Vec3<U>& rhs) {
		if constexpr (IsUnitless<T> && !IsUnitless<U>) {
			return Vec3<U>(lhs.asDefaultUnits() / rhs.asDefaultUnits());
		}
		else if constexpr (!IsUnitless<T> && IsUnitless<U>) {
			return Vec3<T>(lhs.asDefaultUnits() / rhs.asDefaultUnits());
		}
		else {
			return Vec3<Unitless>(lhs.asDefaultUnits() / rhs.asDefaultUnits());
		}
	}

	template <IsQuantityOrUnit T, IsQuantity U>
		requires IsUnitless<T> || IsUnitless<U>
	auto operator*(const Vec3<T>& lhs, const U& rhs) {
		if constexpr (IsUnitless<T> && !IsUnitless<U>) {
			return Vec3<U>(lhs.asDefaultUnits() * rhs.asDefaultUnit());
		}
		else if constexpr (IsUnitless<U> && !IsUnitless<T>) {
			return Vec3<T>(lhs.asDefaultUnits() * rhs.asDefaultUnit());
		}
		else {
			return Vec3<Unitless>(lhs.asDefaultUnits() * getValue<Unitless>(rhs));
		}
	}

	template <IsQuantity T, IsQuantityOrUnit U>
		requires IsUnitless<T> || IsUnitless<U>
	auto operator*(const T& lhs, const Vec3<U>& rhs) {
		if constexpr (IsUnitless<T> && !IsUnitless<U>) {
			return Vec3<U>(lhs.asDefaultUnit() * rhs.asDefaultUnits());
		}
		else if constexpr (IsUnitless<U> && !IsUnitless<T>) {
			return Vec3<T>(lhs.asDefaultUnit() * rhs.asDefaultUnits());
		}
		else {
			return Vec3<Unitless>(getValue<Unitless>(lhs) * rhs.asDefaultUnits());
		}
	}

	template <IsQuantityOrUnit T, IsQuantity U>
		requires IsUnitless<T> || IsUnitless<U>
	auto operator/(const Vec3<T>& lhs, const U& rhs) {
		if constexpr (IsUnitless<T> && !IsUnitless<U>) {
			return Vec3<U>(lhs.asDefaultUnits() / rhs.asDefaultUnit());
		}
		else if constexpr (IsUnitless<U> && !IsUnitless<T>) {
			return Vec3<T>(lhs.asDefaultUnits() / rhs.asDefaultUnit());
		}
		else {
			return Vec3<Unitless>(lhs.asDefaultUnits() / rhs.asDefaultUnit());
		}
	}

	template <IsQuantity T, IsQuantityOrUnit U>
		requires IsUnitless<T> || IsUnitless<U>
	auto operator/(const T& lhs, const Vec3<U> rhs) {
		if constexpr (IsUnitless<T> && !IsUnitless<U>) {
			return Vec3<U>(lhs.asDefaultUnit() / rhs.asDefaultUnits());
		}
		else if constexpr (IsUnitless<U> && !IsUnitless<T>) {
			return Vec3<T>(lhs.asDefaultUnit() / rhs.asDefaultUnits());
		}
		else {
			return Vec3<Unitless>(getValue<Unitless>(lhs) / rhs.asDefaultUnits());
		}
	}

	template <IsQuantity T, IsQuantity U>
		requires IsUnitless<T> || IsUnitless<U>
	auto operator*(const T& lhs, const U& rhs) {
		if constexpr (IsUnitless<T> && !IsUnitless<U>) {
			return convertValueToQuantity<U>(lhs.asDefaultUnit() * rhs.asDefaultUnit());
		}
		else if constexpr (IsUnitless<U> && !IsUnitless<T>) {
			return convertValueToQuantity<T>(lhs.asDefaultUnit() * rhs.asDefaultUnit());
		}
		else {
			return Unitless(lhs.asDefaultUnit() * rhs.asDefaultUnit());
		}
	}

	template <IsQuantity T, IsQuantity U>
		requires IsUnitless<T> || IsUnitless<U>
	auto operator/(const T& lhs, const U& rhs) {
		if constexpr (IsUnitless<T> && !IsUnitless<U>) {
			return convertValueToQuantity<U>(lhs.asDefaultUnit() / rhs.asDefaultUnit());
		}
		else if constexpr (IsUnitless<U> && !IsUnitless<T>) {
			return convertValueToQuantity<T>(lhs.asDefaultUnit() / rhs.asDefaultUnit());
		}
		else {
			return Unitless(lhs.asDefaultUnit() / rhs.asDefaultUnit());
		}
	}
}

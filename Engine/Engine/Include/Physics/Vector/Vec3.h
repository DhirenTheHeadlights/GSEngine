#pragma once

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>
#include "Physics/Units/UnitToQuantityDefinitions.h"

namespace Engine {
	template <typename T>
	concept IsQuantity = std::is_base_of_v<
		Quantity<std::remove_cvref_t<T>, typename std::remove_cvref_t<T>::DefaultUnit, typename std::remove_cvref_t<T>::Units>,
		std::remove_cvref_t<T>
	>;

	template <typename T>
	concept IsQuantityOrUnit = IsQuantity<T> || IsUnit<T>;

	template <typename T>
	struct GetQuantityTagType;

	template <typename T>
	struct GetQuantityTagType {
		using Type = typename UnitToQuantity<std::remove_cvref_t<T>>::Type;
	};

	template <typename T, typename U>
	concept HasSameQuantityTag = std::is_same_v<
		typename GetQuantityTagType<std::remove_cvref_t<T>>::Type,
		typename GetQuantityTagType<std::remove_cvref_t<U>>::Type
	>;

	template <typename T, typename... Args>
	concept AreValidVectorArgs = (((IsQuantity<Args> && HasSameQuantityTag<Args, T>) || std::is_convertible_v<Args, float>) && ...);

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

	template <IsQuantityOrUnit T = Unitless>
	struct Vec3 {
		using QuantityType = typename UnitToQuantity<T>::Type;

		template <typename... Args>
			requires ((sizeof...(Args) == 0 || sizeof...(Args) == 1 || sizeof...(Args) == 3) && AreValidVectorArgs<T, Args...>)
		Vec3(Args&&... args) : vec(createVec<T>(std::forward<Args>(args)...)) {}

		Vec3(const glm::vec3& vec3) {
			if constexpr (IsUnit<T>) {
				vec = { convertValueToDefaultUnit<T>(vec3.x), convertValueToDefaultUnit<T>(vec3.y), convertValueToDefaultUnit<T>(vec3.z) };
			}
			else if constexpr (IsQuantity<T>) {
				vec = vec3; // Assume vec3 is in default units
			}
		}

		template <IsUnit Unit>
			requires HasSameQuantityTag<T, Unit>
		[[nodiscard]] glm::vec3 as() const {
			const float convertedMagnitude = length(vec) * Unit::ConversionFactor;
			if (constexpr auto zero = glm::vec3(0.0f); vec == zero) {
				return zero;
			}
			return normalize(vec) * convertedMagnitude;
		}

		// Converter between Vec3<Unit> and Vec3<Quantity>
		template <IsQuantityOrUnit U>
			requires HasSameQuantityTag<T, U>
		Vec3(const Vec3<U>& other) : vec(other.asDefaultUnits()) {}

		[[nodiscard]] glm::vec3& asDefaultUnits() {
			return vec;
		}

		[[nodiscard]] const glm::vec3& asDefaultUnits() const {
			return vec;
		}

	protected:
		glm::vec3 vec = glm::vec3(0.0f);
	};

	/// Unit arithmetic overloads

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
		requires HasSameQuantityTag<T, U>
	auto operator+(const Vec3<T>& lhs, const Vec3<U>& rhs) {
		return Vec3<T>(lhs.asDefaultUnits() + rhs.asDefaultUnits());
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
		requires HasSameQuantityTag<T, U>
	auto operator-(const Vec3<T>& lhs, const Vec3<U>& rhs) {
		return Vec3<T>(lhs.asDefaultUnits() - rhs.asDefaultUnits());
	}

	template <IsQuantityOrUnit T>
	auto operator*(const Vec3<T>& lhs, const float scalar) {
		return Vec3<T>(lhs.asDefaultUnits() * scalar);
	}

	template <IsQuantityOrUnit T>
	auto operator*(const float scalar, const Vec3<T>& rhs) {
		return Vec3<T>(scalar * rhs.asDefaultUnits());
	}

	template <IsQuantityOrUnit T>
	auto operator/(const Vec3<T>& lhs, const float scalar) {
		return Vec3<T>(lhs.asDefaultUnits() / scalar);
	}

	/// Compound arithmetic operators

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
		requires HasSameQuantityTag<T, U>
	auto& operator+=(Vec3<T>& lhs, const Vec3<U>& rhs) {
		lhs = lhs + rhs;
		return lhs;
	}

	template <IsQuantityOrUnit T, IsQuantityOrUnit U>
		requires HasSameQuantityTag<T, U>
	auto& operator-=(Vec3<T>& lhs, const Vec3<U>& rhs) {
		lhs = lhs - rhs;
		return lhs;
	}

	template <IsQuantityOrUnit T>
	auto& operator*=(Vec3<T>& lhs, const float scalar) {
		lhs = lhs * scalar;
		return lhs;
	}

	template <IsQuantityOrUnit T>
	auto& operator/=(Vec3<T>& lhs, const float scalar) {
		lhs = lhs / scalar;
		return lhs;
	}

	/// Casts

	template <IsQuantityOrUnit T>
	auto& operator-(Vec3<T>& lhs) {
		return lhs *= -1.0f;
	}

	/// Comparison Operators

	template <IsQuantityOrUnit T>
	bool operator==(const Vec3<T>& lhs, const Vec3<T>& rhs) {
		return lhs.asDefaultUnits() == rhs.asDefaultUnits();
	}

	template <IsQuantityOrUnit T>
	bool operator!=(const Vec3<T>& lhs, const Vec3<T>& rhs) {
		return !(lhs == rhs);
	}

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
			return Vec3<>(lhs.asDefaultUnits() * rhs.asDefaultUnits());
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
			return Vec3<>(lhs.asDefaultUnits() / rhs.asDefaultUnits());
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
			return Vec3<>(lhs.asDefaultUnits() * getValue<Unitless>(rhs));
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
			return Vec3<>(getValue<Unitless>(lhs) * rhs.asDefaultUnits());
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
			return Vec3<>(lhs.asDefaultUnits() / rhs.asDefaultUnit());
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
			return Vec3<>(getValue<Unitless>(lhs) / rhs.asDefaultUnits());
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

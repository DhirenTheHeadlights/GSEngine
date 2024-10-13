#pragma once

#include <glm/glm.hpp>
#include "Engine/Physics/Units/UnitToQuantityDefinitions.h"

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

	template <IsQuantityOrUnit T>
	struct Vec3 {
		using QuantityType = typename UnitToQuantity<T>::Type;

		Vec3() = default;

		template <typename... Args>
			requires ((sizeof...(Args) == 0 || sizeof...(Args) == 1 || sizeof...(Args) == 3) && AreValidVectorArgs<T, Args...>)
		explicit Vec3(Args&&... args)
			: vec(createVec(std::forward<Args>(args)...)) {}

		explicit Vec3(const glm::vec3& vec3) {
			if constexpr (IsUnit<T>) {
				vec = glm::vec3(T(vec3.x).getValue(), T(vec3.y).getValue(), T(vec3.z).getValue());
			}
			else if constexpr (IsQuantity<T>) {
				vec = vec3; // Assume vec3 is in default units
			}
			else {
				static_assert(sizeof(T) == 0, "Unsupported type in Vec3 constructor with glm::vec3 argument");
			}
		}

		[[nodiscard]] QuantityType magnitude() const {
			if (vec == glm::vec3(0.0f)) {
				return QuantityType(0.0f);
			}
			return QuantityType(length(vec));
		}

		template <IsUnit Unit>
			requires IsSameQuantityTag<T, Unit>
		[[nodiscard]] glm::vec3 as() const {
			const float convertedMagnitude = magnitude().template as<Unit>();
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

		[[nodiscard]] glm::vec3 getDirection() const {
			if (const auto zero = glm::vec3(0.0f); vec == zero) {
				return zero;
			}
			return normalize(vec);
		}

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

		Vec3 operator*(const glm::vec3& other) const {
			return Vec3(vec * normalize(other));
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

	private:
		glm::vec3 vec;           // Direction vector with magnitude

		template <typename... Args>
		[[nodiscard]] static glm::vec3 createVec(Args&&... args) {
			if constexpr (sizeof...(Args) == 0) {
				return glm::vec3(0.0f);
			}
			else if constexpr (sizeof...(Args) == 1) {
				const float value = getValue(std::forward<Args>(args)...);
				return glm::vec3(value);
			}
			else {
				return glm::vec3(getValue(std::forward<Args>(args))...);
			}
		}

		// getValue function
		template <typename U>
		[[nodiscard]] static float getValue(const U& argument) {
			if constexpr (IsQuantity<U>) {
				// Quantity: convert to default units before getting the value
				return argument.asDefaultUnit().getValue();
			}
			else if constexpr (IsUnit<U>) {
				// Unit: use .getValue() directly
				return argument.getValue();
			}
			else if constexpr (std::is_convertible_v<U, float>) {
				float value = static_cast<float>(argument);
				if constexpr (IsUnit<T>) {
					// T is a Unit: cast float to T and get its default unit value
					return T(value).getValue();
				}
				else {
					// T is Quantity or float
					// If T is Quantity, assume float is in default units
					return value;
				}
			}
			else {
				static_assert(sizeof(U) == 0, "Unsupported type in getValue");
				return -1.0f;
			}
		}
	};
}

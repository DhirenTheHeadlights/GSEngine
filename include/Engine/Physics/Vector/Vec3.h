#pragma once

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <iostream>
#include <glm/gtx/norm.hpp>
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
		explicit Vec3(Args&&... args)
			: vec(createVec<T>(std::forward<Args>(args)...)) {
		}

		explicit Vec3(const glm::vec3& vec3) {
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

	template <typename T>
	concept IsUnitless = std::is_same_v<T, Unitless> || std::is_same_v<T, Units::Unitless>;

	/// Lambda functions for Vec3<T> and Vec3<Unitless> arithmetic
	
	template <typename Operator, IsQuantityOrUnit T, IsUnitless U>
	Vec3<T> vec3Arithmetic(const Vec3<T>& vec, const U& scalar, Operator op) {
		return Vec3<T>(op(vec.rawVec3(), getValue<T>(scalar)));
	}

	template <typename Operator, IsQuantityOrUnit T, IsUnitless U>
	Vec3<T> vec3Arithmetic(const Vec3<T>& vec, const Vec3<U>& other, Operator op) {
		return Vec3<T>(op(vec.rawVec3(), other.rawVec3()));
	}

	template <typename Operator, IsQuantityOrUnit T, IsUnitless U>
	Vec3<T> vec3CompoundArithmetic(Vec3<T>& vec, const U& scalar, Operator op) {
		vec = vec3Arithmetic(vec, scalar, op);
		return vec;
	}

	template <typename Operator, IsQuantityOrUnit T, IsUnitless U>
	Vec3<T>& vec3CompoundArithmetic(Vec3<T>& vec, const Vec3<U>& other, Operator op) {
		vec = vec3Arithmetic(vec, other, op);
		return vec;
	}

	template <typename Operator, IsUnitless U, IsQuantityOrUnit T>
	Vec3<T> vec3Arithmetic(const Vec3<U>& vec, const T& scalar, Operator op) {
		return Vec3<T>(op(vec.rawVec3(), getValue<T>(scalar)));
	}

	template <typename Operator, IsUnitless U, IsQuantityOrUnit T>
	Vec3<T> vec3Arithmetic(const Vec3<U>& vec, const Vec3<T>& other, Operator op) {
		return Vec3<T>(op(vec.rawVec3(), other.rawVec3()));
	}

	template <typename Operator, IsUnitless U, IsQuantityOrUnit T>
	Vec3<T>& vec3CompoundArithmetic(Vec3<U>& vec, const T& scalar, Operator op) {
		vec = vec3Arithmetic(vec, scalar, op);
		return vec;
	}

	template <typename Operator, IsUnitless U, IsQuantityOrUnit T>
	Vec3<T>& vec3CompoundArithmetic(Vec3<U>& vec, const Vec3<T>& other, Operator op) {
		vec = vec3Arithmetic(vec, other, op);
		return vec;
	}

	constexpr auto multiply = [](const auto& lhs, const auto& rhs) { return lhs * rhs; };
	constexpr auto divide = [](const auto& lhs, const auto& rhs) { return lhs / rhs; };

#define DEFINE_VEC3_ARITHMETIC_OPERATOR(op, operatorLambda)                        \
    /* Vec3<T> op scalar */                                                        \
    template <IsQuantityOrUnit T, IsUnitless U>                                    \
    Vec3<T> operator op (const Vec3<T>& lhs, const U& rhs) {                       \
        return vec3Arithmetic(lhs, rhs, operatorLambda);                           \
    }                                                                              \
                                                                                   \
    /* Vec3<T> op Vec3<U> */                                                       \
    template <IsQuantityOrUnit T, IsUnitless U>                                    \
    Vec3<T> operator op (const Vec3<T>& lhs, const Vec3<U>& rhs) {                 \
        return vec3Arithmetic(lhs, rhs, operatorLambda);                           \
    }                                                                              \
                                                                                   \
    /* scalar op Vec3<T> (flipped order) */                                        \
    template <IsQuantityOrUnit T, IsUnitless U>                                    \
    Vec3<T> operator op (const U& lhs, const Vec3<T>& rhs) {                       \
        return vec3Arithmetic(rhs, lhs, operatorLambda);                           \
    }                                                                              \
                                                                                   \
    /* scalar op Vec3<Unitless> */                                                 \
    template <IsUnitless U, IsQuantityOrUnit T>                                    \
    Vec3<T> operator op (const T& lhs, const Vec3<U>& rhs) {                       \
        return vec3Arithmetic(rhs, lhs, operatorLambda);                           \
    }                                                                              \
                                                                                   \
    /* Vec3<Unitless> op scalar */                                                 \
    template <IsUnitless U, IsQuantityOrUnit T>                                    \
    Vec3<T> operator op (const Vec3<U>& lhs, const T& rhs) {                       \
        return vec3Arithmetic(lhs, rhs, operatorLambda);                           \
    }                                                                              \
                                                                                   \
    /* Vec3<T> op= scalar */                                                       \
    template <IsQuantityOrUnit T, IsUnitless U>                                    \
    Vec3<T>& operator op##= (Vec3<T>& lhs, const U& rhs) {                         \
        return vec3CompoundArithmetic(lhs, rhs, operatorLambda);                   \
    }                                                                              \
                                                                                   \
    /* Vec3<T> op= Vec3<U> */                                                      \
    template <IsQuantityOrUnit T, IsUnitless U>                                    \
    Vec3<T>& operator op##= (Vec3<T>& lhs, const Vec3<U>& rhs) {                   \
        return vec3CompoundArithmetic(lhs, rhs, operatorLambda);                   \
    }

	/// Combining a units and vectors with unitless quantities
	DEFINE_VEC3_ARITHMETIC_OPERATOR(*, multiply)
	DEFINE_VEC3_ARITHMETIC_OPERATOR(/ , divide)

#undef DEFINE_VEC3_ARITHMETIC_OPERATOR
}

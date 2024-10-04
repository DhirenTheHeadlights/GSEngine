#pragma once

#include <initializer_list>
#include <glm/glm.hpp>
#include "Engine/Physics/Units/UnitTemplate.h"

namespace Engine {
	template <typename T>
	concept IsQuantity = requires {
		typename T::Units;
		std::derived_from<T, Quantity<typename T::Units>>;
	};

	template <typename T>
	concept IsUnitOrQuantity = IsUnit<T> || IsQuantity<T>;

	template <IsUnitOrQuantity T>
	struct Vec3 {
		Vec3() : vec(glm::vec3(0.0f)), magnitude(typename UnitToQuantity<T>::Type(0.0f)) {}

		// Constructor that initializes with a glm::vec3 and a unit
		template <IsUnit Unit>
		explicit Vec3(const glm::vec3& vec, const Unit& unit)
			: vec(glm::normalize(vec)), magnitude(typename UnitToQuantity<Unit>::Type(unit.getValue()* glm::length(vec))) {}

		// Constructor that initializes with 3 UnitType units
		template <IsUnit Unit>
		explicit Vec3(const Unit& x, const Unit& y, const Unit& z)
			: vec(glm::vec3(x.getValue(), y.getValue(), z.getValue())),
			magnitude(typename UnitToQuantity<Unit>::Type(glm::length(vec))) {}

		// Constructor that initializes with a glm::vec3 from an initializer list
		template <IsUnit Unit>
		explicit Vec3(const std::initializer_list<float>& list) {
			assert(list.size() == 3);
			vec = glm::vec3(*(list.begin()), *(list.begin() + 1), *(list.begin() + 2));
			magnitude = typename UnitToQuantity<Unit>::Type(glm::length(vec));
		}

		// Return the magnitude as the QuantityType
		auto length() const {
			return magnitude;
		}

		// Convert the vector to a different unit
		template <typename Unit>
		glm::vec3 as() const {
			static_assert(typename UnitToQuantity<Unit>::Type::template isValidUnit<Unit>(), "Invalid unit type for conversion");
			float convertedMagnitude = magnitude.template as<Unit>();
			return vec * convertedMagnitude;
		}

		glm::vec3 getDirection() const {
			return vec;
		}

		// Arithmetic operators (+, -, *, /)
		Vec3 operator+(const Vec3& other) const {
			glm::vec3 combinedVector = vec * magnitude.as() + other.vec * other.magnitude.as();
			return Vec3(combinedVector, magnitude);
		}

		Vec3 operator+(const glm::vec3& other) const {
			glm::vec3 combinedVector = vec * magnitude.as() + other;
			return Vec3(combinedVector, magnitude);
		}

		Vec3 operator+(const float scalar) const {
			return Vec3(vec * (magnitude.as() + scalar), magnitude);
		}

		Vec3 operator-(const Vec3& other) const {
			glm::vec3 combinedVector = vec * magnitude.as() - other.vec * other.magnitude.as();
			return Vec3(combinedVector, magnitude);
		}

		Vec3 operator*(const float scalar) const {
			return Vec3(vec * (magnitude.as() * scalar), magnitude);
		}

		template <typename ScalarUnit, typename ResultUnit>
		Vec3<ResultUnit> operator*(const ScalarUnit& scalar) const {
			return Vec3<ResultUnit>(vec * (magnitude.as() * scalar.getValue()));
		}

		Vec3 operator/(const float scalar) const {
			return Vec3(vec * (magnitude.as() / scalar), magnitude);
		}

		// Compound assignment operators (+=, -=, *=, /=)
		Vec3& operator+=(const Vec3& other) {
			*this = *this + other;
			return *this;
		}

		Vec3& operator+=(const glm::vec3& other) {
			*this = *this + other;
			return *this;
		}

		Vec3& operator+=(const float scalar) {
			magnitude = typename decltype(magnitude)::QuantityType(magnitude.as() + scalar);
			return *this;
		}

		Vec3& operator-=(const Vec3& other) {
			*this = *this - other;
			return *this;
		}

		Vec3& operator*=(const float scalar) {
			magnitude = typename decltype(magnitude)::QuantityType(magnitude.as() * scalar);
			return *this;
		}

		Vec3& operator/=(const float scalar) {
			magnitude = typename decltype(magnitude)::QuantityType(magnitude.as() / scalar);
			return *this;
		}

		/// Be careful - this function is not unit safe
		glm::vec3& getFullVector() {
			return vec;
		}

	private:
		glm::vec3 vec;  // Normalized direction vector
		typename UnitToQuantity<T>::Type magnitude;  // Scalar magnitude as either Quantity or deduced type from Unit
	};
}
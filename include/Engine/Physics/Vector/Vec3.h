#pragma once

#include <glm/glm.hpp>
#include "Engine/Physics/Units/UnitToQuantityDefinitions.h"

namespace Engine {
	// Trait to extract QuantityTagType
	template <typename T>
	struct GetQuantityTagType;

	template <typename T>
	struct GetQuantityTagType {
		using Type = typename UnitToQuantity<T>::Type;
	};

	// Concept to check if two types have the same QuantityTagType
	template <typename T, typename U>
	concept IsSameQuantityTag = std::is_same_v<typename GetQuantityTagType<T>::Type, typename GetQuantityTagType<U>::Type>;

	template <typename T>
	concept IsQuantity = std::is_base_of_v<Quantity<typename T::DefaultUnit, typename T::Units>, T>;

	template <typename T>
	concept IsQuantityOrUnit = IsQuantity<T> || IsUnit<T>;

	template <IsQuantityOrUnit T>
	struct Vec3 {
		using QuantityType = typename UnitToQuantity<T>::Type;

		Vec3() : magnitude(QuantityType()), vec(0.0f) {}

		// Constructor that only takes glm::vec3
		explicit Vec3(const glm::vec3& vector)
			: vec(glm::length(vector) > 0.0f ? normalize(vector) : glm::vec3(0.0f)) {
			float length = glm::length(vector);
			if constexpr (IsUnit<T>) {
				magnitude = QuantityType(T(length));
			}
			else if constexpr (IsQuantity<T>) {
				magnitude = T(T::DefaultUnit(length));
			}
		}

		explicit Vec3(const float x, const float y, const float z)
			: Vec3(glm::vec3(x, y, z)) {}

		explicit Vec3(const float xyz) : Vec3(glm::vec3(xyz)) {}

		template <IsUnit Unit>
		explicit Vec3(const Unit& x, const Unit& y, const Unit& z)
			: Vec3(glm::vec3(x.getValue(), y.getValue(), z.getValue())) {}

		template <IsUnit Unit>
		explicit Vec3(const Unit& xyz) : Vec3(glm::vec3(xyz.getValue())) {}

		template <IsQuantity Quantity>
		explicit Vec3(const Quantity& x, const Quantity& y, const Quantity& z)
			: Vec3(glm::vec3(x.template as<typename Quantity::DefaultUnit>(),
			                 y.template as<typename Quantity::DefaultUnit>(),
			                 z.template as<typename Quantity::DefaultUnit>())) {}

		template <IsQuantity Quantity>
		explicit Vec3(const Quantity& xyz) : Vec3(glm::vec3(xyz.template as<typename Quantity::DefaultUnit>())) {}

		// Converter from Vec3<Unit> to Vec3<Quantity>
		template <IsUnit Unit>
		Vec3(const Vec3<Unit>& other)
			: magnitude(other.magnitude), vec(other.getDirection()) {}

		// Converter from Vec3<Quantity> to Vec3<Unit>
		template <IsQuantity Quantity>
		Vec3(const Vec3<Quantity>& other)
			: magnitude(other.magnitude.template as<typename Quantity::DefaultUnit>()),
			  vec(other.getDirection()) {}

		// Magnitude
		// Call: magnitude.as<Units::[unit name]>()
		QuantityType magnitude;

		// Convert the vector to a different unit
		template <IsUnit Unit>
		[[nodiscard]] glm::vec3 as() const {
			const float convertedMagnitude = magnitude.template as<Unit>();
			return vec * convertedMagnitude;
		}

		[[nodiscard]] glm::vec3 getDirection() const {
			return vec;
		}

		/// Arithmetic operators

		template <typename U>
			requires IsSameQuantityTag<T, U>
		friend auto operator+(const Vec3& lhs, const Vec3<U>& rhs) {
			using ResultQuantity = decltype(lhs.magnitude + rhs.magnitude);
			glm::vec3 combinedVector = lhs.getDirection() * lhs.magnitude.template as<typename ResultQuantity::DefaultUnit>() +
				rhs.getDirection() * rhs.magnitude.template as<typename ResultQuantity::DefaultUnit>();
			return Vec3<ResultQuantity>(combinedVector);
		}

		template <typename U>
			requires IsSameQuantityTag<T, U>
		friend auto operator-(const Vec3& lhs, const Vec3<U>& rhs) {
			using ResultQuantity = decltype(lhs.magnitude - rhs.magnitude);
			glm::vec3 combinedVector = lhs.getDirection() * lhs.magnitude.template as<typename ResultQuantity::DefaultUnit>() -
				rhs.getDirection() * rhs.magnitude.template as<typename ResultQuantity::DefaultUnit>();
			return Vec3<ResultQuantity>(combinedVector);
		}

		friend Vec3 operator*(const Vec3& vec, const float scalar) {
			return Vec3(vec.getDirection() * scalar * vec.magnitude.value());
		}

		friend Vec3 operator/(const Vec3& vec, const float scalar) {
			return Vec3(vec.getDirection() * (vec.magnitude.value() / scalar));
		}

		/// Compound arithmetic operators

		template <IsQuantityOrUnit U>
			requires IsSameQuantityTag<T, U>
		Vec3& operator+=(const Vec3<U>& other) {
			*this = Vec3(*this + other);
			return *this;
		}

		template <IsQuantityOrUnit U>
			requires IsSameQuantityTag<T, U>
		Vec3& operator-=(const Vec3<U>& other) {
			*this = Vec3(*this - other);
			return *this;
		}

		Vec3& operator*=(const float scalar) {
			magnitude = magnitude * scalar;
			return *this;
		}

		Vec3& operator/=(const float scalar) {
			magnitude = magnitude / scalar;
			return *this;
		}

		/// Comparison Operators

		bool operator==(const Vec3& other) const {
			return magnitude == other.magnitude && vec == other.vec;
		}

		bool operator!=(const Vec3& other) const {
			return !(*this == other);
		}

		template <IsQuantityOrUnit U>
			requires IsSameQuantityTag<T, U>
		bool operator<(const Vec3<U>& other) const {
			return magnitude * vec < other.magnitude * other.vec;
		}

		template <IsQuantityOrUnit U>
			requires IsSameQuantityTag<T, U>
		bool operator<=(const Vec3<U>& other) const {
			return magnitude * vec <= other.magnitude * other.vec;
		}

		template <IsQuantityOrUnit U>
			requires IsSameQuantityTag<T, U>
		bool operator>(const Vec3<U>& other) const {
			return magnitude * vec > other.magnitude * other.vec;
		}

		template <IsQuantityOrUnit U>
			requires IsSameQuantityTag<T, U>
		bool operator>=(const Vec3<U>& other) const {
			return magnitude * vec >= other.magnitude * other.vec;
		}

		// Raw vector access
		// CAUTION: Not unit safe
		[[nodiscard]] glm::vec3& rawVec3() {
			return vec;
		}

	private:
		glm::vec3 vec;           // Normalized direction vector
	};
}



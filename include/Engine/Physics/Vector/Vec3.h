#include <glm/glm.hpp>
#include <initializer_list>
#include "Engine/Physics/Units/UnitTemplate.h"

namespace Engine {
	template <typename UnitType>
	struct Vec3 {
		Vec3() = default;

		// Constructor that initializes with a glm::vec3
		// Caution: This is not unit safe
		explicit Vec3(const glm::vec3& vec)
			: vec(normalize(vec)), magnitude(glm::length(vec)) {}

		// Constructor that initializes with 3 UnitType units
		template <IsUnit Unit>
		explicit Vec3(const Unit& x, const Unit& y, const Unit& z)
			: vec(glm::vec3(x.getValue(), y.getValue(), z.getValue())),
			magnitude(glm::length(vec)) {}

		// Constructor that initializes with an initializer list of units
		template <IsUnit Unit>
		explicit Vec3(std::initializer_list<Unit> list)
			: vec(glm::vec3(list.begin()[0].getValue(), list.begin()[1].getValue(), list.begin()[2].getValue())),
			magnitude(glm::length(vec)) {}

		// Return the magnitude as the UnitType (e.g., Meters, MetersPerSecond)
		UnitType length() const {
			return UnitType(magnitude);
		}

		// Convert the vector to a different unit type
		template <typename OtherUnitType>
		glm::vec3 as() const {
			return vec * magnitude * OtherUnitType(1.0f).getValue();
		}

		glm::vec3 getDirection() const {
			return vec;
		}

		// Arithmetic operators (+, -, *, /)
		Vec3 operator+(const Vec3& other) const {
			glm::vec3 combinedVector = vec * magnitude + other.vec * other.magnitude;
			return Vec3(combinedVector);
		}

		Vec3 operator+(const glm::vec3& other) const {
			glm::vec3 combinedVector = vec * magnitude + other;
			return Vec3(combinedVector);
		}

		Vec3 operator+(const float scalar) const {
			return Vec3(vec * (magnitude + scalar));
		}

		Vec3 operator-(const Vec3& other) const {
			glm::vec3 combinedVector = vec * magnitude - other.vec * other.magnitude;
			return Vec3(combinedVector);
		}

		Vec3 operator*(const float scalar) const {
			return Vec3(vec * (magnitude * scalar));
		}

		template <typename ScalarUnit, typename ResultUnit>
		Vec3<ResultUnit> operator*(const ScalarUnit& scalar) const {
			return Vec3<ResultUnit>(vec * (magnitude * scalar.getValue()));
		}

		Vec3 operator/(const float scalar) const {
			return Vec3(vec * (magnitude / scalar));
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
			magnitude += scalar;
			return *this;
		}

		Vec3& operator-=(const Vec3& other) {
			*this = *this - other;
			return *this;
		}

		Vec3& operator*=(const float scalar) {
			magnitude *= scalar;
			return *this;
		}

		Vec3& operator/=(const float scalar) {
			magnitude /= scalar;
			return *this;
		}

		/// Be careful - this function is not unit safe
		glm::vec3& getFullVector() {
			return vec;
		}
	private:
		glm::vec3 vec;  // Normalized direction vector
		float magnitude = 1.0f;  // Scalar magnitude
	};
}

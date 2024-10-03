#include <glm/glm.hpp>
#include <initializer_list>
#include "Engine/Physics/Units/UnitTemplate.h"

namespace Engine {
	template <typename T>
	concept IsUnit = requires(T t) {
		{ t.getValue() } -> std::convertible_to<float>;
	};

	template <typename Unit>
	requires IsUnit<Unit>
	struct Vec3Units {
		explicit Vec3Units(const glm::vec3& vec)
			: x(vec.x), y(vec.y), z(vec.z) {}
		Unit x;
		Unit y;
		Unit z;
	};

	template <typename T>
	concept IsQuantity = requires(T t) {
		{ t } -> std::is_same<Quantity>;
	};

	template <typename UnitType>
	struct Vec3 {
		Vec3() = default;

		// Constructor that initializes with a glm::vec3
		// Caution: This is not unit safe
		explicit Vec3(const glm::vec3& vec)
			: vec(normalize(vec)), magnitude(glm::length(vec)) {}

		// Constructor that initializes with 3 UnitType units
		explicit Vec3(const Vec3Units<UnitType>& units)
			: vec(glm::vec3(units.x.getValue(), units.y.getValue(), units.z.getValue())),
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

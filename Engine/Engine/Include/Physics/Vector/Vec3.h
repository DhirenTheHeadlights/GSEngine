#pragma once

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>
#include "Physics/Units/UnitToQuantityDefinitions.h"

namespace gse {
	template <typename T>
	concept is_quantity = std::derived_from<
		std::remove_cvref_t<T>,
		quantity<std::remove_cvref_t<T>, typename std::remove_cvref_t<T>::default_unit, typename std::remove_cvref_t<T>::units>
	>;

	template <typename unit_or_quantity_type>
	concept is_unit_or_quantity = is_quantity<unit_or_quantity_type> || is_unit<unit_or_quantity_type>;

	template <typename possibly_unitless_type>
	concept is_unitless = std::is_same_v<possibly_unitless_type, unitless>;

	template <typename T>
	struct get_quantity_tag_type;

	template <typename unit_or_quantity_type>
	struct get_quantity_tag_type {
		using type = typename unit_to_quantity<std::remove_cvref_t<unit_or_quantity_type>>::type;
	};

	template <typename unit_or_quantity_a, typename unit_or_quantity_b>
	concept has_same_quantity_tag = std::is_same_v<
		typename get_quantity_tag_type<std::remove_cvref_t<unit_or_quantity_a>>::type,
		typename get_quantity_tag_type<std::remove_cvref_t<unit_or_quantity_b>>::type>;

	template <typename unit_or_quantity_type, typename... arguments>
	concept are_valid_vector_args = (((is_quantity<arguments> && has_same_quantity_tag<arguments, unit_or_quantity_type>) || std::is_convertible_v<arguments, float>) && ...);

	template <is_unit_or_quantity unit_or_quantity_type>
	float convert_value_to_default_unit(const float value) {
		using quantity_type = typename unit_to_quantity<unit_or_quantity_type>::type;
		return quantity_type::template from<typename quantity_type::default_unit>(value).as_default_unit();
	}

	template <is_unit_or_quantity unit_or_quantity_type>
	auto convert_value_to_quantity(const float value) {
		using quantity_type = typename unit_to_quantity<unit_or_quantity_type>::type;
		return quantity_type::template from<typename quantity_type::default_unit>(value);
	}

	template <typename quantity_or_unit_type, typename argument_type>
	[[nodiscard]] static float get_value(const argument_type& argument) {
		if constexpr (is_quantity<argument_type>) {
			return argument.as_default_unit();
		}
		else if constexpr (std::is_convertible_v<argument_type, float>) {
			const float value = static_cast<float>(argument);
			if constexpr (is_unit<quantity_or_unit_type>) {
				return convert_value_to_default_unit<quantity_or_unit_type>(value);
			}

			return value; // Assume value is in default units
		}
		return 0.0f;
	}

	template <typename unit_or_quantity_type, typename... arguments>
	[[nodiscard]] static glm::vec3 create_vec(arguments&&... args) {
		if constexpr (sizeof...(arguments) == 0) {
			return glm::vec3(0.0f);
		}
		else if constexpr (sizeof...(arguments) == 1) {
			const float value = get_value<unit_or_quantity_type>(std::forward<arguments>(args)...);
			return glm::vec3(value);
		}
		else {
			return glm::vec3(get_value<unit_or_quantity_type>(std::forward<arguments>(args))...);
		}
	}

	template <is_unit_or_quantity unit_or_quantity_type = unitless>
	struct vec3 {
		template <typename... arguments>
			requires ((sizeof...(arguments) == 0 || sizeof...(arguments) == 1 || sizeof...(arguments) == 3) && are_valid_vector_args<unit_or_quantity_type, arguments...>)
		vec3(arguments&&... args) : m_vec(create_vec<unit_or_quantity_type>(std::forward<arguments>(args)...)) {}

		vec3(const glm::vec3& vec3) {
			if constexpr (is_unit<unit_or_quantity_type>) {
				m_vec = { convert_value_to_default_unit<unit_or_quantity_type>(vec3.x), convert_value_to_default_unit<unit_or_quantity_type>(vec3.y), convert_value_to_default_unit<unit_or_quantity_type>(vec3.z) };
			}
			else if constexpr (is_quantity<unit_or_quantity_type>) {
				m_vec = vec3; // Assume vec3 is in default units
			}
		}

		template <is_unit_or_quantity other_unit_or_quantity_type>
			requires has_same_quantity_tag<unit_or_quantity_type, other_unit_or_quantity_type>
		vec3(const vec3<other_unit_or_quantity_type>& other) : m_vec(other.as_default_units()) {}

		template <is_unit unit_type>
			requires has_same_quantity_tag<unit_or_quantity_type, unit_type>
		[[nodiscard]] glm::vec3 as() const {
			const float converted_magnitude = glm::length(m_vec) * unit_type::conversion_factor;
			if (constexpr auto zero = glm::vec3(0.0f); m_vec == zero) {
				return zero;
			}
			return normalize(m_vec) * converted_magnitude;
		}

		[[nodiscard]] glm::vec3& as_default_units() {
			return m_vec;
		}

		[[nodiscard]] const glm::vec3& as_default_units() const {
			return m_vec;
		}

	protected:
		glm::vec3 m_vec = glm::vec3(0.0f);
	};

	/// Unit arithmetic overloads

	template <is_unit_or_quantity unit_or_quantity_type_a, is_unit_or_quantity unit_or_quantity_type_b>
		requires has_same_quantity_tag<unit_or_quantity_type_a, unit_or_quantity_type_b>
	auto operator+(const vec3<unit_or_quantity_type_a>& lhs, const vec3<unit_or_quantity_type_b>& rhs) {
		return vec3<unit_or_quantity_type_a>(lhs.as_default_units() + rhs.as_default_units());
	}

	template <is_unit_or_quantity unit_or_quantity_type_a, is_unit_or_quantity unit_or_quantity_type_b>
		requires has_same_quantity_tag<unit_or_quantity_type_a, unit_or_quantity_type_b>
	auto operator-(const vec3<unit_or_quantity_type_a>& lhs, const vec3<unit_or_quantity_type_b>& rhs) {
		return vec3<unit_or_quantity_type_a>(lhs.as_default_units() - rhs.as_default_units());
	}

	template <is_unit_or_quantity unit_or_quantity_type>
	auto operator*(const vec3<unit_or_quantity_type>& lhs, const float scalar) {
		return vec3<unit_or_quantity_type>(lhs.as_default_units() * scalar);
	}

	template <is_unit_or_quantity unit_or_quantity_type>
	auto operator*(const float scalar, const vec3<unit_or_quantity_type>& rhs) {
		return vec3<unit_or_quantity_type>(scalar * rhs.as_default_units());
	}

	template <is_unit_or_quantity unit_or_quantity_type>
	auto operator/(const vec3<unit_or_quantity_type>& lhs, const float scalar) {
		return vec3<unit_or_quantity_type>(lhs.as_default_units() / scalar);
	}

	/// Compound arithmetic operators

	template <is_unit_or_quantity unit_or_quantity_type_a, is_unit_or_quantity unit_or_quantity_type_b>
		requires has_same_quantity_tag<unit_or_quantity_type_a, unit_or_quantity_type_b>
	auto& operator+=(vec3<unit_or_quantity_type_a>& lhs, const vec3<unit_or_quantity_type_b>& rhs) {
		lhs = lhs + rhs;
		return lhs;
	}

	template <is_unit_or_quantity unit_or_quantity_type_a, is_unit_or_quantity unit_or_quantity_type_b>
		requires has_same_quantity_tag<unit_or_quantity_type_a, unit_or_quantity_type_b>
	auto& operator-=(vec3<unit_or_quantity_type_a>& lhs, const vec3<unit_or_quantity_type_b>& rhs) {
		lhs = lhs - rhs;
		return lhs;
	}

	template <is_unit_or_quantity unit_or_quantity_type>
	auto& operator*=(vec3<unit_or_quantity_type>& lhs, const float scalar) {
		lhs = lhs * scalar;
		return lhs;
	}

	template <is_unit_or_quantity unit_or_quantity_type>
	auto& operator/=(vec3<unit_or_quantity_type>& lhs, const float scalar) {
		lhs = lhs / scalar;
		return lhs;
	}

	/// Casts

	template <is_unit_or_quantity T>
	auto& operator-(vec3<T>& lhs) {
		return lhs *= -1.0f;
	}

	/// Comparison Operators

	template <is_unit_or_quantity T>
	bool operator==(const vec3<T>& lhs, const vec3<T>& rhs) {
		return lhs.as_default_units() == rhs.as_default_units();
	}

	template <is_unit_or_quantity T>
	bool operator!=(const vec3<T>& lhs, const vec3<T>& rhs) {
		return !(lhs == rhs);
	}

	/// Unitless arithmetic overloads

	template <typename unit_or_quantity_a, typename unit_or_quantity_b>
	using non_unitless_type = std::conditional_t<
		(is_unitless<unit_or_quantity_a> && !is_unitless<unit_or_quantity_b>), unit_or_quantity_b,
		std::conditional_t<
		(!is_unitless<unit_or_quantity_a>&& is_unitless<unit_or_quantity_b>), unit_or_quantity_a,
		unitless // Both are unitless, return unitless
		>
	>;

	template <is_unit_or_quantity unit_or_quantity_type_a, is_unit_or_quantity unit_or_quantity_type_b>
		requires is_unitless<unit_or_quantity_type_a> || is_unitless<unit_or_quantity_type_b>
	auto operator*(const vec3<unit_or_quantity_type_a>& lhs, const vec3<unit_or_quantity_type_b>& rhs) {
		return vec3<non_unitless_type<unit_or_quantity_type_a, unit_or_quantity_type_b>>(lhs.as_default_units() * rhs.as_default_units());
	}

	template <is_unit_or_quantity unit_or_quantity_type_a, is_unit_or_quantity unit_or_quantity_type_b>
		requires is_unitless<unit_or_quantity_type_a> || is_unitless<unit_or_quantity_type_b>
	auto operator/(const vec3<unit_or_quantity_type_a>& lhs, const vec3<unit_or_quantity_type_b>& rhs) {
		return vec3<non_unitless_type<unit_or_quantity_type_a, unit_or_quantity_type_b>>(lhs.as_default_units() / rhs.as_default_units());
	}

	template <is_unit_or_quantity unit_or_quantity_vector, is_quantity quantity_type>
		requires is_unitless<unit_or_quantity_vector> || is_unitless<quantity_type>
	auto operator*(const vec3<unit_or_quantity_vector>& lhs, const quantity_type& rhs) {
		return vec3<non_unitless_type<unit_or_quantity_vector, quantity_type>>(lhs.as_default_units() * rhs.as_default_unit());
	}

	template <is_quantity quantity_type, is_unit_or_quantity unit_or_quantity_vector>
		requires is_unitless<quantity_type> || is_unitless<unit_or_quantity_vector>
	auto operator*(const quantity_type& lhs, const vec3<unit_or_quantity_vector>& rhs) {
		return vec3<non_unitless_type<quantity_type, unit_or_quantity_vector>>(lhs.as_default_unit() * rhs.as_default_units());
	}

	template <is_unit_or_quantity unit_or_quantity_type, is_quantity quantity_type>
		requires is_unitless<unit_or_quantity_type> || is_unitless<quantity_type>
	auto operator/(const vec3<unit_or_quantity_type>& lhs, const quantity_type& rhs) {
		return vec3<non_unitless_type<unit_or_quantity_type, quantity_type>>(lhs.as_default_units() / rhs.as_default_unit());
	}

	template <is_quantity quantity_type, is_unit_or_quantity unit_or_quantity_vector>
		requires is_unitless<quantity_type> || is_unitless<unit_or_quantity_vector>
	auto operator/(const quantity_type& lhs, const vec3<unit_or_quantity_vector> rhs) {
		return vec3<non_unitless_type<quantity_type, unit_or_quantity_vector>>(lhs.as_default_unit() / rhs.as_default_units());
	}

	template <is_quantity quantity_type_a, is_quantity quantity_type_b>
		requires is_unitless<quantity_type_a> || is_unitless<quantity_type_b>
	auto operator*(const quantity_type_a& lhs, const quantity_type_b& rhs) {
		return convert_value_to_quantity<non_unitless_type<quantity_type_a, quantity_type_b>>(lhs.as_default_unit() * rhs.as_default_unit());
	}

	template <is_quantity quantity_type_a, is_quantity quantity_type_b>
		requires is_unitless<quantity_type_a> || is_unitless<quantity_type_b>
	auto operator/(const quantity_type_a& lhs, const quantity_type_b& rhs) {
		return convert_value_to_quantity<non_unitless_type<quantity_type_a, quantity_type_b>>(lhs.as_default_unit() / rhs.as_default_unit());
	}
}

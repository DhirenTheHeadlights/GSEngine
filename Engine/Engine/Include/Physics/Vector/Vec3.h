#pragma once

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>
#include "Physics/Units/UnitToQuantityDefinitions.h"

namespace gse {
	template <typename T>
	concept is_quantity = std::is_base_of_v<
		quantity<std::remove_cvref_t<T>, typename std::remove_cvref_t<T>::default_unit, typename std::remove_cvref_t<T>::units>,
		std::remove_cvref_t<T>
	>;

	template <typename unit_or_quantity_type>
	concept is_unit_or_quantity = is_quantity<unit_or_quantity_type> || is_unit<unit_or_quantity_type>;

	template <typename T>
	struct get_quantity_tag_type;

	template <typename unit_or_quantity_type>
	struct get_quantity_tag_type {
		using type = typename unit_to_quantity<std::remove_cvref_t<unit_or_quantity_type>>::type;
	};

	template <typename unit_or_quantity_a, typename unit_or_quantity_b>
	concept has_same_quantity_tag = std::is_same_v<
		typename get_quantity_tag_type<std::remove_cvref_t<unit_or_quantity_a>>::type,
		typename get_quantity_tag_type<std::remove_cvref_t<unit_or_quantity_b>>::type
	>;

	template <typename unit_or_quantity_type, typename... arguments>
	concept are_valid_vector_args = (((is_quantity<arguments> && has_same_quantity_tag<arguments, unit_or_quantity_type>) || std::is_convertible_v<arguments, float>) && ...);

	template <typename possibly_unitless_type>
	concept is_unitless = std::is_same_v<possibly_unitless_type, struct unitless>;

	template <is_unit_or_quantity unit_or_quantity_type>
	float convert_value_to_default_unit(const float value) {
		using quantity_type = typename unit_to_quantity<unit_or_quantity_type>::type;
		return quantity_type::template from<typename quantity_type::default_unit>(value).as_default_units();
	}

	template <is_unit_or_quantity unit_or_quantity_type>
	typename unit_to_quantity<unit_or_quantity_type>::type convert_value_to_quantity(const float value) {
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

	template <is_unit_or_quantity unit_or_quantity_type = struct unitless>
	struct vec3 {
		using quantity_type = typename unit_to_quantity<unit_or_quantity_type>::type;

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

		template <is_unit unit_type>
			requires has_same_quantity_tag<unit_or_quantity_type, unit_type>
		[[nodiscard]] glm::vec3 as() const {
			const float converted_magnitude = glm::length(m_vec) * unit_type::converstion_factor;
			if (constexpr auto zero = glm::vec3(0.0f); m_vec == zero) {
				return zero;
			}
			return normalize(m_vec) * converted_magnitude;
		}

		// Converter between Vec3<Unit> and Vec3<Quantity>
		template <is_unit_or_quantity other_unit_or_quantity_type>
			requires has_same_quantity_tag<unit_or_quantity_type, other_unit_or_quantity_type>
		vec3(const vec3<other_unit_or_quantity_type>& other) : m_vec(other.as_default_units()) {}

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

	template <is_unit_or_quantity unit_or_quantity_type_a, is_unit_or_quantity U>
		requires has_same_quantity_tag<unit_or_quantity_type_a, U>
	auto operator-(const vec3<unit_or_quantity_type_a>& lhs, const vec3<U>& rhs) {
		return vec3<unit_or_quantity_type_a>(lhs.as_default_units() - rhs.as_default_units());
	}

	template <is_unit_or_quantity T>
	auto operator*(const vec3<T>& lhs, const float scalar) {
		return vec3<T>(lhs.as_default_units() * scalar);
	}

	template <is_unit_or_quantity T>
	auto operator*(const float scalar, const vec3<T>& rhs) {
		return vec3<T>(scalar * rhs.as_default_units());
	}

	template <is_unit_or_quantity T>
	auto operator/(const vec3<T>& lhs, const float scalar) {
		return vec3<T>(lhs.as_default_units() / scalar);
	}

	/// Compound arithmetic operators

	template <is_unit_or_quantity T, is_unit_or_quantity U>
		requires has_same_quantity_tag<T, U>
	auto& operator+=(vec3<T>& lhs, const vec3<U>& rhs) {
		lhs = lhs + rhs;
		return lhs;
	}

	template <is_unit_or_quantity T, is_unit_or_quantity U>
		requires has_same_quantity_tag<T, U>
	auto& operator-=(vec3<T>& lhs, const vec3<U>& rhs) {
		lhs = lhs - rhs;
		return lhs;
	}

	template <is_unit_or_quantity T>
	auto& operator*=(vec3<T>& lhs, const float scalar) {
		lhs = lhs * scalar;
		return lhs;
	}

	template <is_unit_or_quantity T>
	auto& operator/=(vec3<T>& lhs, const float scalar) {
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

	template <is_unit_or_quantity unit_or_quantity_type_a, is_unit_or_quantity unit_or_quantity_type_b>
		requires is_unitless<unit_or_quantity_type_a> || is_unitless<unit_or_quantity_type_b>
	auto operator*(const vec3<unit_or_quantity_type_a>& lhs, const vec3<unit_or_quantity_type_b>& rhs) {
		if constexpr (is_unitless<unit_or_quantity_type_a> && !is_unitless<unit_or_quantity_type_b>) {
			return vec3<unit_or_quantity_type_b>(lhs.as_default_units() * rhs.as_default_units());
		}
		else if constexpr (!is_unitless<unit_or_quantity_type_a> && is_unitless<unit_or_quantity_type_b>) {
			return vec3<unit_or_quantity_type_a>(lhs.as_default_units() * rhs.as_default_units());
		}
		else {
			return vec3(lhs.as_default_units() * rhs.as_default_units());
		}
	}

	template <is_unit_or_quantity unit_or_quantity_type_a, is_unit_or_quantity unit_or_quantity_type_b>
		requires is_unitless<unit_or_quantity_type_a> || is_unitless<unit_or_quantity_type_b>
	auto operator/(const vec3<unit_or_quantity_type_a>& lhs, const vec3<unit_or_quantity_type_b>& rhs) {
		if constexpr (is_unitless<unit_or_quantity_type_a> && !is_unitless<unit_or_quantity_type_b>) {
			return vec3<unit_or_quantity_type_b>(lhs.as_default_units() / rhs.as_default_units());
		}
		else if constexpr (!is_unitless<unit_or_quantity_type_a> && is_unitless<unit_or_quantity_type_b>) {
			return vec3<unit_or_quantity_type_a>(lhs.as_default_units() / rhs.as_default_units());
		}
		else {
			return vec3(lhs.as_default_units() / rhs.as_default_units());
		}
	}

	template <is_unit_or_quantity unit_or_quantity_vector, is_quantity quantity_type>
		requires is_unitless<unit_or_quantity_vector> || is_unitless<quantity_type>
	auto operator*(const vec3<unit_or_quantity_vector>& lhs, const quantity_type& rhs) {
		if constexpr (is_unitless<unit_or_quantity_vector> && !is_unitless<quantity_type>) {
			return vec3<quantity_type>(lhs.as_default_units() * rhs.as_default_unit());
		}
		else if constexpr (is_unitless<quantity_type> && !is_unitless<unit_or_quantity_vector>) {
			return vec3<unit_or_quantity_vector>(lhs.as_default_units() * rhs.as_default_unit());
		}
		else {
			return vec3(lhs.as_default_units() * get_value<struct unitless>(rhs));
		}
	}

	template <is_quantity quantity_type, is_unit_or_quantity unit_or_quantity_vector>
		requires is_unitless<quantity_type> || is_unitless<unit_or_quantity_vector>
	auto operator*(const quantity_type& lhs, const vec3<unit_or_quantity_vector>& rhs) {
		if constexpr (is_unitless<quantity_type> && !is_unitless<unit_or_quantity_vector>) {
			return vec3<unit_or_quantity_vector>(lhs.as_default_unit() * rhs.as_default_units());
		}
		else if constexpr (is_unitless<unit_or_quantity_vector> && !is_unitless<quantity_type>) {
			return vec3<quantity_type>(lhs.as_default_unit() * rhs.as_default_units());
		}
		else {
			return vec3(get_value<struct unitless>(lhs) * rhs.as_default_units());
		}
	}

	template <is_unit_or_quantity unit_or_quantity_type, is_quantity quantity_type>
		requires is_unitless<unit_or_quantity_type> || is_unitless<quantity_type>
	auto operator/(const vec3<unit_or_quantity_type>& lhs, const quantity_type& rhs) {
		if constexpr (is_unitless<unit_or_quantity_type> && !is_unitless<quantity_type>) {
			return vec3<quantity_type>(lhs.as_default_units() / rhs.as_default_unit());
		}
		else if constexpr (is_unitless<quantity_type> && !is_unitless<unit_or_quantity_type>) {
			return vec3<unit_or_quantity_type>(lhs.as_default_units() / rhs.as_default_unit());
		}
		else {
			return vec3(lhs.as_default_units() / rhs.as_default_unit());
		}
	}

	template <is_quantity quantity_type, is_unit_or_quantity unit_or_quantity_vector>
		requires is_unitless<quantity_type> || is_unitless<unit_or_quantity_vector>
	auto operator/(const quantity_type& lhs, const vec3<unit_or_quantity_vector> rhs) {
		if constexpr (is_unitless<quantity_type> && !is_unitless<unit_or_quantity_vector>) {
			return vec3<unit_or_quantity_vector>(lhs.asDefaultUnit() / rhs.as_default_units());
		}
		else if constexpr (is_unitless<unit_or_quantity_vector> && !is_unitless<quantity_type>) {
			return vec3<quantity_type>(lhs.as_default_unit() / rhs.as_default_units());
		}
		else {
			return vec3(get_value<struct unitless>(lhs) / rhs.as_default_units());
		}
	}

	template <is_quantity quantity_type_a, is_quantity quantity_type_b>
		requires is_unitless<quantity_type_a> || is_unitless<quantity_type_b>
	auto operator*(const quantity_type_a& lhs, const quantity_type_b& rhs) {
		if constexpr (is_unitless<quantity_type_a> && !is_unitless<quantity_type_b>) {
			return convert_value_to_quantity<quantity_type_b>(lhs.as_default_unit() * rhs.as_default_unit());
		}
		else if constexpr (is_unitless<quantity_type_b> && !is_unitless<quantity_type_a>) {
			return convert_value_to_quantity<quantity_type_a>(lhs.as_default_unit() * rhs.as_default_unit());
		}
		else {
			return unitless(lhs.as_default_unit() * rhs.as_default_unit());
		}
	}

	template <is_quantity quantity_type_a, is_quantity quantity_type_b>
		requires is_unitless<quantity_type_a> || is_unitless<quantity_type_b>
	auto operator/(const quantity_type_a& lhs, const quantity_type_b& rhs) {
		if constexpr (is_unitless<quantity_type_a> && !is_unitless<quantity_type_b>) {
			return convert_value_to_quantity<quantity_type_b>(lhs.as_default_unit() / rhs.as_default_unit());
		}
		else if constexpr (is_unitless<quantity_type_b> && !is_unitless<quantity_type_a>) {
			return convert_value_to_quantity<quantity_type_a>(lhs.as_default_unit() / rhs.as_default_unit());
		}
		else {
			return unitless(lhs.as_default_unit() / rhs.as_default_unit());
		}
	}
}

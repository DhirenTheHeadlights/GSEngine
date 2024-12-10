#pragma once
#include <concepts>
#include <sstream>
#include <type_traits>
#include <tuple>
#include <string>

namespace gse {
	template<typename... Units>
	struct unit_list {
		using type = std::tuple<Units...>;
	};

	template <typename quantity_tag_type, float conversion_factor_type, const char unit_name_type[]>
	struct unit {
		using quantity_tag = quantity_tag_type;
		static constexpr float conversion_factor = conversion_factor_type;
		static constexpr const char* unit_name = unit_name_type;
	};

	template <typename T>
	concept is_unit = requires {
		typename std::remove_cvref_t<T>::quantity_tag;
		{ std::remove_cvref_t<T>::unit_name } -> std::convertible_to<const char*>;
		{ std::remove_cvref_t<T>::conversion_factor } -> std::convertible_to<float>;
	};

	template <typename unit_type, typename valid_units>
	constexpr bool is_valid_unit_for_quantity() {
		return std::apply([]<typename... Units>(Units... u) {
			return ((std::is_same_v<typename unit_type::quantity_tag, typename Units::quantity_tag>) || ...);
		}, typename valid_units::type{});
	}

	template <typename derived_type, is_unit default_unit_type, typename valid_units_type>
	struct quantity {
		using units = valid_units_type;
		using default_unit = default_unit_type;

		quantity() = default;

		template <is_unit unit_type>
		void set(const float value) {
			static_assert(is_valid_unit_for_quantity<unit_type, valid_units_type>(), "Invalid unit type for assignment");
			m_val = get_converted_value<unit_type>(value);
		}

		// Convert to any other valid unit
		template <is_unit unit_type>
		float as() const {
			static_assert(is_valid_unit_for_quantity<unit_type, valid_units_type>(), "Invalid unit type for conversion");
			return m_val / unit_type::conversion_factor;
		}

		float asDefaultUnit() const {
			return m_val;
		}

		// Assignment operator overload
		template <is_unit unit_type>
		derived_type& operator=(const float value) {
			static_assert(is_valid_unit_for_quantity<unit_type, valid_units_type>(), "Invalid unit type for assignment");
			m_val = get_converted_value<unit_type>(value);
			return static_cast<derived_type&>(*this);
		}

		/// Arithmetic operators
		derived_type operator+(const derived_type& other) const {
			return derived_type(m_val + other.m_val);
		}

		derived_type operator-(const derived_type& other) const {
			return derived_type(m_val - other.m_val);
		}

		derived_type operator*(const float scalar) const {
			return derived_type(m_val * scalar);
		}

		derived_type operator/(const float scalar) const {
			return derived_type(m_val / scalar);
		}

		/// Compound assignment operators
		derived_type& operator+=(const derived_type& other) {
			m_val += other.m_val;
			return static_cast<derived_type&>(*this);
		}

		derived_type& operator-=(const derived_type& other) {
			m_val -= other.m_val;
			return static_cast<derived_type&>(*this);
		}

		derived_type& operator*=(const float scalar) {
			m_val *= scalar;
			return static_cast<derived_type&>(*this);
		}

		derived_type& operator/=(const float scalar) {
			m_val /= scalar;
			return static_cast<derived_type&>(*this);
		}

		// Comparison operators
		bool operator==(const derived_type& other) const {
			return m_val == other.m_val;
		}

		bool operator!=(const derived_type& other) const {
			return m_val != other.m_val;
		}

		bool operator<(const derived_type& other) const {
			return m_val < other.m_val;
		}

		bool operator>(const derived_type& other) const {
			return m_val > other.m_val;
		}

		bool operator<=(const derived_type& other) const {
			return m_val <= other.m_val;
		}

		bool operator>=(const derived_type& other) const {
			return m_val >= other.m_val;
		}

		// Other operators
		derived_type operator-() const {
			return derived_type(-m_val);
		}

		template <is_unit unit_type>
		static derived_type from(const float value) {
			static_assert(is_valid_unit_for_quantity<unit_type, valid_units_type>(), "Invalid unit type for conversion");
			derived_type result;
			result.m_val = result.template get_converted_value<unit_type>(value);
			return result;
		}
	protected:
		float m_val = 0.0f;  // Stored in base units

		template <is_unit T>
		float get_converted_value(const float value) {
			return value * T::conversion_factor / default_unit_type::conversion_factor;
		}

		explicit quantity(const float value) : m_val(value) {}
	};
}

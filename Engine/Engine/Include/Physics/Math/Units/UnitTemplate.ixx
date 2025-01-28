export module gse.physics.math.units.quantity;

import std;

export namespace gse {
	template<typename... Units>
	struct unit_list {
		using type = std::tuple<Units...>;
	};

	template <typename QuantityTagType, float ConversionFactorType, const char UnitNameType[]>
	struct unit {
		using quantity_tag = QuantityTagType;
		static constexpr float conversion_factor = ConversionFactorType;
		static constexpr const char* unit_name = UnitNameType;
	};

	template <typename T>
	concept is_unit = requires {
		typename std::remove_cvref_t<T>::quantity_tag;
		{ std::remove_cvref_t<T>::unit_name } -> std::convertible_to<const char*>;
		{ std::remove_cvref_t<T>::conversion_factor } -> std::convertible_to<float>;
	};

	template <typename UnitType, typename ValidUnits>
	constexpr bool is_valid_unit_for_quantity() {
		using tuple_type = typename ValidUnits::type;
		return std::apply(
			[]<typename... T0>(T0... units) {
				return ((std::is_same_v<typename UnitType::quantity_tag,
					typename T0::quantity_tag>) || ...);
			},
			tuple_type{} // default-constructed std::tuple<Units...>
		);
	}

	template <typename DerivedType, is_unit DefaultUnitType, typename ValidUnitsType>
	struct quantity {
		using units = ValidUnitsType;
		using default_unit = DefaultUnitType;

		quantity() = default;

		template <is_unit UnitType>
		auto set(const float value) -> void {
			static_assert(is_valid_unit_for_quantity<UnitType, ValidUnitsType>(), "Invalid unit type for assignment");
			m_val = get_converted_value<UnitType>(value);
		}

		// Convert to any other valid unit
		template <is_unit UnitType>
		auto as() const -> float {
			static_assert(is_valid_unit_for_quantity<UnitType, ValidUnitsType>(), "Invalid unit type for conversion");
			return m_val / UnitType::conversion_factor;
		}

		auto as_default_unit() const -> float {
			return m_val;
		}

		// Assignment operator overload
		template <is_unit UnitType>
		auto operator=(const float value) -> DerivedType& {
			static_assert(is_valid_unit_for_quantity<UnitType, ValidUnitsType>(), "Invalid unit type for assignment");
			m_val = get_converted_value<UnitType>(value);
			return static_cast<DerivedType&>(*this);
		}

		/// Arithmetic operators
		auto operator+(const DerivedType& other) const -> DerivedType {
			return DerivedType(m_val + other.m_val);
		}

		auto operator-(const DerivedType& other) const -> DerivedType {
			return DerivedType(m_val - other.m_val);
		}

		auto operator*(const float scalar) const -> DerivedType {
			return DerivedType(m_val * scalar);
		}

		auto operator/(const float scalar) const -> DerivedType {
			return DerivedType(m_val / scalar);
		}

		/// Compound assignment operators
		auto operator+=(const DerivedType& other) -> DerivedType& {
			m_val += other.m_val;
			return static_cast<DerivedType&>(*this);
		}

		auto operator-=(const DerivedType& other) -> DerivedType& {
			m_val -= other.m_val;
			return static_cast<DerivedType&>(*this);
		}

		auto operator*=(const float scalar) -> DerivedType& {
			m_val *= scalar;
			return static_cast<DerivedType&>(*this);
		}

		auto operator/=(const float scalar) -> DerivedType& {
			m_val /= scalar;
			return static_cast<DerivedType&>(*this);
		}

		// Comparison operators
		auto operator==(const DerivedType& other) const -> bool {
			return m_val == other.m_val;
		}

		auto operator!=(const DerivedType& other) const -> bool {
			return m_val != other.m_val;
		}

		auto operator<(const DerivedType& other) const -> bool {
			return m_val < other.m_val;
		}

		auto operator>(const DerivedType& other) const -> bool {
			return m_val > other.m_val;
		}

		auto operator<=(const DerivedType& other) const -> bool {
			return m_val <= other.m_val;
		}

		auto operator>=(const DerivedType& other) const -> bool {
			return m_val >= other.m_val;
		}

		// Other operators
		auto operator-() const -> DerivedType {
			return DerivedType(-m_val);
		}

		template <is_unit UnitType>
		static auto from(const float value) -> DerivedType {
			static_assert(is_valid_unit_for_quantity<UnitType, ValidUnitsType>(), "Invalid unit type for conversion");
			DerivedType result;
			result.m_val = result.template get_converted_value<UnitType>(value);
			return result;
		}
	protected:
		float m_val = 0.0f;  // Stored in base units

		template <is_unit UnitType>
		auto get_converted_value(const float value) -> float {
			return value * UnitType::conversion_factor / DefaultUnitType::conversion_factor;
		}

		explicit quantity(const float value) : m_val(value) {}

		friend DerivedType;
	};
}

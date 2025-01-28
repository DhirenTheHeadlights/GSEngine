module gse.physics.math.units.quant;

import std;

template <typename UnitType, typename ValidUnits>
constexpr auto gse::internal::is_valid_unit_for_quantity() -> bool {
    using tuple_type = typename ValidUnits::type;
    return std::apply(
        []<typename... T0>(T0... units) {
            return ((std::is_same_v<typename UnitType::quantity_tag,
                typename T0::quantity_tag>) || ...);
        },
        tuple_type{} // default-constructed std::tuple<Units...>
    );
}

template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
template <gse::internal::is_unit UnitType>
constexpr auto gse::internal::quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::set(ArithmeticType value) -> void {
    static_assert(internal::is_valid_unit_for_quantity<UnitType, ValidUnits>(), "Invalid unit type for assignment");
    m_val = this->template get_converted_value<UnitType>(value);
}

template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
template <gse::internal::is_unit UnitType>
constexpr auto gse::internal::quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::as() const -> float {
    static_assert(internal::is_valid_unit_for_quantity<UnitType, ValidUnits>(), "Invalid unit type for conversion");
    return m_val / UnitType::conversion_factor;
}

template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
constexpr auto gse::internal::quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::as_default_unit() const -> ArithmeticType {
    return m_val;
}

template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
constexpr auto gse::internal::quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator=(const quantity& other) -> quantity& {
    m_val = other.m_val;
    return *this;
}

template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
constexpr auto gse::internal::quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator+(const quantity& other) const -> quantity {
    return quantity(m_val + other.m_val);
}

template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
constexpr auto gse::internal::quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator-(const quantity& other) const -> quantity {
    return quantity(m_val - other.m_val);
}

template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
constexpr auto gse::internal::quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator*(ArithmeticType scalar) const -> quantity {
    return quantity(m_val * scalar);
}

template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
constexpr auto gse::internal::quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator/(ArithmeticType scalar) const -> quantity {
    return quantity(m_val / scalar);
}

template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
constexpr auto gse::internal::quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator+=(const quantity& other) -> quantity& {
    m_val += other.m_val;
    return *this;
}

template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
constexpr auto gse::internal::quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator-=(const quantity& other) -> quantity& {
    m_val -= other.m_val;
    return *this;
}

template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
constexpr auto gse::internal::quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator*=(ArithmeticType scalar) -> quantity& {
    m_val *= scalar;
    return *this;
}

template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
constexpr auto gse::internal::quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator/=(ArithmeticType scalar) -> quantity& {
    m_val /= scalar;
    return *this;
}

template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
constexpr auto gse::internal::quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator==(const quantity& other) const -> bool {
    return m_val == other.m_val;
}

template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
constexpr auto gse::internal::quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator!=(const quantity& other) const -> bool {
    return m_val != other.m_val;
}

template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
constexpr auto gse::internal::quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator<(const quantity& other) const -> bool {
    return m_val < other.m_val;
}

template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
constexpr auto gse::internal::quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator>(const quantity& other) const -> bool {
    return m_val > other.m_val;
}

template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
constexpr auto gse::internal::quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator<=(const quantity& other) const -> bool {
    return m_val <= other.m_val;
}

template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
constexpr auto gse::internal::quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator>=(const quantity& other) const -> bool {
    return m_val >= other.m_val;
}

template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
constexpr auto gse::internal::quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::operator-() const -> quantity {
    return quantity(-m_val);
}

template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
template <gse::internal::is_unit UnitType>
constexpr auto gse::internal::quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::from(ArithmeticType value) -> quantity {
    static_assert(internal::is_valid_unit_for_quantity<UnitType, ValidUnits>(), "Invalid unit type for conversion");
    quantity result;
    result.m_val = result.template get_converted_value<UnitType>(value);
    return result;
}

template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
template <gse::internal::is_unit UnitType>
constexpr auto gse::internal::quantity<ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::get_converted_value(float value) const -> float {
    return value * UnitType::conversion_factor;
}
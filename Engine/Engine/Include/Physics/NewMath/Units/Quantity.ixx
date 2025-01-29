export module gse.physics.math.units.quant;

import std;

namespace gse::internal {
    export template<typename... Units>
    struct unit_list {
        using type = std::tuple<Units...>;
    };

    export template <typename QuantityTagType, float ConversionFactorType, const char UnitNameType[]>
    struct unit {
        using quantity_tag = QuantityTagType;
        static constexpr float conversion_factor = ConversionFactorType;
        static constexpr const char* unit_name = UnitNameType;
    };

    export template <typename T>
    concept is_unit = requires {
        typename std::remove_cvref_t<T>::quantity_tag;
        { std::remove_cvref_t<T>::unit_name } -> std::convertible_to<const char*>;
        { std::remove_cvref_t<T>::conversion_factor } -> std::convertible_to<float>;
    };

    template <typename UnitType, typename ValidUnits>
    constexpr auto is_valid_unit_for_quantity() -> bool;

    export template <typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    class quantity {
    public:
        using value_type = ArithmeticType;
        using quantity_tag = QuantityTagType;
        using default_unit = DefaultUnitType;
        using units = ValidUnits;

        constexpr quantity() = default;
        constexpr quantity(ArithmeticType value) : m_val(value) {}

        template <is_unit UnitType>
		constexpr quantity(ArithmeticType value) : m_val(get_converted_value<UnitType>(value)) {}

        template <is_unit UnitType>
        constexpr auto set(ArithmeticType value) -> void;

        template <is_unit UnitType>
        constexpr auto as() const -> float;

        constexpr auto as_default_unit() const->ArithmeticType;

        constexpr auto operator=(const quantity& other) -> quantity&;
        constexpr auto operator+(const quantity& other) const->quantity;
        constexpr auto operator-(const quantity& other) const->quantity;
        constexpr auto operator*(ArithmeticType scalar) const->quantity;
        constexpr auto operator/(ArithmeticType scalar) const->quantity;

        constexpr auto operator+=(const quantity& other) -> quantity&;
        constexpr auto operator-=(const quantity& other) -> quantity&;
        constexpr auto operator*=(ArithmeticType scalar) -> quantity&;
        constexpr auto operator/=(ArithmeticType scalar) -> quantity&;

        constexpr auto operator==(const quantity& other) const -> bool;
        constexpr auto operator!=(const quantity& other) const -> bool;
        constexpr auto operator<(const quantity& other) const -> bool;
        constexpr auto operator>(const quantity& other) const -> bool;
        constexpr auto operator<=(const quantity& other) const -> bool;
        constexpr auto operator>=(const quantity& other) const -> bool;

        constexpr auto operator-() const->quantity;

        template <is_unit UnitType>
        constexpr static auto from(ArithmeticType value) -> quantity;
    private:
        template <is_unit UnitType>
        constexpr auto get_converted_value(float value) const -> float;

        ArithmeticType m_val = static_cast<ArithmeticType>(0);
    };
}

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

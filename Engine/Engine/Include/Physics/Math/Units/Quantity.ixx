export module gse.physics.math.units.quant;

import std;

namespace gse::vec {
	template <typename T, int N>
	struct storage;
}

export namespace gse::internal {
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
    constexpr auto is_valid_unit_for_quantity() -> bool;

    template <typename Derived, typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    class quantity {
    public:
		using derived = Derived;
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
        constexpr auto as() const -> ArithmeticType;

        constexpr auto as_default_unit() const -> ArithmeticType;
        constexpr auto as_default_unit() -> ArithmeticType&;

        template <is_unit UnitType>
        constexpr static auto from(ArithmeticType value) -> Derived;
    protected:
        template <is_unit UnitType>
        constexpr auto get_converted_value(ArithmeticType value) const -> ArithmeticType;

        ArithmeticType m_val = static_cast<ArithmeticType>(0);

		operator ArithmeticType() const {
			return m_val;
		}

    };

    template <typename Q>
	concept is_quantity = std::derived_from<
        std::remove_cvref_t<Q>, quantity<
        std::remove_cvref_t<Q>,
        typename std::remove_cvref_t<Q>::value_type,
        typename std::remove_cvref_t<Q>::quantity_tag,
        typename std::remove_cvref_t<Q>::default_unit,
        typename std::remove_cvref_t<Q>::units
        >
    >;

    template <is_quantity T> constexpr auto operator+(const T& lhs, const T& rhs) -> T;
    template <is_quantity T> constexpr auto operator-(const T& lhs, const T& rhs) -> T;
    template <is_quantity T, typename U> requires std::is_arithmetic_v<U> constexpr auto operator*(const T& lhs, const U& rhs) -> T;
    template <is_quantity T, typename U> requires std::is_arithmetic_v<U> constexpr auto operator*(const U& lhs, const T& rhs) -> T;
	template <is_quantity T, is_quantity U> constexpr auto operator*(const T& lhs, const U& rhs) -> typename T::value_type;
    template <is_quantity T, typename U> requires std::is_arithmetic_v<U> constexpr auto operator/(const T& lhs, const U& rhs) -> T;
    template <is_quantity T, typename U> requires std::is_arithmetic_v<U> constexpr auto operator/(const U& lhs, const T& rhs) -> T;
	template <is_quantity T, is_quantity U> constexpr auto operator/(const T& lhs, const U& rhs) -> typename T::value_type;

	template <is_quantity T> constexpr auto operator*(const T& lhs, const T& rhs) -> typename T::value_type;
	template <is_quantity T> constexpr auto operator/(const T& lhs, const T& rhs) -> typename T::value_type;

    template <is_quantity T> constexpr auto operator+=(T& lhs, const T& rhs) -> T&;
    template <is_quantity T> constexpr auto operator-=(T& lhs, const T& rhs) -> T&;
    template <is_quantity T, typename U> requires std::is_arithmetic_v<U> constexpr auto operator*=(T& lhs, const U& rhs) -> T&;
    template <is_quantity T, typename U> requires std::is_arithmetic_v<U> constexpr auto operator/=(T& lhs, const U& rhs) -> T&;

    template <is_quantity T> constexpr auto operator==(const T& lhs, const T& rhs) -> bool;
    template <is_quantity T> constexpr auto operator!=(const T& lhs, const T& rhs) -> bool;
	template <is_quantity T> constexpr auto operator<(const T& lhs, const T& rhs) -> bool;
	template <is_quantity T> constexpr auto operator>(const T& lhs, const T& rhs) -> bool;
	template <is_quantity T> constexpr auto operator<=(const T& lhs, const T& rhs) -> bool;
	template <is_quantity T> constexpr auto operator>=(const T& lhs, const T& rhs) -> bool;

    template <is_quantity T> constexpr auto operator-(const T& value) -> T;
}

template <typename UnitType, typename ValidUnits>
constexpr auto gse::internal::is_valid_unit_for_quantity() -> bool {
    using tuple_type = typename ValidUnits::type;
    return std::apply(
        []<typename... T0>(T0... units) constexpr {
        return ((std::is_same_v<typename UnitType::quantity_tag,
            typename T0::quantity_tag>) || ...);
    },
        tuple_type{} // default-constructed std::tuple<Units...>
    );
}

template <typename Derived, typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
template <gse::internal::is_unit UnitType>
constexpr auto gse::internal::quantity<Derived, ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::set(ArithmeticType value) -> void {
    static_assert(internal::is_valid_unit_for_quantity<UnitType, ValidUnits>(), "Invalid unit type for assignment");
    m_val = this->get_converted_value<UnitType>(value);
}

template <typename Derived, typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
template <gse::internal::is_unit UnitType>
constexpr auto gse::internal::quantity<Derived, ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::as() const -> ArithmeticType {
    static_assert(internal::is_valid_unit_for_quantity<UnitType, ValidUnits>(), "Invalid unit type for conversion");
    return m_val / UnitType::conversion_factor;
}

template <typename Derived, typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
constexpr auto gse::internal::quantity<Derived, ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::as_default_unit() const -> ArithmeticType {
    return m_val;
}

template <typename Derived, typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
	requires std::is_arithmetic_v<ArithmeticType>
constexpr auto gse::internal::quantity<Derived, ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::as_default_unit() -> ArithmeticType& {
    return m_val;
}

template <typename Derived, typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
template <gse::internal::is_unit UnitType>
constexpr auto gse::internal::quantity<Derived, ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::from(const ArithmeticType value) -> Derived {
    static_assert(internal::is_valid_unit_for_quantity<UnitType, ValidUnits>(), "Invalid unit type for conversion");
    Derived result;
    result.m_val = result.template get_converted_value<UnitType>(value);
    return result;
}

template <typename Derived, typename ArithmeticType, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
template <gse::internal::is_unit UnitType>
constexpr auto gse::internal::quantity<Derived, ArithmeticType, QuantityTagType, DefaultUnitType, ValidUnits>::get_converted_value(ArithmeticType value) const -> ArithmeticType {
    return value * UnitType::conversion_factor;
}

template <gse::internal::is_quantity T>
constexpr auto gse::internal::operator+(const T& lhs, const T& rhs) -> T {
	T result(lhs.as_default_unit() + rhs.as_default_unit());
	return result;
}

template <gse::internal::is_quantity T>
constexpr auto gse::internal::operator-(const T& lhs, const T& rhs) -> T {
	T result(lhs.as_default_unit() - rhs.as_default_unit());
	return result;
}

template <gse::internal::is_quantity T, typename U> requires std::is_arithmetic_v<U>
constexpr auto gse::internal::operator*(const T& lhs, const U& rhs) -> T {
	T result(lhs.as_default_unit() * rhs);
	return result;
}

template <gse::internal::is_quantity T, typename U> requires std::is_arithmetic_v<U>
constexpr auto gse::internal::operator*(const U& lhs, const T& rhs) -> T {
	T result(lhs * rhs.as_default_unit());
	return result;
}

template <gse::internal::is_quantity T, gse::internal::is_quantity U>
constexpr auto gse::internal::operator*(const T& lhs, const U& rhs) -> typename T::value_type {
	return lhs.as_default_unit() * rhs.as_default_unit();
}

template <gse::internal::is_quantity T, typename U> requires std::is_arithmetic_v<U>
constexpr auto gse::internal::operator/(const T& lhs, const U& rhs) -> T {
	T result(lhs.as_default_unit() / rhs);
	return result;
}

template <gse::internal::is_quantity T, typename U> requires std::is_arithmetic_v<U>
constexpr auto gse::internal::operator/(const U& lhs, const T& rhs) -> T {
	T result(lhs / rhs.as_default_unit());
	return result;
}

template <gse::internal::is_quantity T, gse::internal::is_quantity U>
constexpr auto gse::internal::operator/(const T& lhs, const U& rhs) -> typename T::value_type {
	return lhs.as_default_unit() / rhs.as_default_unit();
}

template <gse::internal::is_quantity T>
constexpr auto gse::internal::operator*(const T& lhs, const T& rhs) -> typename T::value_type {
	return lhs.as_default_unit() * rhs.as_default_unit();
}

template <gse::internal::is_quantity T>
constexpr auto gse::internal::operator/(const T& lhs, const T& rhs) -> typename T::value_type {
	return lhs.as_default_unit() / rhs.as_default_unit();
}

template <gse::internal::is_quantity T>
constexpr auto gse::internal::operator+=(T& lhs, const T& rhs) -> T& {
	lhs.set<T::default_unit>(lhs.as_default_unit() + rhs.as_default_unit());
	return lhs;
}

template <gse::internal::is_quantity T>
constexpr auto gse::internal::operator-=(T& lhs, const T& rhs) -> T& {
	lhs.set<T::default_unit>(lhs.as_default_unit() - rhs.as_default_unit());
	return lhs;
}

template <gse::internal::is_quantity T, typename U> requires std::is_arithmetic_v<U>
constexpr auto gse::internal::operator*=(T& lhs, const U& rhs) -> T& {
	lhs.set<T::default_unit>(lhs.as_default_unit() * rhs);
	return lhs;
}

template <gse::internal::is_quantity T, typename U> requires std::is_arithmetic_v<U>
constexpr auto gse::internal::operator/=(T& lhs, const U& rhs) -> T& {
	lhs.set<T::default_unit>(lhs.as_default_unit() / rhs);
	return lhs;
}

template <gse::internal::is_quantity T>
constexpr auto gse::internal::operator==(const T& lhs, const T& rhs) -> bool {
	return lhs.as_default_unit() == rhs.as_default_unit();
}

template <gse::internal::is_quantity T>
constexpr auto gse::internal::operator!=(const T& lhs, const T& rhs) -> bool {
	return lhs.as_default_unit() != rhs.as_default_unit();
}

template <gse::internal::is_quantity T>
constexpr auto gse::internal::operator<(const T& lhs, const T& rhs) -> bool {
	return lhs.as_default_unit() < rhs.as_default_unit();
}

template <gse::internal::is_quantity T>
constexpr auto gse::internal::operator>(const T& lhs, const T& rhs) -> bool {
	return lhs.as_default_unit() > rhs.as_default_unit();
}

template <gse::internal::is_quantity T>
constexpr auto gse::internal::operator<=(const T& lhs, const T& rhs) -> bool {
	return lhs.as_default_unit() <= rhs.as_default_unit();
}

template <gse::internal::is_quantity T>
constexpr auto gse::internal::operator>=(const T& lhs, const T& rhs) -> bool {
	return lhs.as_default_unit() >= rhs.as_default_unit();
}

template <gse::internal::is_quantity T>
constexpr auto gse::internal::operator-(const T& value) -> T {
	T result(-value.as_default_unit());
	return result;
}


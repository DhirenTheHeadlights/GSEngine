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

    template <typename Derived, typename ArithmeticType, typename Dimensions, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
        requires std::is_arithmetic_v<ArithmeticType>
    class quantity {
    public:
        using derived = Derived;
        using value_type = ArithmeticType;
        using dimension = Dimensions;
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
        constexpr auto as() const->ArithmeticType;

        constexpr auto as_default_unit() const->ArithmeticType;
        constexpr auto as_default_unit() -> ArithmeticType&;

        template <is_unit UnitType>
        constexpr static auto from(ArithmeticType value) -> Derived;
    protected:
        template <is_unit UnitType>
        constexpr auto get_converted_value(ArithmeticType value) const->ArithmeticType;

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
        typename std::remove_cvref_t<Q>::dimension,
        typename std::remove_cvref_t<Q>::quantity_tag,
        typename std::remove_cvref_t<Q>::default_unit,
        typename std::remove_cvref_t<Q>::units
        >
    >;
}


export namespace gse::units {
    struct energy_tag {};
    struct power_tag {};
    struct velocity_tag {};
    struct acceleration_tag {};
    struct time_tag {};
    struct angle_tag {};
    struct mass_tag {};
    struct force_tag {};
    struct length_tag {};
}

export namespace gse::units::dimensions {

    struct derived_quantity_generic_tag {};

    export template <int L, int T, int M>
    struct dim {
        static constexpr int length = L;
        static constexpr int time = T;
        static constexpr int mass = M;
    };

    template<typename Dim>
	struct quantity_tag_from_dimensions {
		using type = derived_quantity_generic_tag;
	};

    template<>
    struct quantity_tag_from_dimensions<dim<2, -3, 1>> {
        using type = power_tag;
    };
    template<>
    struct quantity_tag_from_dimensions<dim<1, -1, 0>> {
        using type = velocity_tag;
    };

    template<>
    struct quantity_tag_from_dimensions<dim<1, -2, 0>> {
        using type = acceleration_tag;
    };

    template<>
    struct quantity_tag_from_dimensions<dim<0, 1, 0>> {
        using type = time_tag;
    };

    template<>
    struct quantity_tag_from_dimensions<dim<0, 0, 1>> {
        using type = mass_tag;
    };

    template<>
    struct quantity_tag_from_dimensions<dim<1, -2, 1>> {
        using type = force_tag;
    };

    template<>
    struct quantity_tag_from_dimensions<dim<1, 0, 0>> {
        using type = length_tag;
    };

    template <typename D1, typename D2>
    struct dim_multiply {
        using type = dim<
            D1::length + D2::length,
            D1::time + D2::time,
            D1::mass + D2::mass
        >;
    };

    template <typename D1, typename D2>
    struct dim_divide {
        using type = dim<
            D1::length - D2::length,
            D1::time - D2::time,
            D1::mass - D2::mass
        >;
    };

    template <typename Q1, typename Q2> 
        requires gse::internal::is_quantity<Q1>&& gse::internal::is_quantity<Q2>
    struct quantity_multiply_result {
       
        using value_type = typename Q1::value_type;
        using dimension = typename dim_multiply<
            typename Q1::dimension,
            typename Q2::dimension
        >::type;

        using type = gse::internal::quantity< void, value_type, dimension, quantity_tag_from_dimensions<dimension>, void, void>;
    };

    template <typename Q1, typename Q2>
        requires gse::internal::is_quantity<Q1> && gse::internal::is_quantity<Q2>
    struct quantity_divide_result {
        using value_type = typename Q1::value_type;

        using dimension = typename dim_divide<
            typename Q1::dimension,
            typename Q2::dimension
        >::type;

        using tag = typename quantity_tag_from_dimensions<dimension>::type;

        using type = gse::internal::quantity<void, value_type, dimension, quantity_tag_from_dimensions<dimension>, void, void>;
    };
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

export namespace gse::internal {
    template <typename T> requires is_quantity<T> constexpr auto operator+(const T& lhs, const T& rhs) -> T;
    template <typename T> requires is_quantity<T> constexpr auto operator-(const T& lhs, const T& rhs) -> T;
    template <typename T, typename U> requires is_quantity<T> && std::is_arithmetic_v<U> constexpr auto operator*(const T& lhs, const U& rhs) -> T;
    template <typename T, typename U> requires is_quantity<T> && std::is_arithmetic_v<U> constexpr auto operator*(const U& lhs, const T& rhs) -> T;
    template <typename T, typename U> requires is_quantity<T> && is_quantity<U> constexpr auto operator*(const T& lhs, const U& rhs) -> typename units::dimensions::quantity_multiply_result<T, U>::type;
    template <typename T, typename U> requires is_quantity<T> && std::is_arithmetic_v<U> constexpr auto operator/(const T& lhs, const U& rhs) -> T;
    template <typename T, typename U> requires is_quantity<T> && std::is_arithmetic_v<U> constexpr auto operator/(const U& lhs, const T& rhs) -> T;
	template <typename T, typename U> requires is_quantity<T> && is_quantity<U> constexpr auto operator/(const T& lhs, const U& rhs) -> typename units::dimensions::quantity_divide_result<T, U>::type;
    
	template <typename T> requires is_quantity<T> constexpr auto operator*(const T& lhs, const T& rhs) -> typename units::dimensions::quantity_multiply_result<T, T>::type;
	template <typename T> requires is_quantity<T> constexpr auto operator/(const T& lhs, const T& rhs) -> typename units::dimensions::quantity_divide_result<T, T>::type;

    template <typename T> requires is_quantity<T> constexpr auto operator+=(T& lhs, const T& rhs) -> T&;
    template <typename T> requires is_quantity<T> constexpr auto operator-=(T& lhs, const T& rhs) -> T&;
    template <typename T, typename U> requires is_quantity<T> && std::is_arithmetic_v<U> constexpr auto operator*=(T& lhs, const U& rhs) -> T&;
    template <typename T, typename U> requires is_quantity<T> && std::is_arithmetic_v<U> constexpr auto operator/=(T& lhs, const U& rhs) -> T&;

    template <typename T> requires is_quantity<T> constexpr auto operator==(const T& lhs, const T& rhs) -> bool;
    template <typename T> requires is_quantity<T> constexpr auto operator!=(const T& lhs, const T& rhs) -> bool;
	template <typename T> requires is_quantity<T> constexpr auto operator<(const T& lhs, const T& rhs) -> bool;
	template <typename T> requires is_quantity<T> constexpr auto operator>(const T& lhs, const T& rhs) -> bool;
	template <typename T> requires is_quantity<T> constexpr auto operator<=(const T& lhs, const T& rhs) -> bool;
	template <typename T> requires is_quantity<T> constexpr auto operator>=(const T& lhs, const T& rhs) -> bool;

    template <typename T> requires is_quantity<T> constexpr auto operator-(const T& value) -> T;
}

template <typename Derived, typename ArithmeticType, typename Dimensions, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
template <gse::internal::is_unit UnitType>
constexpr auto gse::internal::quantity<Derived, ArithmeticType, Dimensions, QuantityTagType, DefaultUnitType, ValidUnits>::set(ArithmeticType value) -> void {
    static_assert(internal::is_valid_unit_for_quantity<UnitType, ValidUnits>(), "Invalid unit type for assignment");
    m_val = this->get_converted_value<UnitType>(value);
}

template <typename Derived, typename ArithmeticType, typename Dimensions, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
template <gse::internal::is_unit UnitType>
constexpr auto gse::internal::quantity<Derived, ArithmeticType, Dimensions, QuantityTagType, DefaultUnitType, ValidUnits>::as() const -> ArithmeticType {
    static_assert(internal::is_valid_unit_for_quantity<UnitType, ValidUnits>(), "Invalid unit type for conversion");
    return m_val / UnitType::conversion_factor;
}

template <typename Derived, typename ArithmeticType, typename Dimensions, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
constexpr auto gse::internal::quantity<Derived, ArithmeticType, Dimensions, QuantityTagType, DefaultUnitType, ValidUnits>::as_default_unit() const -> ArithmeticType {
    return m_val;
}

template <typename Derived, typename ArithmeticType, typename Dimensions, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
	requires std::is_arithmetic_v<ArithmeticType>
constexpr auto gse::internal::quantity<Derived, ArithmeticType, Dimensions, QuantityTagType, DefaultUnitType, ValidUnits>::as_default_unit() -> ArithmeticType& {
    return m_val;
}

template <typename Derived, typename ArithmeticType, typename Dimensions, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
template <gse::internal::is_unit UnitType>
constexpr auto gse::internal::quantity<Derived, ArithmeticType, Dimensions, QuantityTagType, DefaultUnitType, ValidUnits>::from(const ArithmeticType value) -> Derived {
    static_assert(internal::is_valid_unit_for_quantity<UnitType, ValidUnits>(), "Invalid unit type for conversion");
    Derived result;
    result.m_val = result.template get_converted_value<UnitType>(value);
    return result;
}

template <typename Derived, typename ArithmeticType, typename Dimensions, typename QuantityTagType, typename DefaultUnitType, typename ValidUnits>
    requires std::is_arithmetic_v<ArithmeticType>
template <gse::internal::is_unit UnitType>
constexpr auto gse::internal::quantity<Derived, ArithmeticType, Dimensions, QuantityTagType, DefaultUnitType, ValidUnits>::get_converted_value(ArithmeticType value) const -> ArithmeticType {
    return value * UnitType::conversion_factor;
}

template <typename T>
    requires gse::internal::is_quantity<T>
constexpr auto gse::internal::operator+(const T& lhs, const T& rhs) -> T {
	T result(lhs.as_default_unit() + rhs.as_default_unit());
	return result;
}

template <typename T>
    requires gse::internal::is_quantity<T>
constexpr auto gse::internal::operator-(const T& lhs, const T& rhs) -> T {
	T result(lhs.as_default_unit() - rhs.as_default_unit());
	return result;
}

template <typename T, typename U> requires gse::internal::is_quantity<T> && std::is_arithmetic_v<U>
constexpr auto gse::internal::operator*(const T& lhs, const U& rhs) -> T {
	T result(lhs.as_default_unit() * rhs);
	return result;
}

template <typename T, typename U> requires gse::internal::is_quantity<T> && std::is_arithmetic_v<U>
constexpr auto gse::internal::operator*(const U& lhs, const T& rhs) -> T {
	T result(lhs * rhs.as_default_unit());
	return result;
}

template <typename T, typename U> requires gse::internal::is_quantity<T> && gse::internal::is_quantity<U>
constexpr auto gse::internal::operator*(const T& lhs, const U& rhs) -> typename gse::units::dimensions::quantity_multiply_result<T, U>::type {
    using result_t = typename gse::units::dimensions::quantity_multiply_result<T, U>::type;
    return result_t(lhs.as_default_unit() * rhs.as_default_unit());
}

template <typename T, typename U> requires gse::internal::is_quantity<T> && std::is_arithmetic_v<U>
constexpr auto gse::internal::operator/(const T& lhs, const U& rhs) -> T {
	T result(lhs.as_default_unit() / rhs);
	return result;
}

template <typename T, typename U> requires gse::internal::is_quantity<T> && std::is_arithmetic_v<U>
constexpr auto gse::internal::operator/(const U& lhs, const T& rhs) -> T {
	T result(lhs / rhs.as_default_unit());
	return result;
}

template <typename T, typename U> requires gse::internal::is_quantity<T> && gse::internal::is_quantity<U>
constexpr auto gse::internal::operator/(const T& lhs, const U& rhs) -> typename gse::units::dimensions::quantity_divide_result<T, U>::type {
    using result_t = typename gse::units::dimensions::quantity_divide_result<T, U>::type;
    return result_t(lhs.as_default_unit() / rhs.as_default_unit());
}

template <typename T> requires gse::internal::is_quantity<T>
constexpr auto gse::internal::operator*(const T& lhs, const T& rhs) -> typename gse::units::dimensions::quantity_multiply_result<T, T>::type {
    using result_t = typename gse::units::dimensions::quantity_multiply_result<T, T>::type;
    return result_t(lhs.as_default_unit() * rhs.as_default_unit());
}

template <typename T> requires gse::internal::is_quantity<T>
constexpr auto gse::internal::operator/(const T& lhs, const T& rhs) -> typename gse::units::dimensions::quantity_divide_result<T, T>::type {
    using result_t = typename gse::units::dimensions::quantity_divide_result<T, T>::type;
    return result_t(lhs.as_default_unit() / rhs.as_default_unit());
}

template <typename T> requires gse::internal::is_quantity<T>
constexpr auto gse::internal::operator+=(T& lhs, const T& rhs) -> T& {
	lhs.set<T::default_unit>(lhs.as_default_unit() + rhs.as_default_unit());
	return lhs;
}

template <typename T> requires gse::internal::is_quantity<T>
constexpr auto gse::internal::operator-=(T& lhs, const T& rhs) -> T& {
	lhs.set<T::default_unit>(lhs.as_default_unit() - rhs.as_default_unit());
	return lhs;
}

template <typename T, typename U> requires gse::internal::is_quantity<T> && std::is_arithmetic_v<U>
constexpr auto gse::internal::operator*=(T& lhs, const U& rhs) -> T& {
	lhs.set<T::default_unit>(lhs.as_default_unit() * rhs);
	return lhs;
}

template <typename T, typename U> requires gse::internal::is_quantity<T> && std::is_arithmetic_v<U>
constexpr auto gse::internal::operator/=(T& lhs, const U& rhs) -> T& {
	lhs.set<T::default_unit>(lhs.as_default_unit() / rhs);
	return lhs;
}

template <typename T> requires gse::internal::is_quantity<T>
constexpr auto gse::internal::operator==(const T& lhs, const T& rhs) -> bool {
	return lhs.as_default_unit() == rhs.as_default_unit();
}

template <typename T> requires gse::internal::is_quantity<T>
constexpr auto gse::internal::operator!=(const T& lhs, const T& rhs) -> bool {
	return lhs.as_default_unit() != rhs.as_default_unit();
}

template <typename T> requires gse::internal::is_quantity<T>
constexpr auto gse::internal::operator<(const T& lhs, const T& rhs) -> bool {
	return lhs.as_default_unit() < rhs.as_default_unit();
}

template <typename T> requires gse::internal::is_quantity<T>
constexpr auto gse::internal::operator>(const T& lhs, const T& rhs) -> bool {
	return lhs.as_default_unit() > rhs.as_default_unit();
}

template <typename T> requires gse::internal::is_quantity<T>
constexpr auto gse::internal::operator<=(const T& lhs, const T& rhs) -> bool {
	return lhs.as_default_unit() <= rhs.as_default_unit();
}

template <typename T> requires gse::internal::is_quantity<T>
constexpr auto gse::internal::operator>=(const T& lhs, const T& rhs) -> bool {
	return lhs.as_default_unit() >= rhs.as_default_unit();
}

template <typename T> requires gse::internal::is_quantity<T>
constexpr auto gse::internal::operator-(const T& value) -> T {
	T result(-value.as_default_unit());
	return result;
}


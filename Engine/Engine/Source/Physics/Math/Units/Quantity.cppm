
export module gse.physics.math:quant;

import std;

import gse.assert;

import :dimension;

namespace gse::internal {
    template <typename Tag>
    struct quantity_traits;

    template <size_t N>
    struct fixed_string {
        char data[N]{};
        constexpr fixed_string(const char(&str)[N]) {
            std::copy_n(str, N, data);
        }
        constexpr operator std::string_view() const {
            return { data, N - 1 };
        }
    };

    export template <typename T>
    concept is_arithmetic = std::integral<T> || std::floating_point<T>;

    constexpr auto cexpr_llround(const long double x) -> long long {
        return x >= 0 ? static_cast<long long>(x + 0.5L) : static_cast<long long>(x - 0.5L);
    }
}

namespace gse::internal {
    export template <typename T>
    concept is_ratio = requires {
        { T::num } -> std::convertible_to<std::intmax_t>;
        { T::den } -> std::convertible_to<std::intmax_t>;
    };

    export template <typename QuantityTagType, is_ratio ConversionRatio, fixed_string UnitName>
    struct unit {
        using quantity_tag = QuantityTagType;
        using conversion_ratio = ConversionRatio;
        static constexpr auto unit_name = UnitName;

        template <typename T>
        constexpr auto operator()(T value) const noexcept;
    };
}

template <typename QuantityTagType, gse::internal::is_ratio ConversionRatio, gse::internal::fixed_string UnitName>
template <typename T>
constexpr auto gse::internal::unit<QuantityTagType, ConversionRatio, UnitName>::operator()(T value) const noexcept {
    using quantity_template = quantity_traits<QuantityTagType>;
    return typename quantity_template::template type<T>::template from<unit>(value);
}

namespace gse::internal {
    export template <typename T>
    concept is_unit = requires {
        typename std::remove_cvref_t<T>::quantity_tag;
        { std::remove_cvref_t<T>::unit_name } -> std::convertible_to<std::string_view>;
        typename std::remove_cvref_t<T>::conversion_ratio;
        requires is_ratio<typename std::remove_cvref_t<T>::conversion_ratio>;
    };

    struct generic_quantity_tag {};
    using no_default_unit = unit<generic_quantity_tag, std::ratio<1>, "no_default_unit">;

    template <typename UnitType, typename QuantityType>
	concept valid_unit_for_quantity = std::same_as<typename UnitType::quantity_tag, typename QuantityType::quantity_tag> || std::same_as<typename QuantityType::quantity_tag, generic_quantity_tag>;

    export template <
        is_arithmetic ArithmeticType,
        is_dimension Dimensions,
        typename QuantityTagType,
        typename DefaultUnitType
    >
    class quantity {
    public:
        using value_type = ArithmeticType;
        using dimension = Dimensions;
        using quantity_tag = QuantityTagType;
        using default_unit = DefaultUnitType;

        constexpr quantity() = default;

        constexpr quantity(
            ArithmeticType value
        );

        template <is_arithmetic T2, is_dimension Dim2, typename Tag2, typename Unit2>
            requires has_same_dimensions<Dimensions, Dim2> && (std::same_as<QuantityTagType, Tag2> || std::same_as<Tag2, generic_quantity_tag>)
        constexpr quantity(const quantity<T2, Dim2, Tag2, Unit2>& other)
            : m_val(static_cast<ArithmeticType>(other.template as<DefaultUnitType>())) {}

        template <is_unit UnitType> requires valid_unit_for_quantity<UnitType, quantity>
        constexpr auto set(ArithmeticType value) -> void {
            m_val = this->converted_value<UnitType>(value);
        }

        template <is_unit UnitType> requires valid_unit_for_quantity<UnitType, quantity>
        constexpr auto as() const -> ArithmeticType {
            using r_u = UnitType::conversion_ratio;
            using r_d = DefaultUnitType::conversion_ratio;

            const long double v = static_cast<long double>(m_val);

            const long double num = static_cast<long double>(r_d::num) * static_cast<long double>(r_u::den);
            const long double den = static_cast<long double>(r_d::den) * static_cast<long double>(r_u::num);

            long double out = v * num / den;

            if constexpr (std::is_integral_v<ArithmeticType>) {
                return static_cast<ArithmeticType>(cexpr_llround(out));
            }
            else {
                return static_cast<ArithmeticType>(out);
            }
        }

        template <auto UnitObj> requires is_unit<decltype(UnitObj)> && valid_unit_for_quantity<decltype(UnitObj), quantity>
		constexpr auto as() const -> ArithmeticType {
            return this->as<decltype(UnitObj)>();
        }

        template <is_unit UnitType> requires valid_unit_for_quantity<UnitType, quantity>
        constexpr static auto from(ArithmeticType value) -> quantity {
            quantity result;
            result.m_val = result.converted_value<UnitType>(value);
            return result;
        }

        constexpr auto operator<=>(
            const quantity&
        ) const = default;
    protected:
        template <is_unit UnitType>
        constexpr auto converted_value(
            ArithmeticType value
        ) const -> ArithmeticType;

        ArithmeticType m_val = static_cast<ArithmeticType>(0);
    };
}

template <gse::internal::is_arithmetic A, gse::internal::is_dimension D, typename Tag, typename DefUnit>
constexpr gse::internal::quantity<A, D, Tag, DefUnit>::quantity(A value): m_val(value) {}

template <gse::internal::is_arithmetic A, gse::internal::is_dimension D, typename Tag, typename DefUnit>
template <gse::internal::is_unit UnitType>
constexpr auto gse::internal::quantity<A, D, Tag, DefUnit>::converted_value(A value) const -> A {
    using u = UnitType;
    using def = DefUnit;

    using r_u = u::conversion_ratio;
    using r_d = def::conversion_ratio;

    const long double v = static_cast<long double>(value);

    const long double num = static_cast<long double>(r_u::num) * static_cast<long double>(r_d::den);
    const long double den = static_cast<long double>(r_u::den) * static_cast<long double>(r_d::num);

    long double out = v * num / den;

    if constexpr (std::is_integral_v<A>) {
        return static_cast<A>(cexpr_llround(out));
    }
    else {
        return static_cast<A>(out);
    }
}

template <typename A, typename Dim, typename Tag, typename Unit, typename CharT>
struct std::formatter<gse::internal::quantity<A, Dim, Tag, Unit>, CharT> {
    std::formatter<A, CharT> value_fmt;

    template <class ParseContext>
    constexpr auto parse(ParseContext& ctx) -> ParseContext::iterator {
        return value_fmt.parse(ctx);
    }

    template <typename FormatContext>
    auto format(const gse::internal::quantity<A, Dim, Tag, Unit>& q, FormatContext& ctx) const {
        auto it = value_fmt.format(q.template as<typename gse::internal::quantity<A, Dim, Tag, Unit>::default_unit>(), ctx);
        if constexpr (!std::same_as<Unit, gse::internal::no_default_unit>) {
            it = std::ranges::copy(std::string_view{ " " }, it).out;
            it = std::ranges::copy(std::string_view{ Unit::unit_name }, it).out;
        }
        return it;
    }
};

export namespace gse::internal {
    template <typename Q>
    concept is_quantity =
        requires {
        typename std::remove_cvref_t<Q>::value_type;
        typename std::remove_cvref_t<Q>::dimension;
        typename std::remove_cvref_t<Q>::quantity_tag;
        typename std::remove_cvref_t<Q>::default_unit;
    };

    template <typename Q1, typename Q2>
    concept has_same_dimension_as =
        is_quantity<Q1> && is_quantity<Q2> &&
        has_same_dimensions<
            typename std::remove_cvref_t<Q1>::dimension,
            typename std::remove_cvref_t<Q2>::dimension
        >;

    template <typename T, is_dimension Dim>
    using generic_quantity = quantity<T, Dim, generic_quantity_tag, no_default_unit>;

    template <typename T1, typename T2>
    consteval auto common_quantity_fn() {
        if constexpr (is_quantity<T1> && is_quantity<T2>) {
            if constexpr (std::is_same_v<typename T1::quantity_tag, generic_quantity_tag>) {
                return std::type_identity<T2>{};
            } else {
                return std::type_identity<T1>{};
            }
        } else {
            return std::type_identity<std::common_type_t<T1, T2>>{};
        }
    }

    template <typename T1, typename T2>
    using common_quantity_t = decltype(common_quantity_fn<T1, T2>())::type;
}

export namespace gse::internal {
	template <is_quantity Q1, is_quantity Q2>
		requires has_same_dimension_as<Q1, Q2>
	constexpr auto operator+(
		const Q1& lhs,
		const Q2& rhs
	) -> Q1;

	template <is_quantity Q1, is_quantity Q2>
		requires has_same_dimension_as<Q1, Q2>
	constexpr auto operator-(
		const Q1& lhs,
		const Q2& rhs
	) -> Q1;

	template <is_quantity Q1, is_quantity Q2>
	constexpr auto operator*(
		const Q1& lhs,
		const Q2& rhs
	);

	template <is_quantity Q, is_arithmetic S>
	constexpr auto operator*(
		const Q& lhs,
		const S& rhs
	) -> Q;

	template <is_arithmetic S, is_quantity Q>
	constexpr auto operator*(
		const S& lhs,
		const Q& rhs
	) -> Q;

	template <is_quantity Q1, is_quantity Q2>
		requires has_same_dimension_as<Q1, Q2>
	constexpr auto operator/(
		const Q1& lhs,
		const Q2& rhs
	) -> Q1::value_type;

	template <is_quantity Q1, is_quantity Q2>
	constexpr auto operator/(
		const Q1& lhs,
		const Q2& rhs
	);

	template <is_quantity Q, is_arithmetic S>
	constexpr auto operator/(
		const Q& lhs,
		const S& rhs
	) -> Q;

	template <is_arithmetic S, is_quantity Q>
	constexpr auto operator/(
		const S& lhs,
		const Q& rhs
	);

	template <is_quantity Q1, is_quantity Q2>
		requires has_same_dimension_as<Q1, Q2>
	constexpr auto operator+=(
		Q1& lhs,
		const Q2& rhs
	) -> Q1&;

	template <is_quantity Q1, is_quantity Q2>
		requires has_same_dimension_as<Q1, Q2>
	constexpr auto operator-=(
		Q1& lhs,
		const Q2& rhs
	) -> Q1&;

	template <is_quantity Q, is_arithmetic S>
	constexpr auto operator*=(
		Q& lhs,
		const S& rhs
	) -> Q&;

	template <is_quantity Q, is_arithmetic S>
	constexpr auto operator/=(
		Q& lhs,
		const S& rhs
	) -> Q&;

	template <is_quantity Q>
	constexpr auto operator-(
		const Q& v
	) -> Q;
}

template <gse::internal::is_quantity Q1, gse::internal::is_quantity Q2>
	requires gse::internal::has_same_dimension_as<Q1, Q2>
constexpr auto gse::internal::operator+(const Q1& lhs, const Q2& rhs) -> Q1 {
	return Q1(lhs.template as<typename Q1::default_unit>() + rhs.template as<typename Q2::default_unit>());
}

template <gse::internal::is_quantity Q1, gse::internal::is_quantity Q2>
	requires gse::internal::has_same_dimension_as<Q1, Q2>
constexpr auto gse::internal::operator-(const Q1& lhs, const Q2& rhs) -> Q1 {
	return Q1(lhs.template as<typename Q1::default_unit>() - rhs.template as<typename Q2::default_unit>());
}

template <gse::internal::is_quantity Q1, gse::internal::is_quantity Q2>
constexpr auto gse::internal::operator*(const Q1& lhs, const Q2& rhs) {
	using result_v = std::common_type_t<typename Q1::value_type, typename Q2::value_type>;
	using result_d = decltype(typename Q1::dimension() * typename Q2::dimension());
	return generic_quantity<result_v, result_d>(lhs.template as<Q1::default_unit>() * rhs.template as<Q2::default_unit>());
}

template <gse::internal::is_quantity Q, gse::internal::is_arithmetic S>
constexpr auto gse::internal::operator*(const Q& lhs, const S& rhs) -> Q {
	return Q(lhs.template as<Q::default_unit>() * static_cast<Q::value_type>(rhs));
}

template <gse::internal::is_arithmetic S, gse::internal::is_quantity Q>
constexpr auto gse::internal::operator*(const S& lhs, const Q& rhs) -> Q {
	return Q(static_cast<Q::value_type>(lhs) * rhs.template as<Q::default_unit>());
}

template <gse::internal::is_quantity Q1, gse::internal::is_quantity Q2>
	requires gse::internal::has_same_dimension_as<Q1, Q2>
constexpr auto gse::internal::operator/(const Q1& lhs, const Q2& rhs) -> Q1::value_type {
	return lhs.template as<typename Q1::default_unit>() / rhs.template as<typename Q2::default_unit>();
}

template <gse::internal::is_quantity Q1, gse::internal::is_quantity Q2>
constexpr auto gse::internal::operator/(const Q1& lhs, const Q2& rhs) {
	using result_v = std::common_type_t<typename Q1::value_type, typename Q2::value_type>;
	using result_d = decltype(typename Q1::dimension() / typename Q2::dimension());
	return generic_quantity<result_v, result_d>(lhs.template as<typename Q1::default_unit>() / rhs.template as<typename Q2::default_unit>());
}

template <gse::internal::is_quantity Q, gse::internal::is_arithmetic S>
constexpr auto gse::internal::operator/(const Q& lhs, const S& rhs) -> Q {
	return Q(lhs.template as<typename Q::default_unit>() / static_cast<Q::value_type>(rhs));
}

template <gse::internal::is_arithmetic S, gse::internal::is_quantity Q>
constexpr auto gse::internal::operator/(const S& lhs, const Q& rhs) {
	using result_v = Q::value_type;
	using result_d = decltype(dim<0, 0, 0>() / typename Q::dimension());
	return generic_quantity<result_v, result_d>(static_cast<result_v>(lhs) / rhs.template as<typename Q::default_unit>());
}

template <gse::internal::is_quantity Q1, gse::internal::is_quantity Q2>
	requires gse::internal::has_same_dimension_as<Q1, Q2>
constexpr auto gse::internal::operator+=(Q1& lhs, const Q2& rhs) -> Q1& {
	lhs.set<typename Q1::default_unit>(lhs.template as<typename Q1::default_unit>() + rhs.template as<typename Q1::default_unit>());
	return lhs;
}

template <gse::internal::is_quantity Q1, gse::internal::is_quantity Q2>
	requires gse::internal::has_same_dimension_as<Q1, Q2>
constexpr auto gse::internal::operator-=(Q1& lhs, const Q2& rhs) -> Q1& {
	lhs.set<typename Q1::default_unit>(lhs.template as<typename Q1::default_unit>() - rhs.template as<typename Q1::default_unit>());
	return lhs;
}

template <gse::internal::is_quantity Q, gse::internal::is_arithmetic S>
constexpr auto gse::internal::operator*=(Q& lhs, const S& rhs) -> Q& {
	lhs.set<typename Q::default_unit>(lhs.template as<typename Q::default_unit>() * rhs);
	return lhs;
}

template <gse::internal::is_quantity Q, gse::internal::is_arithmetic S>
constexpr auto gse::internal::operator/=(Q& lhs, const S& rhs) -> Q& {
	lhs.set<typename Q::default_unit>(lhs.template as<typename Q::default_unit>() / rhs);
	return lhs;
}

template <gse::internal::is_quantity Q>
constexpr auto gse::internal::operator-(const Q& v) -> Q {
	return Q(-v.template as<typename Q::default_unit>());
}

export namespace gse {
    template <typename ToQuantity, typename FromQuantity> requires gse::internal::has_same_dimension_as<ToQuantity, FromQuantity>
    constexpr auto quantity_cast(const FromQuantity& q) -> ToQuantity;
}

template <typename ToQuantity, typename FromQuantity> requires gse::internal::has_same_dimension_as<ToQuantity, FromQuantity>
constexpr auto gse::quantity_cast(const FromQuantity& q) -> ToQuantity {
    using to_unit = ToQuantity::default_unit;
    using to_val = ToQuantity::value_type;

    const long double value_in_to_unit = static_cast<long double>(q.template as<to_unit>());

    if constexpr (std::is_integral_v<to_val>) {
        return ToQuantity::template from<to_unit>(
            static_cast<to_val>(internal::cexpr_llround(value_in_to_unit))
        );
    }
    else {
        return ToQuantity::template from<to_unit>(
            static_cast<to_val>(value_in_to_unit)
        );
    }
}

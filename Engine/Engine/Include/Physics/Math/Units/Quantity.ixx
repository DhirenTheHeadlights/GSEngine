export module gse.physics.math:quant;

import std;

import :dimension;

namespace gse::internal {
    export template<typename... Units>
    struct unit_list {
        using type = std::tuple<Units...>;
    };

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
}

template <size_t N, typename CharT>
struct std::formatter<gse::internal::fixed_string<N>, CharT> : std::formatter<std::string_view, CharT> {
    template <typename FormatContext>
    auto format(const gse::internal::fixed_string<N>& fs, FormatContext& ctx) const {
        return std::formatter<std::string_view, CharT>::format(fs, ctx);
    }
};

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

    export template <is_dimension>
    struct dimension_traits {
        using tag = generic_quantity_tag;
        using default_unit = no_default_unit;
    };
}

namespace gse::internal {
    template <typename UnitType, typename QuantityType>
    concept valid_unit_for_quantity =
        std::is_same_v<
            typename UnitType::quantity_tag,
            typename QuantityType::quantity_tag
        >;

    template <
        typename Derived,
        is_arithmetic ArithmeticType,
        is_dimension Dimensions,
        typename QuantityTagType,
        typename DefaultUnitType
    >
    class quantity_t {
    public:
        using derived = Derived;
        using value_type = ArithmeticType;
        using dimension = Dimensions;
        using quantity_tag = QuantityTagType;
        using default_unit = DefaultUnitType;

        constexpr quantity_t() = default;
        constexpr quantity_t(ArithmeticType value) : m_val(value) {}

        template <is_unit UnitType> requires valid_unit_for_quantity<UnitType, Derived>
        constexpr auto set(ArithmeticType value) -> void {
            m_val = this->converted_value<UnitType>(value);
        }

        template <auto UnitObject> requires is_unit<decltype(UnitObject)> && valid_unit_for_quantity<decltype(UnitObject), Derived>
        constexpr auto as() const -> ArithmeticType {
            using ratio = typename decltype(UnitObject)::conversion_ratio;
            return m_val * static_cast<ArithmeticType>(ratio::den) / static_cast<ArithmeticType>(ratio::num);
        }

        constexpr auto as_default_unit(this auto&& self) -> ArithmeticType {
            return self.m_val;
        }

        template <is_unit UnitType> requires valid_unit_for_quantity<UnitType, Derived>
        constexpr static auto from(ArithmeticType value) -> Derived {
            static_assert(valid_unit_for_quantity<UnitType, Derived>, "Invalid unit type for conversion");
            Derived result;
            result.m_val = result.template converted_value<UnitType>(value);
            return result;
        }

        constexpr auto operator<=>(const quantity_t&) const = default;
    protected:
        template <is_unit UnitType>
        constexpr auto converted_value(ArithmeticType value) const -> ArithmeticType {
            using ratio = typename UnitType::conversion_ratio;
            return value * static_cast<ArithmeticType>(ratio::num) / static_cast<ArithmeticType>(ratio::den);
        }

        ArithmeticType m_val = static_cast<ArithmeticType>(0);
    };
}

export namespace gse::internal {
    template <is_arithmetic T, is_dimension Dim>
    struct quantity : quantity_t<
        quantity<T, Dim>,                      
        T,                                       
        Dim,                                     
        typename dimension_traits<Dim>::tag,
        typename dimension_traits<Dim>::default_unit
	> {
		using quantity_t<
			quantity,
			T,
			Dim,
			typename dimension_traits<Dim>::tag,
			typename dimension_traits<Dim>::default_unit
		>::quantity_t;
	};

    template <typename Q>
    concept is_quantity =
        requires {
        typename std::remove_cvref_t<Q>::value_type;
        typename std::remove_cvref_t<Q>::dimension;
        typename std::remove_cvref_t<Q>::quantity_tag;
        typename std::remove_cvref_t<Q>::default_unit;
        typename std::remove_cvref_t<Q>::derived;
    } &&
        std::same_as<
        typename std::remove_cvref_t<Q>::derived,
        std::remove_cvref_t<Q>
	>;

    template <typename Q1, typename Q2>
	concept has_same_dimension_as = 
        is_quantity<Q1> && is_quantity<Q2> &&
        has_same_dimensions<
	        typename std::remove_cvref_t<Q1>::dimension,
			typename std::remove_cvref_t<Q2>::dimension
		>;
}

template <gse::internal::is_quantity Q, typename CharT>
struct std::formatter<Q, CharT> {
    std::formatter<typename Q::value_type, CharT> value_fmt;

	template <class ParseContext>
    constexpr auto parse(ParseContext& ctx) -> typename ParseContext::iterator {
        return value_fmt.parse(ctx);
    }

    template <typename FormatContext>
    auto format(const Q& q, FormatContext& ctx) const {
        auto it = value_fmt.format(q.as_default_unit(), ctx);
		if (std::string_view(Q::default_unit::unit_name) != "no_default_unit") {
		    it = std::ranges::copy(std::string_view{" "}, it).out;
		    it = std::ranges::copy(std::string_view{Q::default_unit::unit_name}, it).out;
		}
		return it;
    }
};

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
	) -> typename Q1::value_type;

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
	return Q1(lhs.as_default_unit() + rhs.as_default_unit());
}

template <gse::internal::is_quantity Q1, gse::internal::is_quantity Q2>
	requires gse::internal::has_same_dimension_as<Q1, Q2>
constexpr auto gse::internal::operator-(const Q1& lhs, const Q2& rhs) -> Q1 {
	return Q1(lhs.as_default_unit() - rhs.as_default_unit());
}

template <gse::internal::is_quantity Q1, gse::internal::is_quantity Q2>
constexpr auto gse::internal::operator*(const Q1& lhs, const Q2& rhs) {
	using result_v = std::common_type_t<typename Q1::value_type, typename Q2::value_type>;
	using result_d = decltype(typename Q1::dimension()* typename Q2::dimension());
	return quantity<result_v, result_d>(lhs.as_default_unit() * rhs.as_default_unit());
}

template <gse::internal::is_quantity Q, gse::internal::is_arithmetic S>
constexpr auto gse::internal::operator*(const Q& lhs, const S& rhs) -> Q {
	return Q(lhs.as_default_unit() * static_cast<typename Q::value_type>(rhs));
}

template <gse::internal::is_arithmetic S, gse::internal::is_quantity Q>
constexpr auto gse::internal::operator*(const S& lhs, const Q& rhs) -> Q {
	return Q(static_cast<typename Q::value_type>(lhs) * rhs.as_default_unit());
}

template <gse::internal::is_quantity Q1, gse::internal::is_quantity Q2>
	requires gse::internal::has_same_dimension_as<Q1, Q2>
constexpr auto gse::internal::operator/(const Q1& lhs, const Q2& rhs) -> typename Q1::value_type {
	return lhs.as_default_unit() / rhs.as_default_unit();
}

template <gse::internal::is_quantity Q1, gse::internal::is_quantity Q2>
constexpr auto gse::internal::operator/(const Q1& lhs, const Q2& rhs) {
	using result_v = std::common_type_t<typename Q1::value_type, typename Q2::value_type>;
	using result_d = decltype(typename Q1::dimension() / typename Q2::dimension());
	return quantity<result_v, result_d>(lhs.as_default_unit() / rhs.as_default_unit());
}

template <gse::internal::is_quantity Q, gse::internal::is_arithmetic S>
constexpr auto gse::internal::operator/(const Q& lhs, const S& rhs) -> Q {
	return Q(lhs.as_default_unit() / static_cast<typename Q::value_type>(rhs));
}

template <gse::internal::is_arithmetic S, gse::internal::is_quantity Q>
constexpr auto gse::internal::operator/(const S& lhs, const Q& rhs) {
	using result_v = typename Q::value_type;
	using result_d = decltype(dim<0, 0, 0>() / typename Q::dimension());
	return quantity<result_v, result_d>(static_cast<result_v>(lhs) / rhs.as_default_unit());
}

template <gse::internal::is_quantity Q1, gse::internal::is_quantity Q2>
	requires gse::internal::has_same_dimension_as<Q1, Q2>
constexpr auto gse::internal::operator+=(Q1& lhs, const Q2& rhs) -> Q1& {
	lhs.set<typename Q1::default_unit>(lhs.as_default_unit() + rhs.as_default_unit());
	return lhs;
}

template <gse::internal::is_quantity Q1, gse::internal::is_quantity Q2>
	requires gse::internal::has_same_dimension_as<Q1, Q2>
constexpr auto gse::internal::operator-=(Q1& lhs, const Q2& rhs) -> Q1& {
	lhs.set<typename Q1::default_unit>(lhs.as_default_unit() - rhs.as_default_unit());
	return lhs;
}

template <gse::internal::is_quantity Q, gse::internal::is_arithmetic S>
constexpr auto gse::internal::operator*=(Q& lhs, const S& rhs) -> Q& {
	lhs.set<typename Q::default_unit>(lhs.as_default_unit() * rhs);
	return lhs;
}

template <gse::internal::is_quantity Q, gse::internal::is_arithmetic S>
constexpr auto gse::internal::operator/=(Q& lhs, const S& rhs) -> Q& {
	lhs.set<typename Q::default_unit>(lhs.as_default_unit() / rhs);
	return lhs;
}

template <gse::internal::is_quantity Q>
constexpr auto gse::internal::operator-(const Q& v) -> Q {
	return Q(-v.as_default_unit());
}
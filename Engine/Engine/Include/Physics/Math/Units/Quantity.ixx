export module gse.physics.math:quant;

import std;

import :dimension;

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

    struct generic_quantity_tag {};
	constexpr char no_default_unit_name[] = "no_default_unit";
	using no_default_unit = unit<generic_quantity_tag, 1.0f, no_default_unit_name>;

    export template <is_dimension>
    struct dimension_traits {
        using tag = generic_quantity_tag;
		using default_unit = no_default_unit;
		using valid_units = unit_list<no_default_unit>;;
    };

    export template <typename T>
	concept is_arithmetic = std::integral<T> || std::floating_point<T>;

    template <typename UnitType, typename ValidUnits>
    constexpr auto is_valid_unit_for_quantity() -> bool {
        using tuple_type = typename ValidUnits::type;
        return std::apply(
            []<typename... T0>(T0...) constexpr {
            return ((std::is_same_v<typename UnitType::quantity_tag,
                typename T0::quantity_tag>) || ...);
        },
            tuple_type{}
        );
    }
}

namespace gse::internal {
    template <typename T, typename U>
    concept valid_unit_for_quantity = is_valid_unit_for_quantity<T, U>();

    template <
        typename Derived,
		is_arithmetic ArithmeticType,
		is_dimension Dimensions,
		typename QuantityTagType,
		typename DefaultUnitType,
		typename ValidUnits
	>
    class quantity_t {
    public:
        using derived = Derived;
        using value_type = ArithmeticType;
        using dimension = Dimensions;
        using quantity_tag = QuantityTagType;
        using default_unit = DefaultUnitType;
        using units = ValidUnits;

        constexpr quantity_t() = default;
        constexpr quantity_t(ArithmeticType value) : m_val(value) {}

        template <is_unit UnitType>
        constexpr quantity_t(ArithmeticType value) : m_val(this->converted_value<UnitType>(value)) {}

		template <is_unit UnitType> requires valid_unit_for_quantity<UnitType, ValidUnits>
        constexpr auto set(ArithmeticType value) -> void {
			m_val = this->converted_value<UnitType>(value);
        }

		template <is_unit UnitType> requires valid_unit_for_quantity<UnitType, ValidUnits>
		constexpr auto as() const -> ArithmeticType {
			return m_val / UnitType::conversion_factor;
		}

		constexpr auto as_default_unit(this auto&& self) -> ArithmeticType {
			return self.m_val;
		}

		template <is_unit UnitType> requires valid_unit_for_quantity<UnitType, ValidUnits>
		constexpr static auto from(ArithmeticType value) -> Derived {
			static_assert(internal::is_valid_unit_for_quantity<UnitType, ValidUnits>(), "Invalid unit type for conversion");
			Derived result;
			result.m_val = result.template converted_value<UnitType>(value);
			return result;
		}

        constexpr auto operator<=>(const quantity_t&) const = default;
    protected:
        template <is_unit UnitType>
		constexpr auto converted_value(ArithmeticType value) const -> ArithmeticType {
			return value * UnitType::conversion_factor;
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
        typename dimension_traits<Dim>::default_unit,
        typename dimension_traits<Dim>::valid_units
	> {
		using quantity_t<
			quantity,
			T,
			Dim,
			typename dimension_traits<Dim>::tag,
			typename dimension_traits<Dim>::default_unit,
			typename dimension_traits<Dim>::valid_units
		>::quantity_t;
	};

    template <typename Q>
    concept is_quantity =
        requires {
        typename std::remove_cvref_t<Q>::value_type;
        typename std::remove_cvref_t<Q>::dimension;
        typename std::remove_cvref_t<Q>::quantity_tag;
        typename std::remove_cvref_t<Q>::default_unit;
        typename std::remove_cvref_t<Q>::units;
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

    constexpr auto parse(std::format_parse_context& ctx) {
        return value_fmt.parse(ctx);
    }

    template <typename FormatContext>
    auto format(const Q& q, FormatContext& ctx) const {
        auto out = value_fmt.format(q.as_default_unit(), ctx);
        if (std::string_view(Q::default_unit::unit_name) != gse::internal::no_default_unit_name) {
            out = std::format_to(out, " {}", Q::default_unit::unit_name);
        }
        return out;
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

export module gse.math:quant;

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

export namespace gse::internal {
    struct root_quantity_spec {};
    struct quantity_spec_marker {};
    struct kind_quantity_spec_marker {};

    template <typename Parent = root_quantity_spec>
    struct quantity_spec : quantity_spec_marker {
        using parent_spec = Parent;
    };

    template <typename Parent = root_quantity_spec>
    struct kind_quantity_spec : quantity_spec<Parent>, kind_quantity_spec_marker {};

    template <typename T>
    concept is_quantity_spec = std::derived_from<T, quantity_spec_marker>;

    template <typename T>
    concept is_kind_spec = std::derived_from<T, kind_quantity_spec_marker>;
}

namespace gse::internal {
    template <typename... Ts>
    struct type_list {};

    template <typename T, typename List>
    struct tl_prepend;

    template <typename T, typename... Ts>
    struct tl_prepend<T, type_list<Ts...>> {
        using type = type_list<T, Ts...>;
    };

    template <typename T, typename List>
    using tl_prepend_t = typename tl_prepend<T, List>::type;

    template <typename T, typename List>
    struct tl_contains : std::false_type {};

    template <typename T, typename... Ts>
    struct tl_contains<T, type_list<T, Ts...>> : std::true_type {};

    template <typename T, typename U, typename... Ts>
    struct tl_contains<T, type_list<U, Ts...>> : tl_contains<T, type_list<Ts...>> {};

    template <typename T, typename List>
    constexpr bool tl_contains_v = tl_contains<T, List>::value;

    template <typename Spec>
    struct spec_ancestors_impl {
        using type = type_list<Spec>;
    };

    template <typename Spec>
        requires is_quantity_spec<Spec> && (!std::is_same_v<typename Spec::parent_spec, root_quantity_spec>)
    struct spec_ancestors_impl<Spec> {
        using type = tl_prepend_t<Spec, typename spec_ancestors_impl<typename Spec::parent_spec>::type>;
    };

    template <typename Spec>
    using spec_ancestors_t = typename spec_ancestors_impl<Spec>::type;

    template <typename Ancestor, typename Spec>
    struct is_ancestor_or_equal_impl : std::false_type {};

    template <typename Ancestor, typename Spec>
        requires is_quantity_spec<Ancestor> && is_quantity_spec<Spec>
    struct is_ancestor_or_equal_impl<Ancestor, Spec>
        : std::bool_constant<tl_contains_v<Ancestor, spec_ancestors_t<Spec>>> {};

    template <typename Ancestor, typename Spec>
    constexpr bool is_ancestor_or_equal_v = is_ancestor_or_equal_impl<Ancestor, Spec>::value;

    template <typename AncestorList, typename S2>
    struct lca_impl {
        using type = void;
    };

    template <typename Head, typename... Tail, typename S2>
    struct lca_impl<type_list<Head, Tail...>, S2> {
        using type = std::conditional_t<
            is_ancestor_or_equal_v<Head, S2>,
            Head,
            typename lca_impl<type_list<Tail...>, S2>::type
        >;
    };

    template <typename S1, typename S2>
        requires is_quantity_spec<S1> && is_quantity_spec<S2>
    using lca_spec_t = typename lca_impl<spec_ancestors_t<S1>, S2>::type;

    template <typename Tag1, typename Tag2>
    consteval bool are_specs_addable_fn() {
        if constexpr (std::is_same_v<Tag1, Tag2>) {
            return true;
        } else if constexpr (is_quantity_spec<Tag1> && is_quantity_spec<Tag2>) {
            if constexpr (is_ancestor_or_equal_v<Tag1, Tag2> || is_ancestor_or_equal_v<Tag2, Tag1>) {
                return true;
            } else {
                return !(is_kind_spec<Tag1> || is_kind_spec<Tag2>);
            }
        } else {
            return true;
        }
    }

    template <typename Tag1, typename Tag2>
    concept specs_addable = are_specs_addable_fn<Tag1, Tag2>();

    template <typename Tag1, typename Tag2>
    struct kind_mismatch_diagnostic {
        static_assert(!std::is_same_v<Tag1, Tag1>,
            "Cannot add or subtract quantities of incompatible kind specs");
    };
}

namespace gse::internal {
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
	concept valid_unit_for_quantity =
        std::same_as<typename QuantityType::quantity_tag, generic_quantity_tag> ||
        std::same_as<typename UnitType::quantity_tag, generic_quantity_tag> ||
        std::same_as<typename UnitType::quantity_tag, typename QuantityType::quantity_tag> ||
        is_ancestor_or_equal_v<typename UnitType::quantity_tag, typename QuantityType::quantity_tag>;

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

        explicit constexpr quantity(
            ArithmeticType value
        );

        template <is_arithmetic T2, is_dimension Dim2, typename Tag2, typename Unit2>
            requires has_same_dimensions<Dimensions, Dim2> &&
                     (std::same_as<QuantityTagType, Tag2> ||
                      std::same_as<Tag2, generic_quantity_tag> ||
                      is_ancestor_or_equal_v<QuantityTagType, Tag2>)
        constexpr quantity(const quantity<T2, Dim2, Tag2, Unit2>& other)
            : m_val(static_cast<ArithmeticType>(other.template as<DefaultUnitType>())) {}

        template <is_arithmetic T2, is_dimension Dim2, typename Tag2, typename Unit2>
            requires (!has_same_dimensions<Dimensions, Dim2>)
        constexpr quantity(const quantity<T2, Dim2, Tag2, Unit2>&) {
            dimension_mismatch_diagnostic<Dimensions, Dim2>{};
        }

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
            using Tag1 = typename T1::quantity_tag;
            using Tag2 = typename T2::quantity_tag;
            if constexpr (std::is_same_v<Tag1, generic_quantity_tag>) {
                return std::type_identity<T2>{};
            } else if constexpr (std::is_same_v<Tag2, generic_quantity_tag>) {
                return std::type_identity<T1>{};
            } else if constexpr (is_quantity_spec<Tag1> && is_quantity_spec<Tag2>) {
                using lca = lca_spec_t<Tag1, Tag2>;
                using result_val = std::common_type_t<typename T1::value_type, typename T2::value_type>;
                if constexpr (std::is_void_v<lca> || (!std::is_same_v<lca, Tag1> && !std::is_same_v<lca, Tag2>)) {
                    return std::type_identity<generic_quantity<result_val, typename T1::dimension>>{};
                } else if constexpr (std::is_same_v<lca, Tag1>) {
                    return std::type_identity<quantity<result_val, typename T1::dimension, Tag1, typename T1::default_unit>>{};
                } else {
                    return std::type_identity<quantity<result_val, typename T1::dimension, Tag2, typename T2::default_unit>>{};
                }
            } else {
                return std::type_identity<T1>{};
            }
        } else {
            return std::type_identity<T1>{};
        }
    }

    template <typename T1, typename T2>
    using common_quantity_t = decltype(common_quantity_fn<T1, T2>())::type;
}

export namespace gse {
    template <typename Q, typename Spec>
    concept quantity_of =
        internal::is_quantity<Q> &&
        internal::is_ancestor_or_equal_v<Spec, typename std::remove_cvref_t<Q>::quantity_tag>;
}

export namespace gse::internal {
	template <is_quantity Q1, is_quantity Q2>
		requires has_same_dimension_as<Q1, Q2> && specs_addable<typename Q1::quantity_tag, typename Q2::quantity_tag>
	constexpr auto operator+(
		const Q1& lhs,
		const Q2& rhs
	) -> common_quantity_t<Q1, Q2>;

	template <is_quantity Q1, is_quantity Q2>
		requires has_same_dimension_as<Q1, Q2> && specs_addable<typename Q1::quantity_tag, typename Q2::quantity_tag>
	constexpr auto operator-(
		const Q1& lhs,
		const Q2& rhs
	) -> common_quantity_t<Q1, Q2>;

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
		requires has_same_dimension_as<Q1, Q2> && specs_addable<typename Q1::quantity_tag, typename Q2::quantity_tag>
	constexpr auto operator+=(
		Q1& lhs,
		const Q2& rhs
	) -> Q1&;

	template <is_quantity Q1, is_quantity Q2>
		requires has_same_dimension_as<Q1, Q2> && specs_addable<typename Q1::quantity_tag, typename Q2::quantity_tag>
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

	template <is_quantity Q1, is_quantity Q2>
		requires (!has_same_dimension_as<Q1, Q2>)
	constexpr auto operator+(const Q1& lhs, const Q2&) -> Q1 {
		dimension_mismatch_diagnostic<typename Q1::dimension, typename Q2::dimension>{};
		return lhs;
	}

	template <is_quantity Q1, is_quantity Q2>
		requires (!has_same_dimension_as<Q1, Q2>)
	constexpr auto operator-(const Q1& lhs, const Q2&) -> Q1 {
		dimension_mismatch_diagnostic<typename Q1::dimension, typename Q2::dimension>{};
		return lhs;
	}

	template <is_quantity Q1, is_quantity Q2>
		requires (!has_same_dimension_as<Q1, Q2>)
	constexpr auto operator+=(Q1& lhs, const Q2&) -> Q1& {
		dimension_mismatch_diagnostic<typename Q1::dimension, typename Q2::dimension>{};
		return lhs;
	}

	template <is_quantity Q1, is_quantity Q2>
		requires (!has_same_dimension_as<Q1, Q2>)
	constexpr auto operator-=(Q1& lhs, const Q2&) -> Q1& {
		dimension_mismatch_diagnostic<typename Q1::dimension, typename Q2::dimension>{};
		return lhs;
	}

	template <is_quantity Q1, is_quantity Q2>
		requires has_same_dimension_as<Q1, Q2> && (!specs_addable<typename Q1::quantity_tag, typename Q2::quantity_tag>)
	constexpr auto operator+(const Q1& lhs, const Q2&) -> Q1 {
		kind_mismatch_diagnostic<typename Q1::quantity_tag, typename Q2::quantity_tag>{};
		return lhs;
	}

	template <is_quantity Q1, is_quantity Q2>
		requires has_same_dimension_as<Q1, Q2> && (!specs_addable<typename Q1::quantity_tag, typename Q2::quantity_tag>)
	constexpr auto operator-(const Q1& lhs, const Q2&) -> Q1 {
		kind_mismatch_diagnostic<typename Q1::quantity_tag, typename Q2::quantity_tag>{};
		return lhs;
	}

	template <is_quantity Q1, is_quantity Q2>
		requires has_same_dimension_as<Q1, Q2> && (!specs_addable<typename Q1::quantity_tag, typename Q2::quantity_tag>)
	constexpr auto operator+=(Q1& lhs, const Q2&) -> Q1& {
		kind_mismatch_diagnostic<typename Q1::quantity_tag, typename Q2::quantity_tag>{};
		return lhs;
	}

	template <is_quantity Q1, is_quantity Q2>
		requires has_same_dimension_as<Q1, Q2> && (!specs_addable<typename Q1::quantity_tag, typename Q2::quantity_tag>)
	constexpr auto operator-=(Q1& lhs, const Q2&) -> Q1& {
		kind_mismatch_diagnostic<typename Q1::quantity_tag, typename Q2::quantity_tag>{};
		return lhs;
	}
}

template <gse::internal::is_quantity Q1, gse::internal::is_quantity Q2>
	requires gse::internal::has_same_dimension_as<Q1, Q2> && gse::internal::specs_addable<typename Q1::quantity_tag, typename Q2::quantity_tag>
constexpr auto gse::internal::operator+(const Q1& lhs, const Q2& rhs) -> common_quantity_t<Q1, Q2> {
	using R = common_quantity_t<Q1, Q2>;
	return R(lhs.template as<typename R::default_unit>() + rhs.template as<typename R::default_unit>());
}

template <gse::internal::is_quantity Q1, gse::internal::is_quantity Q2>
	requires gse::internal::has_same_dimension_as<Q1, Q2> && gse::internal::specs_addable<typename Q1::quantity_tag, typename Q2::quantity_tag>
constexpr auto gse::internal::operator-(const Q1& lhs, const Q2& rhs) -> common_quantity_t<Q1, Q2> {
	using R = common_quantity_t<Q1, Q2>;
	return R(lhs.template as<typename R::default_unit>() - rhs.template as<typename R::default_unit>());
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
	using result_d = decltype(dimensionless{} / typename Q::dimension());
	return generic_quantity<result_v, result_d>(static_cast<result_v>(lhs) / rhs.template as<typename Q::default_unit>());
}

template <gse::internal::is_quantity Q1, gse::internal::is_quantity Q2>
	requires gse::internal::has_same_dimension_as<Q1, Q2> && gse::internal::specs_addable<typename Q1::quantity_tag, typename Q2::quantity_tag>
constexpr auto gse::internal::operator+=(Q1& lhs, const Q2& rhs) -> Q1& {
	lhs.set<typename Q1::default_unit>(lhs.template as<typename Q1::default_unit>() + rhs.template as<typename Q1::default_unit>());
	return lhs;
}

template <gse::internal::is_quantity Q1, gse::internal::is_quantity Q2>
	requires gse::internal::has_same_dimension_as<Q1, Q2> && gse::internal::specs_addable<typename Q1::quantity_tag, typename Q2::quantity_tag>
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

    template <internal::is_quantity Q1, internal::is_quantity Q2>
        requires internal::has_same_dimension_as<Q1, Q2>
    constexpr auto fmod(const Q1& a, const Q2& b) -> Q1;
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

template <gse::internal::is_quantity Q1, gse::internal::is_quantity Q2>
    requires gse::internal::has_same_dimension_as<Q1, Q2>
constexpr auto gse::fmod(const Q1& a, const Q2& b) -> Q1 {
    return Q1(std::fmod(a.template as<typename Q1::default_unit>(), b.template as<typename Q1::default_unit>()));
}

export namespace gse {
    template <internal::is_quantity Q>
    constexpr auto abs(const Q& q) -> Q {
        return Q(std::abs(q.template as<typename Q::default_unit>()));
    }

    template <internal::is_quantity Q>
    constexpr auto isfinite(const Q& q) -> bool {
        return std::isfinite(q.template as<typename Q::default_unit>());
    }

    template <internal::is_quantity Q>
    constexpr auto isnan(const Q& q) -> bool {
        return std::isnan(q.template as<typename Q::default_unit>());
    }

    template <internal::is_quantity Q1, internal::is_quantity Q2>
        requires internal::has_same_dimension_as<Q1, Q2>
    auto hypot(const Q1& a, const Q2& b) -> Q1 {
        return Q1(std::hypot(a.template as<typename Q1::default_unit>(), b.template as<typename Q1::default_unit>()));
    }

    template <internal::is_quantity Q>
    constexpr auto sqrt(const Q& q) {
        using result_d = decltype(internal::dim_sqrt(typename Q::dimension()));
        return internal::generic_quantity<typename Q::value_type, result_d>(
            std::sqrt(q.template as<typename Q::default_unit>())
        );
    }
}

export namespace gse::internal {
    struct default_point_origin {};

    template <is_quantity Quantity, typename Origin = default_point_origin>
    class quantity_point {
    public:
        using quantity_type = Quantity;
        using value_type = typename Quantity::value_type;
        using origin = Origin;

        constexpr quantity_point() = default;
        explicit constexpr quantity_point(value_type value) : m_qty(value) {}
        explicit constexpr quantity_point(const Quantity& q) : m_qty(q) {}

        template <is_quantity Q2, typename O2>
            requires has_same_dimension_as<Quantity, Q2> && std::same_as<Origin, O2>
        constexpr quantity_point(const quantity_point<Q2, O2>& other)
            : m_qty(other.quantity_from_origin()) {}

        constexpr auto quantity_from_origin(this auto const& self) -> const Quantity& {
            return self.m_qty;
        }

        template <is_unit UnitType>
            requires valid_unit_for_quantity<UnitType, Quantity>
        constexpr auto as(this auto const& self) -> value_type {
            return self.m_qty.template as<UnitType>();
        }

        template <auto UnitObj>
            requires is_unit<decltype(UnitObj)> && valid_unit_for_quantity<decltype(UnitObj), Quantity>
        constexpr auto as(this auto const& self) -> value_type {
            return self.m_qty.template as<UnitObj>();
        }

        template <is_unit UnitType>
            requires valid_unit_for_quantity<UnitType, Quantity>
        constexpr auto set(this auto& self, value_type value) -> void {
            self.m_qty.template set<UnitType>(value);
        }

        constexpr auto operator<=>(const quantity_point&) const = default;

    private:
        Quantity m_qty{};
    };

    template <typename T>
    concept is_quantity_point = requires {
        typename std::remove_cvref_t<T>::quantity_type;
        typename std::remove_cvref_t<T>::origin;
    } && is_quantity<typename std::remove_cvref_t<T>::quantity_type>;
}

template <typename Q, typename O, typename CharT>
struct std::formatter<gse::internal::quantity_point<Q, O>, CharT> {
    std::formatter<Q, CharT> qty_fmt;

    template <class ParseContext>
    constexpr auto parse(ParseContext& ctx) -> ParseContext::iterator {
        return qty_fmt.parse(ctx);
    }

    template <typename FormatContext>
    auto format(const gse::internal::quantity_point<Q, O>& pt, FormatContext& ctx) const {
        return qty_fmt.format(pt.quantity_from_origin(), ctx);
    }
};

export namespace gse::internal {
    template <is_quantity Q1, typename O1, is_quantity Q2, typename O2>
        requires has_same_dimension_as<Q1, Q2> && std::same_as<O1, O2>
    constexpr auto operator-(
        const quantity_point<Q1, O1>& lhs,
        const quantity_point<Q2, O2>& rhs
    ) -> common_quantity_t<Q1, Q2>;

    template <is_quantity QP, typename O, is_quantity QD>
        requires has_same_dimension_as<QP, QD>
    constexpr auto operator+(
        const quantity_point<QP, O>& lhs,
        const QD& rhs
    ) -> quantity_point<QP, O>;

    template <is_quantity QD, is_quantity QP, typename O>
        requires has_same_dimension_as<QD, QP>
    constexpr auto operator+(
        const QD& lhs,
        const quantity_point<QP, O>& rhs
    ) -> quantity_point<QP, O>;

    template <is_quantity QP, typename O, is_quantity QD>
        requires has_same_dimension_as<QP, QD>
    constexpr auto operator-(
        const quantity_point<QP, O>& lhs,
        const QD& rhs
    ) -> quantity_point<QP, O>;

    template <is_quantity QP, typename O, is_quantity QD>
        requires has_same_dimension_as<QP, QD>
    constexpr auto operator+=(
        quantity_point<QP, O>& lhs,
        const QD& rhs
    ) -> quantity_point<QP, O>&;

    template <is_quantity QP, typename O, is_quantity QD>
        requires has_same_dimension_as<QP, QD>
    constexpr auto operator-=(
        quantity_point<QP, O>& lhs,
        const QD& rhs
    ) -> quantity_point<QP, O>&;
}

template <gse::internal::is_quantity Q1, typename O1, gse::internal::is_quantity Q2, typename O2>
    requires gse::internal::has_same_dimension_as<Q1, Q2> && std::same_as<O1, O2>
constexpr auto gse::internal::operator-(
    const quantity_point<Q1, O1>& lhs,
    const quantity_point<Q2, O2>& rhs
) -> common_quantity_t<Q1, Q2> {
    return lhs.quantity_from_origin() - rhs.quantity_from_origin();
}

template <gse::internal::is_quantity QP, typename O, gse::internal::is_quantity QD>
    requires gse::internal::has_same_dimension_as<QP, QD>
constexpr auto gse::internal::operator+(
    const quantity_point<QP, O>& lhs,
    const QD& rhs
) -> quantity_point<QP, O> {
    auto sum = lhs.quantity_from_origin() + rhs;
    return quantity_point<QP, O>(QP(sum.template as<typename QP::default_unit>()));
}

template <gse::internal::is_quantity QD, gse::internal::is_quantity QP, typename O>
    requires gse::internal::has_same_dimension_as<QD, QP>
constexpr auto gse::internal::operator+(
    const QD& lhs,
    const quantity_point<QP, O>& rhs
) -> quantity_point<QP, O> {
    auto sum = lhs + rhs.quantity_from_origin();
    return quantity_point<QP, O>(QP(sum.template as<typename QP::default_unit>()));
}

template <gse::internal::is_quantity QP, typename O, gse::internal::is_quantity QD>
    requires gse::internal::has_same_dimension_as<QP, QD>
constexpr auto gse::internal::operator-(
    const quantity_point<QP, O>& lhs,
    const QD& rhs
) -> quantity_point<QP, O> {
    auto diff = lhs.quantity_from_origin() - rhs;
    return quantity_point<QP, O>(QP(diff.template as<typename QP::default_unit>()));
}

template <gse::internal::is_quantity QP, typename O, gse::internal::is_quantity QD>
    requires gse::internal::has_same_dimension_as<QP, QD>
constexpr auto gse::internal::operator+=(
    quantity_point<QP, O>& lhs,
    const QD& rhs
) -> quantity_point<QP, O>& {
    lhs = lhs + rhs;
    return lhs;
}

template <gse::internal::is_quantity QP, typename O, gse::internal::is_quantity QD>
    requires gse::internal::has_same_dimension_as<QP, QD>
constexpr auto gse::internal::operator-=(
    quantity_point<QP, O>& lhs,
    const QD& rhs
) -> quantity_point<QP, O>& {
    lhs = lhs - rhs;
    return lhs;
}

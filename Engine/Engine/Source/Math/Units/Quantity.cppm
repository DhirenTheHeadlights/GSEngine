export module gse.math:quant;

import std;

import gse.assert;

import :dimension;

namespace gse::internal {
    template <typename Tag>
    struct quantity_traits;

    struct no_parent_quantity_tag {};

    enum class quantity_semantic_kind {
        measurement,
        absolute,
        relative
    };

    template <std::size_t N>
    struct fixed_string {
        char data[N]{};
        constexpr fixed_string(const char(&str)[N]) {
            std::copy_n(str, N, data);
        }
        constexpr operator std::string_view() const {
            return { data, N - 1 };
        }
    };

    export template <typename QuantityTagType, is_ratio ConversionRatio, fixed_string UnitName>
    struct unit;

    template <std::meta::info UnitTypeInfo>
    inline constexpr typename [: UnitTypeInfo :] default_unit_for_type{};

    template <std::meta::info Dim, quantity_semantic_kind Kind, typename DefaultRatio, fixed_string DefaultName>
    struct quantity_root_spec {};

    template <std::meta::info Dim, quantity_semantic_kind Kind, typename DefaultRatio, fixed_string DefaultName>
    inline constexpr quantity_root_spec<Dim, Kind, DefaultRatio, DefaultName> quantity_root{};

    template <std::meta::info Parent, quantity_semantic_kind Kind>
    struct quantity_child_spec {};

    template <std::meta::info Parent, quantity_semantic_kind Kind>
    inline constexpr quantity_child_spec<Parent, Kind> quantity_child{};

    template <std::meta::info Parent>
    struct quantity_sub_root_spec {};

    template <std::meta::info Parent>
    inline constexpr quantity_sub_root_spec<Parent> quantity_sub_root{};

    template <std::meta::info Parent, std::meta::info RelativeTag>
    struct quantity_absolute_spec {};

    template <std::meta::info Parent, std::meta::info RelativeTag>
    inline constexpr quantity_absolute_spec<Parent, RelativeTag> quantity_absolute{};

    enum class spec_kind {
        none,
        root,
        child,
        sub_root,
        absolute
    };

    consteval auto quantity_spec_type_of(
        std::meta::info tag_info
    ) -> std::meta::info;

    consteval auto has_quantity_spec(
        std::meta::info tag_info
    ) -> bool;

    consteval auto classify_spec(
        std::meta::info spec_t
    ) -> spec_kind;

    consteval auto resolve_dim_info(
        std::meta::info tag_info
    ) -> std::meta::info;

    consteval auto resolve_default_unit_info(
        std::meta::info tag_info
    ) -> std::meta::info;

    consteval auto resolve_parent_tag(
        std::meta::info tag_info
    ) -> std::meta::info;

    consteval auto resolve_unit_family_tag(
        std::meta::info tag_info
    ) -> std::meta::info;

    consteval auto resolve_relative_tag(
        std::meta::info tag_info
    ) -> std::meta::info;

    consteval auto resolve_semantic_kind(
        std::meta::info tag_info
    ) -> quantity_semantic_kind;

    template <typename Tag>
    struct quantity_tag_traits {
        using parent_tag = [: resolve_parent_tag(^^Tag) :];
        using unit_tag = [: resolve_unit_family_tag(^^Tag) :];
        using relative_tag = [: resolve_relative_tag(^^Tag) :];
        static constexpr quantity_semantic_kind semantic_kind = resolve_semantic_kind(^^Tag);
    };

    template <typename Dim>
    struct dimension_to_tag {
        static constexpr bool found = false;
    };

    template <typename Tag, typename T = float, auto U = ([: resolve_default_unit_info(^^Tag) :])>
    using quantity_t = typename quantity_traits<Tag>::template type<T, U>;

    export template <typename T>
    concept is_arithmetic = std::integral<T> || std::floating_point<T>;

    constexpr auto cexpr_llround(const long double x) -> long long {
        return x >= 0 ? static_cast<long long>(x + 0.5L) : static_cast<long long>(x - 0.5L);
    }
}

consteval auto gse::internal::quantity_spec_type_of(std::meta::info tag_info) -> std::meta::info {
    for (auto ann : std::meta::annotations_of(tag_info)) {
        auto t = std::meta::type_of(ann);
        if (!std::meta::has_template_arguments(t)) {
            continue;
        }
        auto tmpl = std::meta::template_of(t);
        if (tmpl == ^^quantity_root_spec ||
            tmpl == ^^quantity_child_spec ||
            tmpl == ^^quantity_sub_root_spec ||
            tmpl == ^^quantity_absolute_spec) {
            return t;
        }
    }
    return std::meta::info{};
}

consteval auto gse::internal::has_quantity_spec(std::meta::info tag_info) -> bool {
    return quantity_spec_type_of(tag_info) != std::meta::info{};
}

consteval auto gse::internal::classify_spec(std::meta::info spec_t) -> spec_kind {
    if (spec_t == std::meta::info{}) {
        return spec_kind::none;
    }
    auto tmpl = std::meta::template_of(spec_t);
    if (tmpl == ^^quantity_root_spec) {
        return spec_kind::root;
    }
    if (tmpl == ^^quantity_child_spec) {
        return spec_kind::child;
    }
    if (tmpl == ^^quantity_sub_root_spec) {
        return spec_kind::sub_root;
    }
    if (tmpl == ^^quantity_absolute_spec) {
        return spec_kind::absolute;
    }
    return spec_kind::none;
}

consteval auto gse::internal::resolve_dim_info(std::meta::info tag_info) -> std::meta::info {
    auto spec_t = quantity_spec_type_of(tag_info);
    auto kind = classify_spec(spec_t);
    if (kind == spec_kind::root) {
        return std::meta::extract<std::meta::info>(
            std::meta::template_arguments_of(spec_t)[0]
        );
    }
    if (kind == spec_kind::child || kind == spec_kind::sub_root || kind == spec_kind::absolute) {
        auto parent_info = std::meta::extract<std::meta::info>(
            std::meta::template_arguments_of(spec_t)[0]
        );
        return resolve_dim_info(parent_info);
    }
    return std::meta::info{};
}

consteval auto gse::internal::resolve_default_unit_info(std::meta::info tag_info) -> std::meta::info {
    auto spec_t = quantity_spec_type_of(tag_info);
    auto kind = classify_spec(spec_t);
    if (kind == spec_kind::root) {
        auto args = std::meta::template_arguments_of(spec_t);
        std::array<std::meta::info, 3> unit_args = { tag_info, args[2], args[3] };
        auto unit_t = std::meta::substitute(^^unit, unit_args);
        std::array<std::meta::info, 1> holder_args = { std::meta::reflect_constant(unit_t) };
        return std::meta::substitute(^^default_unit_for_type, holder_args);
    }
    if (kind == spec_kind::child || kind == spec_kind::sub_root || kind == spec_kind::absolute) {
        auto parent_info = std::meta::extract<std::meta::info>(
            std::meta::template_arguments_of(spec_t)[0]
        );
        return resolve_default_unit_info(parent_info);
    }
    return std::meta::info{};
}

consteval auto gse::internal::resolve_parent_tag(std::meta::info tag_info) -> std::meta::info {
    auto spec_t = quantity_spec_type_of(tag_info);
    auto kind = classify_spec(spec_t);
    if (kind == spec_kind::child || kind == spec_kind::sub_root || kind == spec_kind::absolute) {
        return std::meta::extract<std::meta::info>(
            std::meta::template_arguments_of(spec_t)[0]
        );
    }
    return ^^no_parent_quantity_tag;
}

consteval auto gse::internal::resolve_unit_family_tag(std::meta::info tag_info) -> std::meta::info {
    auto spec_t = quantity_spec_type_of(tag_info);
    auto kind = classify_spec(spec_t);
    if (kind == spec_kind::root) {
        return tag_info;
    }
    if (kind == spec_kind::child || kind == spec_kind::sub_root || kind == spec_kind::absolute) {
        auto parent_info = std::meta::extract<std::meta::info>(
            std::meta::template_arguments_of(spec_t)[0]
        );
        return resolve_unit_family_tag(parent_info);
    }
    return tag_info;
}

consteval auto gse::internal::resolve_relative_tag(std::meta::info tag_info) -> std::meta::info {
    auto spec_t = quantity_spec_type_of(tag_info);
    auto kind = classify_spec(spec_t);
    if (kind == spec_kind::root || kind == spec_kind::sub_root) {
        return tag_info;
    }
    if (kind == spec_kind::child) {
        auto parent_info = std::meta::extract<std::meta::info>(
            std::meta::template_arguments_of(spec_t)[0]
        );
        return resolve_relative_tag(parent_info);
    }
    if (kind == spec_kind::absolute) {
        return std::meta::extract<std::meta::info>(
            std::meta::template_arguments_of(spec_t)[1]
        );
    }
    return tag_info;
}

consteval auto gse::internal::resolve_semantic_kind(std::meta::info tag_info) -> quantity_semantic_kind {
    auto spec_t = quantity_spec_type_of(tag_info);
    auto kind = classify_spec(spec_t);
    if (kind == spec_kind::root || kind == spec_kind::child) {
        return std::meta::extract<quantity_semantic_kind>(
            std::meta::template_arguments_of(spec_t)[1]
        );
    }
    if (kind == spec_kind::sub_root) {
        return quantity_semantic_kind::relative;
    }
    if (kind == spec_kind::absolute) {
        return quantity_semantic_kind::absolute;
    }
    return quantity_semantic_kind::measurement;
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
    return quantity_template::template type<T>::template from<unit>(value);
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

    template <typename Tag>
    using normalized_tag_t = std::remove_cvref_t<Tag>;

    template <typename Tag>
    using parent_quantity_tag_t = quantity_tag_traits<normalized_tag_t<Tag>>::parent_tag;

    template <typename Tag>
    using unit_family_tag_t = quantity_tag_traits<normalized_tag_t<Tag>>::unit_tag;

    template <typename Tag>
    using relative_quantity_tag_t = quantity_tag_traits<normalized_tag_t<Tag>>::relative_tag;

    template <typename Tag>
    inline constexpr quantity_semantic_kind semantic_kind_v = quantity_tag_traits<normalized_tag_t<Tag>>::semantic_kind;

    template <typename Tag>
    inline constexpr bool is_generic_tag_v = std::same_as<normalized_tag_t<Tag>, generic_quantity_tag>;

    template <typename Tag1, typename Tag2>
    inline constexpr bool same_unit_family_v =
        is_generic_tag_v<Tag1> ||
        is_generic_tag_v<Tag2> ||
        std::same_as<unit_family_tag_t<Tag1>, unit_family_tag_t<Tag2>>;

    template <typename T>
    inline constexpr bool dependent_false_v = false;

    template <typename AncestorTag, typename DescendantTag>
    consteval auto is_same_or_ancestor_tag() -> bool {
        using ancestor = normalized_tag_t<AncestorTag>;
        using descendant = normalized_tag_t<DescendantTag>;

        if constexpr (std::same_as<ancestor, descendant>) {
            return true;
        }
        else if constexpr (std::same_as<descendant, no_parent_quantity_tag>) {
            return false;
        }
        else if constexpr (std::same_as<parent_quantity_tag_t<descendant>, no_parent_quantity_tag>) {
            return false;
        }
        else {
            return is_same_or_ancestor_tag<ancestor, parent_quantity_tag_t<descendant>>();
        }
    }

    template <typename Tag1, typename Tag2>
    consteval auto common_ancestor_tag_fn() {
        using t1 = normalized_tag_t<Tag1>;
        using t2 = normalized_tag_t<Tag2>;

        if constexpr (std::same_as<t1, t2>) {
            return std::type_identity<t1>{};
        }
        else if constexpr (is_same_or_ancestor_tag<t1, t2>()) {
            return std::type_identity<t1>{};
        }
        else if constexpr (is_same_or_ancestor_tag<t2, t1>()) {
            return std::type_identity<t2>{};
        }
        else if constexpr (std::same_as<parent_quantity_tag_t<t1>, no_parent_quantity_tag>) {
            return std::type_identity<unit_family_tag_t<t1>>{};
        }
        else {
            return common_ancestor_tag_fn<parent_quantity_tag_t<t1>, t2>();
        }
    }

    template <typename Tag1, typename Tag2>
    using common_ancestor_tag_t = decltype(common_ancestor_tag_fn<Tag1, Tag2>())::type;

    template <typename Tag1, typename Tag2>
    consteval auto addition_result_tag_fn() {
        using t1 = normalized_tag_t<Tag1>;
        using t2 = normalized_tag_t<Tag2>;

        if constexpr (is_generic_tag_v<t1>) {
            return std::type_identity<t2>{};
        }
        else if constexpr (is_generic_tag_v<t2>) {
            return std::type_identity<t1>{};
        }
        else {
            constexpr auto k1 = semantic_kind_v<t1>;
            constexpr auto k2 = semantic_kind_v<t2>;

            if constexpr (k1 == quantity_semantic_kind::absolute && k2 == quantity_semantic_kind::relative) {
                return std::type_identity<t1>{};
            }
            else if constexpr (k1 == quantity_semantic_kind::relative && k2 == quantity_semantic_kind::absolute) {
                return std::type_identity<t2>{};
            }
            else if constexpr (k1 == quantity_semantic_kind::absolute && k2 == quantity_semantic_kind::absolute) {
                static_assert(dependent_false_v<t1>, "Cannot add two absolute quantities");
            }
            else if constexpr (k1 == quantity_semantic_kind::relative && k2 == quantity_semantic_kind::relative) {
                return std::type_identity<relative_quantity_tag_t<common_ancestor_tag_t<t1, t2>>>{};
            }
            else {
                return std::type_identity<common_ancestor_tag_t<t1, t2>>{};
            }
        }
    }

    template <typename Tag1, typename Tag2>
    using addition_result_tag_t = decltype(addition_result_tag_fn<Tag1, Tag2>())::type;

    template <typename Tag1, typename Tag2>
    consteval auto subtraction_result_tag_fn() {
        using t1 = normalized_tag_t<Tag1>;
        using t2 = normalized_tag_t<Tag2>;

        if constexpr (is_generic_tag_v<t1>) {
            return std::type_identity<t2>{};
        }
        else if constexpr (is_generic_tag_v<t2>) {
            return std::type_identity<t1>{};
        }
        else {
            constexpr auto k1 = semantic_kind_v<t1>;
            constexpr auto k2 = semantic_kind_v<t2>;

            if constexpr (k1 == quantity_semantic_kind::absolute && k2 == quantity_semantic_kind::absolute) {
                return std::type_identity<relative_quantity_tag_t<common_ancestor_tag_t<t1, t2>>>{};
            }
            else if constexpr (k1 == quantity_semantic_kind::absolute && k2 == quantity_semantic_kind::relative) {
                return std::type_identity<t1>{};
            }
            else if constexpr (k1 == quantity_semantic_kind::relative && k2 == quantity_semantic_kind::absolute) {
                static_assert(dependent_false_v<t1>, "Cannot subtract an absolute quantity from a relative quantity");
            }
            else if constexpr (k1 == quantity_semantic_kind::relative && k2 == quantity_semantic_kind::relative) {
                return std::type_identity<relative_quantity_tag_t<common_ancestor_tag_t<t1, t2>>>{};
            }
            else {
                return std::type_identity<common_ancestor_tag_t<t1, t2>>{};
            }
        }
    }

    template <typename Tag1, typename Tag2>
    using subtraction_result_tag_t = decltype(subtraction_result_tag_fn<Tag1, Tag2>())::type;

    template <typename Tag, typename ValueType>
    using quantity_for_tag_t = quantity_traits<Tag>::template type<ValueType>;

    template <typename UnitType, typename QuantityType>
	concept valid_unit_for_quantity =
        same_unit_family_v<typename UnitType::quantity_tag, typename QuantityType::quantity_tag>;

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
                same_unit_family_v<QuantityTagType, Tag2>
        explicit(!is_generic_tag_v<Tag2> &&
            (is_generic_tag_v<QuantityTagType> || semantic_kind_v<QuantityTagType> != semantic_kind_v<Tag2>))
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

        template <is_arithmetic T2, is_dimension Dim2, typename Tag2, typename Unit2>
            requires has_same_dimensions<Dimensions, Dim2> &&
                same_unit_family_v<QuantityTagType, Tag2>
        constexpr auto operator<=>(const quantity<T2, Dim2, Tag2, Unit2>& other) const {
            using common_t = std::common_type_t<ArithmeticType, T2>;
            const auto lhs = static_cast<common_t>(this->as<DefaultUnitType>());
            const auto rhs = static_cast<common_t>(other.template as<DefaultUnitType>());
            return lhs <=> rhs;
        }

        template <is_arithmetic T2, is_dimension Dim2, typename Tag2, typename Unit2>
            requires has_same_dimensions<Dimensions, Dim2> &&
                same_unit_family_v<QuantityTagType, Tag2>
        constexpr auto operator==(const quantity<T2, Dim2, Tag2, Unit2>& other) const -> bool {
            return ((*this <=> other) == 0);
        }
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

namespace gse::internal {
    template <typename Units, typename Fn>
    constexpr auto dispatch_named_unit(const Units& units, std::string_view name, Fn&& fn) -> bool {
        return std::apply([&](const auto&... u) {
            bool found = false;
            auto try_match = [&](const auto& unit_const) {
                if (!found && name == std::string_view(std::remove_cvref_t<decltype(unit_const)>::unit_name)) {
                    fn(unit_const);
                    found = true;
                }
            };
            (try_match(u), ...);
            return found;
        }, units);
    }

    export template <typename Tag>
    struct quantity_units;

    template <typename Tag>
    concept has_unit_list = requires {
        quantity_units<Tag>::units;
    };
}

template <typename A, typename Dim, typename Tag, typename Unit, typename CharT>
struct std::formatter<gse::internal::quantity<A, Dim, Tag, Unit>, CharT> {
    std::formatter<A, CharT> value_fmt;
    std::basic_string_view<CharT> unit_override;

    template <class ParseContext>
    constexpr auto parse(ParseContext& ctx) -> ParseContext::iterator {
        auto begin = ctx.begin();
        auto end = ctx.end();

        auto value_spec_end = begin;
        while (value_spec_end != end && *value_spec_end != ':' && *value_spec_end != '}') {
            ++value_spec_end;
        }

        std::basic_string_view<CharT> value_spec(begin, value_spec_end);
        std::basic_format_parse_context<CharT> sub(value_spec);
        value_fmt.parse(sub);

        if (value_spec_end != end && *value_spec_end == ':') {
            auto unit_start = value_spec_end + 1;
            auto unit_end = unit_start;
            while (unit_end != end) {
                const auto c = *unit_end;
                const bool is_alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
                if (!is_alpha && c != '^' && c != '_' && c != '/') {
                    break;
                }
                ++unit_end;
            }
            unit_override = std::basic_string_view<CharT>(unit_start, unit_end);
            return unit_end;
        }
        return value_spec_end;
    }

    template <typename FormatContext>
    auto format(const gse::internal::quantity<A, Dim, Tag, Unit>& q, FormatContext& ctx) const {
        auto it = ctx.out();

        auto format_default = [&] {
            it = value_fmt.format(q.template as<Unit>(), ctx);
            if constexpr (!std::same_as<Unit, gse::internal::no_default_unit>) {
                it = std::ranges::copy(std::string_view{ " " }, it).out;
                it = std::ranges::copy(std::string_view{ Unit::unit_name }, it).out;
            }
        };

        if (unit_override.empty()) {
            format_default();
            return it;
        }

        bool dispatched = false;
        if constexpr (gse::internal::has_unit_list<Tag>) {
            dispatched = gse::internal::dispatch_named_unit(
                gse::internal::quantity_units<Tag>::units,
                unit_override,
                [&](auto unit_const) {
                    using U = std::remove_cvref_t<decltype(unit_const)>;
                    it = value_fmt.format(q.template as<U>(), ctx);
                    it = std::ranges::copy(std::string_view{ " " }, it).out;
                    it = std::ranges::copy(std::string_view{ U::unit_name }, it).out;
                }
            );
        }

        if (!dispatched) {
            format_default();
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

    template <typename Tag>
        requires (has_quantity_spec(^^Tag))
    struct quantity_traits<Tag> {
        static constexpr auto dim_info = resolve_dim_info(^^Tag);
        static constexpr auto default_unit_info = resolve_default_unit_info(^^Tag);

        template <
            typename T,
            auto U = [: default_unit_info :]
        >
        using type = quantity<
            T,
            typename [: dim_info :],
            Tag,
            decltype(U)
        >;
    };

    template <bool IsGeneric, typename Tag, typename ValueType, typename Dim>
    struct lazy_quantity_for_tag {
        using type = quantity_for_tag_t<Tag, ValueType>;
    };

    template <typename Tag, typename ValueType, typename Dim>
    struct lazy_quantity_for_tag<true, Tag, ValueType, Dim> {
        using type = generic_quantity<ValueType, Dim>;
    };

    template <is_quantity Q1, is_quantity Q2>
    using addition_result_t = lazy_quantity_for_tag<
        is_generic_tag_v<addition_result_tag_t<typename Q1::quantity_tag, typename Q2::quantity_tag>>,
        addition_result_tag_t<typename Q1::quantity_tag, typename Q2::quantity_tag>,
        std::common_type_t<typename Q1::value_type, typename Q2::value_type>,
        typename Q1::dimension
    >::type;

    template <is_quantity Q1, is_quantity Q2>
    using subtraction_result_t = lazy_quantity_for_tag<
        is_generic_tag_v<subtraction_result_tag_t<typename Q1::quantity_tag, typename Q2::quantity_tag>>,
        subtraction_result_tag_t<typename Q1::quantity_tag, typename Q2::quantity_tag>,
        std::common_type_t<typename Q1::value_type, typename Q2::value_type>,
        typename Q1::dimension
    >::type;

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
	) -> addition_result_t<Q1, Q2>;

	template <is_quantity Q1, is_quantity Q2>
		requires has_same_dimension_as<Q1, Q2>
	constexpr auto operator-(
		const Q1& lhs,
		const Q2& rhs
	) -> subtraction_result_t<Q1, Q2>;

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
}

template <gse::internal::is_quantity Q1, gse::internal::is_quantity Q2>
	requires gse::internal::has_same_dimension_as<Q1, Q2>
constexpr auto gse::internal::operator+(const Q1& lhs, const Q2& rhs) -> addition_result_t<Q1, Q2> {
    using result_type = addition_result_t<Q1, Q2>;
	return result_type(lhs.template as<typename result_type::default_unit>() + rhs.template as<typename result_type::default_unit>());
}

template <gse::internal::is_quantity Q1, gse::internal::is_quantity Q2>
	requires gse::internal::has_same_dimension_as<Q1, Q2>
constexpr auto gse::internal::operator-(const Q1& lhs, const Q2& rhs) -> subtraction_result_t<Q1, Q2> {
    using result_type = subtraction_result_t<Q1, Q2>;
	return result_type(lhs.template as<typename result_type::default_unit>() - rhs.template as<typename result_type::default_unit>());
}

template <gse::internal::is_quantity Q1, gse::internal::is_quantity Q2>
constexpr auto gse::internal::operator*(const Q1& lhs, const Q2& rhs) {
	using result_v = std::common_type_t<typename Q1::value_type, typename Q2::value_type>;
	using result_d = decltype(typename Q1::dimension() * typename Q2::dimension());
	if constexpr (dimension_to_tag<result_d>::found) {
		using found_tag = typename dimension_to_tag<result_d>::tag;
		using result_t = typename quantity_traits<found_tag>::template type<result_v>;
		return result_t(lhs.template as<typename Q1::default_unit>() * rhs.template as<typename Q2::default_unit>());
	}
	else {
		return generic_quantity<result_v, result_d>(lhs.template as<typename Q1::default_unit>() * rhs.template as<typename Q2::default_unit>());
	}
}

template <gse::internal::is_quantity Q, gse::internal::is_arithmetic S>
constexpr auto gse::internal::operator*(const Q& lhs, const S& rhs) -> Q {
	return Q(lhs.template as<typename Q::default_unit>() * static_cast<typename Q::value_type>(rhs));
}

template <gse::internal::is_arithmetic S, gse::internal::is_quantity Q>
constexpr auto gse::internal::operator*(const S& lhs, const Q& rhs) -> Q {
	return Q(static_cast<typename Q::value_type>(lhs) * rhs.template as<typename Q::default_unit>());
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
	if constexpr (dimension_to_tag<result_d>::found) {
		using found_tag = typename dimension_to_tag<result_d>::tag;
		using result_t = typename quantity_traits<found_tag>::template type<result_v>;
		return result_t(lhs.template as<typename Q1::default_unit>() / rhs.template as<typename Q2::default_unit>());
	}
	else {
		return generic_quantity<result_v, result_d>(lhs.template as<typename Q1::default_unit>() / rhs.template as<typename Q2::default_unit>());
	}
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
	requires gse::internal::has_same_dimension_as<Q1, Q2>
constexpr auto gse::internal::operator+=(Q1& lhs, const Q2& rhs) -> Q1& {
    lhs = Q1(lhs + rhs);
	return lhs;
}

template <gse::internal::is_quantity Q1, gse::internal::is_quantity Q2>
	requires gse::internal::has_same_dimension_as<Q1, Q2>
constexpr auto gse::internal::operator-=(Q1& lhs, const Q2& rhs) -> Q1& {
    lhs = Q1(lhs - rhs);
	return lhs;
}

template <gse::internal::is_quantity Q, gse::internal::is_arithmetic S>
constexpr auto gse::internal::operator*=(Q& lhs, const S& rhs) -> Q& {
	lhs.template set<typename Q::default_unit>(lhs.template as<typename Q::default_unit>() * rhs);
	return lhs;
}

template <gse::internal::is_quantity Q, gse::internal::is_arithmetic S>
constexpr auto gse::internal::operator/=(Q& lhs, const S& rhs) -> Q& {
	lhs.template set<typename Q::default_unit>(lhs.template as<typename Q::default_unit>() / rhs);
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

    template <internal::is_quantity Q>
    constexpr auto sin(const Q& q) -> Q::value_type {
        return std::sin(q.template as<typename Q::default_unit>());
    }

    template <internal::is_quantity Q>
    constexpr auto cos(const Q& q) -> Q::value_type {
        return std::cos(q.template as<typename Q::default_unit>());
    }

    template <internal::is_quantity Q>
    constexpr auto tan(const Q& q) -> Q::value_type {
        return std::tan(q.template as<typename Q::default_unit>());
    }

    template <internal::is_quantity Q>
    constexpr auto asin(const typename Q::value_type v) -> Q {
        return Q(std::asin(v));
    }

    template <internal::is_quantity Q>
    constexpr auto acos(const typename Q::value_type v) -> Q {
        return Q(std::acos(v));
    }

    template <internal::is_quantity Q>
    constexpr auto atan2(const typename Q::value_type y, const typename Q::value_type x) -> Q {
        return Q(std::atan2(y, x));
    }

    template <internal::is_quantity Q>
    constexpr auto floor(const Q& q) -> Q {
        return Q(std::floor(q.template as<typename Q::default_unit>()));
    }
}

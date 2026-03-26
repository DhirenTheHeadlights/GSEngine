export module gse.math:unit_display;

import std;

import :quant;
import :vector;

export namespace gse {
    template <auto Unit, typename T>
    struct unit_display {
        T value;
    };

    template <auto Unit, typename T> requires internal::is_unit<decltype(Unit)>
    constexpr auto in(const T& val) -> unit_display<Unit, T> { return { val }; }
}

template <auto Unit, typename A, typename Dim, typename Tag, typename DefUnit, typename CharT>
struct std::formatter<gse::unit_display<Unit, gse::internal::quantity<A, Dim, Tag, DefUnit>>, CharT> {
    std::formatter<A, CharT> value_fmt;

    template <class ParseContext>
    constexpr auto parse(ParseContext& ctx) -> ParseContext::iterator {
        return value_fmt.parse(ctx);
    }

    template <typename FormatContext>
    auto format(const gse::unit_display<Unit, gse::internal::quantity<A, Dim, Tag, DefUnit>>& ud, FormatContext& ctx) const {
        auto it = value_fmt.format(ud.value.template as<decltype(Unit)>(), ctx);
        it = std::ranges::copy(std::string_view{ " " }, it).out;
        it = std::ranges::copy(std::string_view{ decltype(Unit)::unit_name }, it).out;
        return it;
    }
};

template <auto Unit, gse::internal::is_arithmetic_wrapper Q, std::size_t N, typename CharT>
struct std::formatter<gse::unit_display<Unit, gse::vec<Q, N>>, CharT> {
    std::formatter<gse::unit_display<Unit, Q>, CharT> elem_;

    template <class ParseContext>
    constexpr auto parse(ParseContext& ctx) -> ParseContext::iterator {
        return elem_.parse(ctx);
    }

    template <class FormatContext>
    auto format(const gse::unit_display<Unit, gse::vec<Q, N>>& ud, FormatContext& ctx) const -> FormatContext::iterator {
        auto out = ctx.out();
        out = std::format_to(out, "(");
        for (std::size_t i = 0; i < N; ++i) {
            if (i > 0) {
                out = std::format_to(out, ", ");
            }
            out = elem_.format(gse::unit_display<Unit, Q>{ ud.value[i] }, ctx);
        }
        return std::format_to(out, ")");
    }
};

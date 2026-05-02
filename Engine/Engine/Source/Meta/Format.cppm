export module gse.meta:format;

import std;
import gse.std_meta;

import :annotations;

export template <typename T, typename CharT>
    requires (std::is_class_v<T> && !std::is_polymorphic_v<T> && std::is_same_v<CharT, char>)
struct std::formatter<T, CharT> {
    constexpr auto parse(auto& ctx) {
        return ctx.begin();
    }

    auto format(const T& value, auto& ctx) const {
        auto out = std::format_to(ctx.out(), "{{");
        bool first = true;
        template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
            if constexpr (!gse::has_annotation<gse::format_skip_tag>(m)) {
                if (!first) {
                    out = std::format_to(out, ", ");
                }
                first = false;
                out = std::format_to(out, "{}={}", std::meta::identifier_of(m), value.[:m:]);
            }
        }
        return std::format_to(out, "}}");
    }
};

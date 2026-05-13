export module gse.assert;

import std;

import gse.log;
import gse.stacktrace;

export namespace gse {
    template <typename... Args>
    struct fmt_loc {
        std::format_string<Args...> fmt;
        std::source_location loc;

        template <typename T> requires std::convertible_to<const T&, std::string_view>
        consteval fmt_loc(
            const T& s,
            std::source_location l = std::source_location::current()
        ) : fmt(s), loc(l) {}
    };

    template <typename... Args>
    auto assert(
        bool condition,
        fmt_loc<std::type_identity_t<Args>...> f,
        Args&&... args
    ) -> void;

    template <typename... Args>
    auto assert(
        bool condition,
        std::source_location loc,
        std::format_string<std::type_identity_t<Args>...> fmt,
        Args&&... args
    ) -> void;
}

namespace gse {
    auto assert_format_message(
        std::source_location loc,
        std::string_view comment
    ) -> std::string;

    auto assert_fail(
        std::string_view message
    ) noexcept -> void;
}

template <class... Args>
auto gse::assert(const bool condition, fmt_loc<std::type_identity_t<Args>...> f, Args&&... args) -> void {
    if (condition) {
        return;
    }

    const std::string comment = std::format(f.fmt, std::forward<Args>(args)...);
    assert_fail(assert_format_message(f.loc, comment));
}

template <class... Args>
auto gse::assert(const bool condition, const std::source_location loc, std::format_string<std::type_identity_t<Args>...> fmt, Args&&... args) -> void {
    if (condition) {
        return;
    }

    const std::string comment = std::format(fmt, std::forward<Args>(args)...);
    assert_fail(assert_format_message(loc, comment));
}

auto gse::assert_format_message(const std::source_location loc, const std::string_view comment) -> std::string {
    return std::format(
        "[Assertion Failure]\n"
        "File: {}\n"
        "Line: {}\n"
        "Function: {}\n"
        "Comment: {}\n"
        "Stack:\n{}",
        loc.file_name(),
        loc.line(),
        loc.function_name(),
        comment,
        capture_stacktrace(2)
    );
}

auto gse::assert_fail(const std::string_view message) noexcept -> void {
    log::println(
        log::level::error,
        log::category::general,
        "{}",
        message
    );
    log::flush();
    std::terminate();
}

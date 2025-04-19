export module gse.platform.assert;

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

import <csignal>;
import <cstdio>;
import <format>;
import <iostream>;
import <source_location>;
import <string>;
import <string_view>;
#ifdef _WIN32
#define NOMINMAX
import <Windows.h>;
#endif

export namespace gse {
    template <typename... Args>
    auto assert(
        bool condition,
        std::format_string<Args...> fmt,
        Args&&... args
    ) -> void;

    template <typename... Args>
    auto assert(
        bool condition,
        std::source_location loc,
        std::format_string<Args...> fmt,
        Args&&... args
    ) -> void;
}

namespace gse {
    auto assert_func_production(
        char const* expression,
        char const* file_name,
        unsigned    line_number,
        char const* comment
    ) -> void;

    auto assert_func_internal(
        char const* expression,
        char const* file_name,
        unsigned    line_number,
        char const* comment
    ) -> void;

    template <typename... Args>
    auto assert(
        bool condition,
        std::format_string<Args...> fmt,
        Args&&... args
    ) -> void {
        assert(
            condition,
            std::source_location::current(),
            fmt,
            std::forward<Args>(args)...
        );
    }

    template <typename... Args>
    auto assert(
        const bool condition,
        const std::source_location loc,
        std::format_string<Args...> fmt,
        Args&&... args
    ) -> void {
        if (!condition) {
            auto msg = std::format(fmt, std::forward<Args>(args)...);
            assert_func_internal(
                msg.c_str(),
                loc.file_name(),
                loc.line(),
                "---"
            );
        }
    }

    auto assert_func_production(
        char const* expression,
        char const* file_name,
        unsigned    line_number,
        char const* comment
    ) -> void {
#ifdef _WIN32
        char message[1024];
        std::snprintf(message, sizeof(message),
            "Assertion failed (Production)\n\n"
            "File: %s\n"
            "Line: %u\n"
            "Expression: %s\n"
            "Comment: %s\n\n"
            "Please report this error to the developer.",
            file_name, line_number, expression, comment);

        MessageBoxA(nullptr, message, "GSEngine",
            MB_TASKMODAL | MB_ICONHAND | MB_OK | MB_SETFOREGROUND);

        std::raise(SIGABRT);
        _exit(3);
#else
        std::cerr << "[Production Assertion Failure]\n"
            << "File: " << file_name << "\n"
            << "Line: " << line_number << "\n"
            << "Expression: " << expression << "\n"
            << "Comment: " << comment << "\n";
        std::raise(SIGABRT);
#endif
    }

    auto assert_func_internal(
        char const* expression,
        char const* file_name,
        unsigned    line_number,
        char const* comment
    ) -> void {
#ifdef _WIN32
        char message[1024];
        std::snprintf(message, sizeof(message),
            "Assertion failed (Internal)\n\n"
            "File: %s\n"
            "Line: %u\n"
            "Expression: %s\n"
            "Comment: %s\n\n"
            "Press Retry to debug.",
            file_name, line_number, expression, comment);

        const int action = ::MessageBoxA(nullptr, message, "Internal Assert",
            MB_TASKMODAL | MB_ICONHAND | MB_ABORTRETRYIGNORE | MB_SETFOREGROUND);

        switch (action) {
        case IDABORT:  std::raise(SIGABRT); _exit(3); break;
        case IDRETRY:  __debugbreak();                break;
        case IDIGNORE:                             break;
        default:      std::abort();                break;
        }
#else
        std::cerr << "[Internal Assertion Failure]\n"
            << "File: " << file_name << "\n"
            << "Line: " << line_number << "\n"
            << "Expression: " << expression << "\n"
            << "Comment: " << comment << "\n";
        std::raise(SIGABRT);
#endif
    }
}

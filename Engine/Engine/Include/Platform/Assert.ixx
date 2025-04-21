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
    auto assert(
        bool condition,
		std::string_view formatted_string,
		const std::source_location& loc = std::source_location::current()
    ) -> void;
}

namespace gse {
	auto assert_func_production(char const* message) -> void;
	auto assert_func_internal(char const* message) -> void;
}

auto gse::assert(
    const bool condition,
    std::string_view formatted_string,
    const std::source_location& loc
) -> void {
    if (!condition) {
        const std::string message = std::format(
            "[Assertion Failure]\n"
            "File: {}\n"
            "Line: {}\n"
            "Expression: {}\n"
            "Comment: {}\n",
            loc.file_name(), loc.line(), formatted_string, loc.function_name()
        );

        assert_func_internal(message.c_str());
    }
}

auto gse::assert_func_production(char const* message) -> void {
#ifdef _WIN32
    MessageBoxA(nullptr, message, "GSEngine",
        MB_TASKMODAL | MB_ICONHAND | MB_OK | MB_SETFOREGROUND);

    std::raise(SIGABRT);
    _exit(3);
#else
    std::cerr << message << "\n";
    std::raise(SIGABRT);
#endif
}

auto gse::assert_func_internal(char const* message) -> void {
#ifdef _WIN32
    const int action = ::MessageBoxA(nullptr, message, "Internal Assert",
        MB_TASKMODAL | MB_ICONHAND | MB_ABORTRETRYIGNORE | MB_SETFOREGROUND);

    switch (action) {
    case IDABORT:  std::raise(SIGABRT); _exit(3); break;
    case IDRETRY:  __debugbreak();                break;
    case IDIGNORE:                                break;
    default:      std::abort();                   break;
    }
#else
    std::cerr << message << "\n";
    std::raise(SIGABRT);
#endif
}

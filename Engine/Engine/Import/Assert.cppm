export module gse.assert;

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifdef _WIN32
#define NOMINMAX
import <Windows.h>;
#endif

import std;

export namespace gse {
	template <typename... Args>
    auto assert(
	    bool condition,
        std::source_location loc = std::source_location::current(),
		std::format_string<Args...> fmt = "{}",
	    Args&&... args
    ) -> void;
}

namespace gse {
    auto assert_func_production(
        std::string_view message
    ) noexcept -> void;

    auto assert_func_internal(
        std::string_view message
    ) noexcept -> void;
}

template <class... Args>
auto gse::assert(const bool condition, const std::source_location loc, std::format_string<Args...> fmt, Args&&... args) -> void {
    if (condition) return;

	const std::string comment = std::format(fmt, std::forward<Args>(args)...);
    const std::string message = std::format(
        "[Assertion Failure]\n"
        "File: {}\n"
        "Line: {}\n"
        "Function: {}\n"
        "Comment: {}\n",
        loc.file_name(),
        loc.line(),
        loc.function_name(),
        comment
    );

    assert_func_internal(message);
}

auto gse::assert_func_production(const std::string_view message) noexcept -> void {
#ifdef _WIN32
    MessageBoxA(nullptr, message.data(), "GSEngine",
        MB_TASKMODAL | MB_ICONHAND | MB_OK | MB_SETFOREGROUND);
#endif
    std::terminate();
}

auto gse::assert_func_internal(std::string_view message) noexcept -> void {
#ifdef _WIN32
    const int action = MessageBoxA(
        nullptr,
        message.data(),
        "Internal Assert",
        MB_TASKMODAL | MB_ICONHAND | MB_ABORTRETRYIGNORE | MB_SETFOREGROUND
    );

    switch (action) {
    case IDABORT:
        std::terminate();
    case IDRETRY:
        __debugbreak();
        break;
    case IDIGNORE:
        break;
    default:
        std::terminate();
    }
#else
	std::println("Internal Assert: {}", message);
    std::terminate();
#endif
}

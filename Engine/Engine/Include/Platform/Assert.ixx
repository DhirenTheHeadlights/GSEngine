export module gse.platform.perma_assert;

// --- Standard/Compiler Headers ---
#ifdef _MSC_VER
  // For MSVC, optionally disable "unsafe" warnings
#define _CRT_SECURE_NO_WARNINGS
#endif

import <csignal>;   // For raise, SIGABRT
import <cstdio>;    // For snprintf
import <iostream>;
import <string>;

// Optional: If Windows-specific features are needed
#ifdef _WIN32
#define NOMINMAX
import <Windows.h>;
#endif

// ------------------------------------------------------
// Forward-Declarations of Underlying Assert Functions
// ------------------------------------------------------
inline auto assert_func_production(
	char const* expression,
	char const* file_name,
	unsigned line_number,
	char const* comment = "---") -> void;

inline auto assert_func_internal(
	char const* expression,
	char const* file_name,
	unsigned line_number,
	char const* comment = "---") -> void;

// ------------------------------------------------------
// Windows vs. Non-Windows Implementations
// ------------------------------------------------------
inline auto assert_func_production(
	char const* expression,
	char const* file_name,
	unsigned line_number,
	char const* comment) -> void {
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

    ::MessageBoxA(nullptr, message, "Platform Layer",
        MB_TASKMODAL | MB_ICONHAND | MB_OK | MB_SETFOREGROUND);

    std::raise(SIGABRT);
    _exit(3);   // Typically not reached, but just in case
#else
    // Non-Windows production assertion
    std::cerr << "[Production Assertion Failure]\n"
        << "File: " << file_name << "\n"
        << "Line: " << line_number << "\n"
        << "Expression: " << expression << "\n"
        << "Comment: " << comment << "\n";
    std::raise(SIGABRT);
#endif
}

inline auto assert_func_internal(
	char const* expression,
	char const* file_name,
	unsigned line_number,
	char const* comment) -> void {
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
        MB_TASKMODAL | MB_ICONHAND |
        MB_ABORTRETRYIGNORE | MB_SETFOREGROUND);

    switch (action) {
    case IDABORT:
        std::raise(SIGABRT);
        _exit(3);
    case IDRETRY:
        __debugbreak();
        break;
    case IDIGNORE:
        // do nothing and continue
        break;
    default:
        std::abort();
    }
#else
    // Non-Windows internal assertion
    std::cerr << "[Internal Assertion Failure]\n"
        << "File: " << file_name << "\n"
        << "Line: " << line_number << "\n"
        << "Expression: " << expression << "\n"
        << "Comment: " << comment << "\n";
    std::raise(SIGABRT);
#endif
}

// ------------------------------------------------------
// Exported Inline "Assert" Functions
// (Replacing macros with inline C++ functions)
// ------------------------------------------------------
#if defined(PRODUCTION_BUILD) && (PRODUCTION_BUILD != 0)

// Production build: calls production asserts
export void perma_assert(
    bool        condition,
    char const* expression,
    char const* file_name = "---",
    unsigned    line_number = 0,
    char const* comment = "---")
{
    if (!condition) {
        assert_func_production(expression, file_name, line_number, comment);
    }
}

export void assert_comment(
    bool        condition,
    char const* expression,
    char const* file_name = "---",
    unsigned    line_number = 0,
    char const* comment = "---")
{
    if (!condition) {
        assert_func_production(expression, file_name, line_number, comment);
    }
}

#else

// Internal (debug) build: calls internal asserts
export auto perma_assert(
	bool condition,
	char const* expression,
	char const* file_name = "---",
	unsigned line_number = 0,
	char const* comment = "---") -> void {
    if (!condition) {
        assert_func_internal(expression, file_name, line_number, comment);
    }
}

export auto assert_comment(
	bool condition,
	char const* expression,
	char const* file_name = "---",
	unsigned line_number = 0,
	char const* comment = "---") -> void {
    if (!condition) {
        assert_func_internal(expression, file_name, line_number, comment);
    }
}

#endif // PRODUCTION_BUILD

// ------------------------------------------------------
// Logging Function Templates
// ------------------------------------------------------
// Instead of macros, use templates toggled by #defines
#if !defined(PRODUCTION_BUILD) || (PRODUCTION_BUILD == 0)
#define FORCE_LOG
#endif

#ifdef ERRORS_ONLY
#undef FORCE_LOG
#endif

#ifdef FORCE_LOG

export template <typename... Args>
inline auto llog(Args&&... args) -> void {
    (std::cout << ... << args) << std::endl;
}

export template <typename... Args>
inline auto wlog(Args&&... args) -> void {
    (std::cout << ... << args) << std::endl;
}

export template <typename... Args>
inline auto ilog(Args&&... args) -> void {
    (std::cout << ... << args) << std::endl;
}

export template <typename... Args>
inline auto glog(Args&&... args) -> void {
    (std::cout << ... << args) << std::endl;
}

export template <typename... Args>
inline auto elog(Args&&... args) -> void {
    (std::cout << ... << args) << std::endl;
}

#else // If FORCE_LOG is not defined, provide no-op versions

export template <typename... Args>
inline void llog(Args&&...) {}

export template <typename... Args>
inline void wlog(Args&&...) {}

export template <typename... Args>
inline void ilog(Args&&...) {}

export template <typename... Args>
inline void glog(Args&&...) {}

export template <typename... Args>
inline void elog(Args&&...) {}

#endif
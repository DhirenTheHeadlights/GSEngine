#pragma once

#include <cstdio>
#include <iostream>
#include <signal.h>

// Forward declarations of assert functions
inline void assert_func_production(
    const char* expression,
    const char* file_name,
    unsigned line_number,
    const char* comment = "---");

inline void assert_func_internal(
    const char* expression,
    const char* file_name,
    unsigned line_number,
    const char* comment = "---");

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>

// Windows-specific implementations
inline void assert_func_production(
    const char* expression,
    const char* file_name,
    const unsigned line_number,
    const char* comment)
{
    char message[1024];
    sprintf_s(message,
        "Assertion failed\n\n"
        "File:\n%s\n\n"
        "Line:\n%u\n\n"
        "Expression:\n%s\n\n"
        "Comment:\n%s\n\n"
        "Please report this error to the developer.",
        file_name, line_number, expression, comment);

    MessageBoxA(nullptr, message, "Platform Layer", MB_TASKMODAL | MB_ICONHAND | MB_OK | MB_SETFOREGROUND);
    raise(SIGABRT);
    _exit(3);
}

inline void assert_func_internal(
    const char* expression,
    const char* file_name,
    const unsigned line_number,
    const char* comment)
{
    char message[1024];
    sprintf_s(message,
        "Assertion failed\n\n"
        "File:\n%s\n\n"
        "Line:\n%u\n\n"
        "Expression:\n%s\n\n"
        "Comment:\n%s\n\n"
        "Press retry to debug.",
        file_name, line_number, expression, comment);

    const int action = MessageBoxA(nullptr, message, "Platform Layer",
                                   MB_TASKMODAL | MB_ICONHAND | MB_ABORTRETRYIGNORE | MB_SETFOREGROUND);

    switch (action) {
    case IDABORT:
        raise(SIGABRT);
        _exit(3);
    case IDRETRY:
        __debugbreak();
        break;
    case IDIGNORE:
        break;
    default:
        abort();
    }
}

#else  // Non-Windows platforms

// Non-Windows implementations
inline void assertFuncProduction(
    const char* expression,
    const char* fileName,
    unsigned lineNumber,
    const char* comment)
{
    fprintf(stderr, "Assertion failed in production build:\nFile: %s\nLine: %u\nExpression: %s\nComment: %s\n",
        fileName, lineNumber, expression, comment);
    raise(SIGABRT);
}

inline void assertFuncInternal(
    const char* expression,
    const char* fileName,
    unsigned lineNumber,
    const char* comment)
{
    fprintf(stderr, "Assertion failed:\nFile: %s\nLine: %u\nExpression: %s\nComment: %s\n",
        fileName, lineNumber, expression, comment);
    raise(SIGABRT);
}

#endif  // _WIN32

// Define permaAssert macros
#if PRODUCTION_BUILD == 0
#define permaAssert(expression) \
    ((expression) ? (void)0 : assert_func_internal(#expression, __FILE__, __LINE__))
#define assert_comment(expression, comment) \
    ((expression) ? (void)0 : assert_func_internal(#expression, __FILE__, __LINE__, comment))
#else
#define permaAssert(expression) \
    ((expression) ? (void)0 : assertFuncProduction(#expression, __FILE__, __LINE__))
#define permaAssertComment(expression, comment) \
    ((expression) ? (void)0 : assertFuncProduction(#expression, __FILE__, __LINE__, comment))
#endif

// Logging Macros
#if PRODUCTION_BUILD == 0
#define FORCE_LOG
#endif

#ifdef ERRORS_ONLY
#undef FORCE_LOG
#endif

#ifdef FORCE_LOG

template<typename... Args>
void generic_log(Args&&... args) {
    (std::cout << ... << args) << std::endl;
}

#define DEFINE_LOG_FUNCTION(log_func)                \
    template<typename... Args>                       \
    void log_func(Args&&... args) {                  \
        generic_log(std::forward<Args>(args)...);    \
    }

#else

#define DEFINE_LOG_FUNCTION(log_func)                \
    template<typename... Args>                       \
    void log_func(Args&&...) {}

#endif

DEFINE_LOG_FUNCTION(llog)
DEFINE_LOG_FUNCTION(wlog)
DEFINE_LOG_FUNCTION(ilog)
DEFINE_LOG_FUNCTION(glog)
DEFINE_LOG_FUNCTION(elog)

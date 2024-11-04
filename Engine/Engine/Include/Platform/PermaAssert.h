#pragma once

#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>

inline void assertFuncProduction(
    const char* expression,
    const char* fileName,
    const unsigned lineNumber,
    const char* comment = "---")
{
    char c[1024] = {};

    sprintf(c,
        "Assertion failed\n\n"
        "File:\n"
        "%s\n\n"
        "Line:\n"
        "%u\n\n"
        "Expression:\n"
        "%s\n\n"
        "Comment:\n"
        "%s"
        "\n\nPlease report this error to the developer.",
        fileName, lineNumber, expression, comment);

    const int action = MessageBox(0, c, "Platform Layer", MB_TASKMODAL
                                  | MB_ICONHAND | MB_OK | MB_SETFOREGROUND);

    if (action == IDOK) {
        raise(SIGABRT);
        _exit(3); // Immediate exit
    }
    else {
        _exit(3);
    }
}

inline void assertFuncInternal(
    const char* expression,
    const char* fileName,
    const unsigned lineNumber,
    const char* comment = "---")
{
    char c[1024] = {};

    sprintf(c,
        "Assertion failed\n\n"
        "File:\n"
        "%s\n\n"
        "Line:\n"
        "%u\n\n"
        "Expression:\n"
        "%s\n\n"
        "Comment:\n"
        "%s"
        "\n\nPress retry to debug.",
        fileName, lineNumber, expression, comment);

    int action = MessageBox(0, c, "Platform Layer", MB_TASKMODAL
        | MB_ICONHAND | MB_ABORTRETRYIGNORE | MB_SETFOREGROUND);

    switch (action) {
    case IDABORT:
        raise(SIGABRT);
        _exit(3);
    case IDRETRY:
        __debugbreak();
        return;
    case IDIGNORE:
        return;
    default:
        abort();
    }
}

#if PRODUCTION_BUILD == 0
#define permaAssert(expression) \
            (void)((!!(expression)) || (assertFuncInternal(#expression, __FILE__, __LINE__), 0))

#define permaAssertComment(expression, comment) \
            (void)((!!(expression)) || (assertFuncInternal(#expression, __FILE__, __LINE__, comment), 1))
#else
#define permaAssert(expression) \
            (void)((!!(expression)) || (assertFuncProduction(#expression, __FILE__, __LINE__), 0))

#define permaAssertComment(expression, comment) \
            (void)((!!(expression)) || (assertFuncProduction(#expression, __FILE__, __LINE__, comment), 1))
#endif

#else // For Linux or other platforms
inline void assertFuncProduction(
    const char* expression,
    const char* file_name,
    unsigned line_number,
    const char* comment = "---")
{
    raise(SIGABRT);
}

inline void assertFuncInternal(
    const char* expression,
    const char* file_name,
    unsigned line_number,
    const char* comment = "---")
{
    raise(SIGABRT);
}

#if PRODUCTION_BUILD == 0
#define permaAssert(expression) \
            (void)((!!(expression)) || (assertFuncInternal(#expression, __FILE__, __LINE__), 0))

#define permaAssertComment(expression, comment) \
            (void)((!!(expression)) || (assertFuncInternal(#expression, __FILE__, __LINE__, comment), 1))
#else
#define permaAssert(expression) \
            (void)((!!(expression)) || (assertFuncProduction(#expression, __FILE__, __LINE__), 0))

#define permaAssertComment(expression, comment) \
            (void)((!!(expression)) || (assertFuncProduction(#expression, __FILE__, __LINE__, comment), 1))
#endif
#endif

// Logging Macros
#if PRODUCTION_BUILD == 0
#define FORCE_LOG
#endif

#ifdef ERRORS_ONLY
#undef FORCE_LOG
#endif

#ifdef FORCE_LOG
inline void llog() { std::cout << "\n"; }

template <typename F, typename... T>
void llog(F&& f, T&&... args) {
    std::cout << std::forward<F>(f) << " ";
    llog(std::forward<T>(args)...);
}
#else
template <typename F, typename... T>
void llog(F&&, T&&...) {}
#endif

#ifdef FORCE_LOG
inline void wlog() { std::cout << "\n"; }

template <typename F, typename... T>
void wlog(F&& f, T&&... args) {
    std::cout << f << " ";
    wlog(std::forward<T>(args)...);
}
#else
template <typename F, typename... T>
void wlog(F&&, T&&...) {}
#endif

#ifdef FORCE_LOG
inline void ilog() { std::cout << "\n"; }

template <typename F, typename... T>
void ilog(F&& f, T&&... args) {
    std::cout << f << " ";
    ilog(std::forward<T>(args)...);
}
#else
template <typename F, typename... T>
void ilog(F&&, T&&...) {}
#endif

#ifdef FORCE_LOG
inline void glog() { std::cout << "\n"; }

template <typename F, typename... T>
void glog(F&& f, T&&... args) {
    std::cout << f << " ";
    glog(std::forward<T>(args)...);
}
#else
template <typename F, typename... T>
void glog(F&&, T&&...) {}
#endif

#ifdef FORCE_LOG
inline void elog() { std::cout << "\n"; }

template <typename F, typename... T>
void elog(F&& f, T&&... args) {
    std::cout << f << " ";
    elog(std::forward<T>(args)...);
}
#else
template <typename F, typename... T>
void elog(F&&, T&&...) {}
#endif

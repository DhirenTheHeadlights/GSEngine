module;

#include <Windows.h>

module gse.assert;

import std;

import gse.log;

auto gse::assert_func_production(const std::string_view message) noexcept -> void {
#ifdef _WIN32
    MessageBoxA(
        nullptr,
        message.data(),
        "GSEngine",
        MB_TASKMODAL | MB_ICONHAND | MB_OK | MB_SETFOREGROUND
    );
#endif
    std::terminate();
}

auto gse::assert_func_internal(const std::string_view message) noexcept -> void {
    log::println(
        log::level::error,
        log::category::general,
        "{}",
        message
    );

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
    std::terminate();
#endif
}

export module gse.log;

import std;

export namespace gse::log {
    enum class level : std::uint8_t {
        debug,
        info,
        warning,
        error
    };

    enum class category : std::uint8_t {
        general,
        runtime,
        render,
        network,
        vulkan,
        vulkan_validation,
        vulkan_memory,
        assets,
        task,
        save_system,
        physics
    };

    template <typename... Args>
    auto println(
        std::format_string<Args...> fmt,
        Args&&... args
    ) -> void;

    template <typename... Args>
    auto println(
        level lvl,
        std::format_string<Args...> fmt,
        Args&&... args
    ) -> void;

    template <typename... Args>
    auto println(
        level lvl,
        std::source_location loc,
        std::format_string<Args...> fmt,
        Args&&... args
    ) -> void;

    template <typename... Args>
    auto println(
        category cat,
        std::format_string<Args...> fmt,
        Args&&... args
    ) -> void;

    template <typename... Args>
    auto println(
        level lvl,
        category cat,
        std::format_string<Args...> fmt,
        Args&&... args
    ) -> void;

    template <typename... Args>
    auto println(
        level lvl,
        category cat,
        std::source_location loc,
        std::format_string<Args...> fmt,
        Args&&... args
    ) -> void;

    auto flush(
    ) -> void;
}

namespace gse::log {
    class logger {
    public:
        logger();
        ~logger();

        auto write(
            level lvl,
            std::string_view msg
        ) -> void;

        auto flush(
        ) -> void;

    private:
        std::ofstream m_file;
        std::mutex m_mutex;
    };

    auto instance(
    ) -> logger&;

    auto decorate(
        level lvl,
        category cat,
        std::string_view msg
    ) -> std::string;
}

template <typename... Args>
auto gse::log::println(std::format_string<Args...> fmt, Args&&... args) -> void {
    println(level::info, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
auto gse::log::println(const level lvl, std::format_string<Args...> fmt, Args&&... args) -> void {
    println(lvl, category::general, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
auto gse::log::println(const level lvl, const std::source_location loc, std::format_string<Args...> fmt, Args&&... args) -> void {
    println(lvl, category::general, loc, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
auto gse::log::println(const category cat, std::format_string<Args...> fmt, Args&&... args) -> void {
    println(level::info, cat, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
auto gse::log::println(const level lvl, const category cat, std::format_string<Args...> fmt, Args&&... args) -> void {
    const auto msg = std::format(fmt, std::forward<Args>(args)...);
    instance().write(lvl, decorate(lvl, cat, msg));
}

template <typename... Args>
auto gse::log::println(const level lvl, const category cat, const std::source_location loc, std::format_string<Args...> fmt, Args&&... args) -> void {
    const auto msg = std::format(fmt, std::forward<Args>(args)...);
    instance().write(
        lvl,
        decorate(lvl, cat, std::format("{}:{} - {}", loc.file_name(), loc.line(), msg))
    );
}

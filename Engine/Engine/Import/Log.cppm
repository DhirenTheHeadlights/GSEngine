export module gse.log;

import std;

import gse.config;

export namespace gse::log {
    enum class level : std::uint8_t {
        debug,
        info,
        warning,
        error
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

    auto flush(
    ) -> void;
}

namespace gse::log {
    auto log_file_path(
    ) -> std::filesystem::path {
        return config::resource_path / "Misc" / "log.txt";
    }

    auto level_to_string(
        const level lvl
    ) -> std::string_view {
        switch (lvl) {
            case level::debug:   return "DEBUG";
            case level::info:    return "INFO";
            case level::warning: return "WARNING";
            case level::error:   return "ERROR";
            default:             return "UNKNOWN";
        }
    }

    class logger {
    public:
        logger() {
            m_file.open(log_file_path(), std::ios::out | std::ios::trunc);
            if (m_file.is_open()) {
                const auto now = std::chrono::system_clock::now();
                const auto time = std::chrono::system_clock::to_time_t(now);
                m_file << "=== Log started at " << std::ctime(&time);
                m_file << std::flush;
            }
        }

        ~logger() {
            if (m_file.is_open()) {
                const auto now = std::chrono::system_clock::now();
                const auto time = std::chrono::system_clock::to_time_t(now);
                m_file << "=== Log ended at " << std::ctime(&time);
                m_file.flush();
                m_file.close();
            }
        }

        auto write(
            const std::string_view msg
        ) -> void {
            std::lock_guard lock(m_mutex);
            if (m_file.is_open()) {
                m_file << msg << '\n';
            }
        }

        auto flush(
        ) -> void {
            std::lock_guard lock(m_mutex);
            if (m_file.is_open()) {
                m_file.flush();
            }
        }

    private:
        std::ofstream m_file;
        std::mutex m_mutex;
    };

    auto instance(
    ) -> logger& {
        static logger s_logger;
        return s_logger;
    }
}

template <typename... Args>
auto gse::log::println(std::format_string<Args...> fmt, Args&&... args) -> void {
    println(level::info, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
auto gse::log::println(const level lvl, std::format_string<Args...> fmt, Args&&... args) -> void {
    const auto msg = std::format(fmt, std::forward<Args>(args)...);
    const auto tagged = std::format("[{}] {}", level_to_string(lvl), msg);
    instance().write(tagged);
}

template <typename... Args>
auto gse::log::println(const level lvl, const std::source_location loc, std::format_string<Args...> fmt, Args&&... args) -> void {
    const auto msg = std::format(fmt, std::forward<Args>(args)...);
    const auto tagged = std::format("[{}] {}:{} - {}", level_to_string(lvl), loc.file_name(), loc.line(), msg);
    instance().write(tagged);
}

auto gse::log::flush() -> void {
    instance().flush();
}

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

    auto category_to_string(
        const category cat
    ) -> std::string_view {
        switch (cat) {
            case category::general:            return "General";
            case category::runtime:            return "Runtime";
            case category::render:             return "Render";
            case category::network:            return "Network";
            case category::vulkan:             return "Vulkan";
            case category::vulkan_validation:  return "Vulkan.Validation";
            case category::vulkan_memory:      return "Vulkan.Memory";
            case category::assets:             return "Assets";
            case category::task:               return "Task";
            case category::save_system:        return "SaveSystem";
            case category::physics:            return "Physics";
            default:                           return "Unknown";
        }
    }

    auto timestamp_string(
    ) -> std::string {
        using namespace std::chrono;

        const auto now = system_clock::now();

        try {
            const auto local_time = zoned_time{ current_zone(), now }.get_local_time();
            const auto local_ms = floor<milliseconds>(local_time);
            const auto local_seconds = floor<seconds>(local_ms);
            const auto millis = duration_cast<milliseconds>(local_ms - local_seconds);
            return std::format("{:%Y-%m-%d %H:%M:%S}.{:03}", local_seconds, millis.count());
        } catch (const std::runtime_error&) {
            const auto utc_ms = floor<milliseconds>(now);
            const auto utc_seconds = floor<seconds>(utc_ms);
            const auto millis = duration_cast<milliseconds>(utc_ms - utc_seconds);
            return std::format("{:%Y-%m-%d %H:%M:%S}.{:03}Z", utc_seconds, millis.count());
        }
    }

    auto current_thread_tag(
    ) -> std::uint64_t {
        return std::hash<std::thread::id>{}(std::this_thread::get_id());
    }

    auto should_flush(
        const level lvl
    ) -> bool {
        return lvl == level::warning || lvl == level::error;
    }

    auto decorate(
        const level lvl,
        const category cat,
        const std::string_view msg
    ) -> std::string {
        return std::format(
            "[{}][{}][{}][T{:016x}] {}",
            timestamp_string(),
            level_to_string(lvl),
            category_to_string(cat),
            current_thread_tag(),
            msg
        );
    }

    class logger {
    public:
        logger() {
            std::error_code ec;
            std::filesystem::create_directories(log_file_path().parent_path(), ec);
            m_file.open(log_file_path(), std::ios::out | std::ios::trunc);
            if (m_file.is_open()) {
                const auto banner = std::format("=== Log started at {} ===", timestamp_string());
                m_file << banner << '\n' << std::flush;
                std::cout << banner << '\n' << std::flush;
            }
        }

        ~logger() {
            const auto banner = std::format("=== Log ended at {} ===", timestamp_string());
            if (m_file.is_open()) {
                m_file << banner << '\n';
                m_file.flush();
                m_file.close();
            }
            std::cout << banner << '\n' << std::flush;
        }

        auto write(
            const level lvl,
            const std::string_view msg
        ) -> void {
            const bool flush_now = should_flush(lvl);
            std::lock_guard lock(m_mutex);

            auto& stream = flush_now ? static_cast<std::ostream&>(std::cerr) : static_cast<std::ostream&>(std::cout);
            stream << msg << '\n';
            if (flush_now) {
                stream.flush();
            }

            if (m_file.is_open()) {
                m_file << msg << '\n' << std::flush;
            }
        }

        auto flush(
        ) -> void {
            std::lock_guard lock(m_mutex);
            std::cout.flush();
            std::cerr.flush();
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

auto gse::log::flush() -> void {
    instance().flush();
}

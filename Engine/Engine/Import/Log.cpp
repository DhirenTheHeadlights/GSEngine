module gse.log;

import std;

import gse.config;

namespace gse::log {
    auto log_file_path(
    ) -> std::filesystem::path;

    auto level_to_string(
        level lvl
    ) -> std::string_view;

    auto category_to_string(
        category cat
    ) -> std::string_view;

    auto timestamp_string(
    ) -> std::string;

    auto current_thread_tag(
    ) -> std::uint64_t;

    auto should_flush(
        level lvl
    ) -> bool;
}

auto gse::log::log_file_path() -> std::filesystem::path {
    return config::resource_path / "Misc" / "log.txt";
}

auto gse::log::level_to_string(const level lvl) -> std::string_view {
    switch (lvl) {
        case level::debug:   return "DEBUG";
        case level::info:    return "INFO";
        case level::warning: return "WARNING";
        case level::error:   return "ERROR";
        default:             return "UNKNOWN";
    }
}

auto gse::log::category_to_string(const category cat) -> std::string_view {
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

auto gse::log::timestamp_string() -> std::string {
    using namespace std::chrono;

    const auto now = system_clock::now();
    const auto utc_ms = floor<milliseconds>(now);
    const auto utc_seconds = floor<seconds>(utc_ms);
    const auto millis = duration_cast<milliseconds>(utc_ms - utc_seconds);
    return std::format("{:%Y-%m-%d %H:%M:%S}.{:03}Z", utc_seconds, millis.count());
}

auto gse::log::current_thread_tag() -> std::uint64_t {
    return std::hash<std::thread::id>{}(std::this_thread::get_id());
}

auto gse::log::should_flush(const level lvl) -> bool {
    return lvl == level::warning || lvl == level::error;
}

auto gse::log::decorate(const level lvl, const category cat, const std::string_view msg) -> std::string {
    return std::format(
        "[{}][{}][{}][T{:016x}] {}",
        timestamp_string(),
        level_to_string(lvl),
        category_to_string(cat),
        current_thread_tag(),
        msg
    );
}

gse::log::logger::logger() {
    std::error_code ec;
    std::filesystem::create_directories(log_file_path().parent_path(), ec);
    m_file.open(log_file_path(), std::ios::out | std::ios::trunc);
    if (m_file.is_open()) {
        const auto banner = std::format("=== Log started at {} ===", timestamp_string());
        m_file << banner << '\n' << std::flush;
        std::cout << banner << '\n' << std::flush;
    }
}

gse::log::logger::~logger() {
    const auto banner = std::format("=== Log ended at {} ===", timestamp_string());
    if (m_file.is_open()) {
        m_file << banner << '\n';
        m_file.flush();
        m_file.close();
    }
    std::cout << banner << '\n' << std::flush;
}

auto gse::log::logger::write(const level lvl, const std::string_view msg) -> void {
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

auto gse::log::logger::flush() -> void {
    std::lock_guard lock(m_mutex);
    std::cout.flush();
    std::cerr.flush();
    if (m_file.is_open()) {
        m_file.flush();
    }
}

auto gse::log::instance() -> logger& {
    static logger s_logger;
    return s_logger;
}

auto gse::log::flush() -> void {
    instance().flush();
}

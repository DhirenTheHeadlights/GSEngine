module gse.log;

import std;

import gse.config;
import gse.meta;

namespace gse::log {
	auto log_file_path() -> std::filesystem::path;

	auto timestamp_string() -> std::string;

	auto current_thread_tag() -> std::uint64_t;

	auto should_flush(level lvl) -> bool;
}

auto gse::log::log_file_path() -> std::filesystem::path {
	return config::resource_path / "Misc" / "log.txt";
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
	return lvl == level::error;
}

gse::log::logger::logger() {
	std::error_code ec;
	std::filesystem::create_directories(log_file_path().parent_path(), ec);
	m_file.open(log_file_path(), std::ios::out | std::ios::trunc);
	if (m_file.is_open()) {
		const auto ts = timestamp_string();
		std::println(m_file, "=== Log started at {} ===", ts);
		std::println(std::cout, "=== Log started at {} ===", ts);
		m_file.flush();
		std::cout.flush();
	}
}

gse::log::logger::~logger() {
	const auto ts = timestamp_string();
	if (m_file.is_open()) {
		std::println(m_file, "=== Log ended at {} ===", ts);
		m_file.flush();
		m_file.close();
	}
	std::println(std::cout, "=== Log ended at {} ===", ts);
	std::cout.flush();
}

auto gse::log::logger::write_line(
	const level lvl,
	const category cat,
	const std::string_view extra_prefix,
	const std::string_view fmt,
	std::format_args args
) -> void {
	std::lock_guard lock(m_mutex);
	const bool flush_now = should_flush(lvl);
	auto& console = flush_now ? static_cast<std::ostream&>(std::cerr) : static_cast<std::ostream&>(std::cout);

	const auto ts = timestamp_string();
	const auto thread = current_thread_tag();

	std::print(console, "[{}][{}][{}][T{:016x}] {}", ts, lvl, cat, thread, extra_prefix);
	std::vprint_unicode(console, fmt, args);
	std::print(console, "\n");

	if (m_file.is_open()) {
		std::print(m_file, "[{}][{}][{}][T{:016x}] {}", ts, lvl, cat, thread, extra_prefix);
		std::vprint_unicode(m_file, fmt, args);
		std::print(m_file, "\n");
	}

	if (flush_now) {
		console.flush();
		if (m_file.is_open()) {
			m_file.flush();
		}
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

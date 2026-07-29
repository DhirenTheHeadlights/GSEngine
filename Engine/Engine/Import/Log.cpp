module;

#include <cstdio>

module gse.log;

import std;

import gse.config;
import gse.meta;
import gse.moodycamel;

namespace gse::log {
	auto log_file_path() -> std::filesystem::path;

	auto timestamp_string() -> std::string;

	auto current_thread_tag() -> std::uint64_t;

	auto thread_display() -> std::string;

	auto should_flush(
		level lvl
	) -> bool;

	auto rotate_logs(
		const std::filesystem::path& path,
		std::size_t max_files
	) -> void;

	auto level_sgr(
		level lvl
	) -> int;

	auto json_escape(
		std::string_view text
	) -> std::string;

	constexpr auto category_count = std::define_static_array(std::meta::enumerators_of(^^category)).size();

	constexpr std::size_t no_thread_index = std::numeric_limits<std::size_t>::max();

	constexpr std::size_t log_files_kept = 5;

	std::array<std::atomic<level>, category_count> category_levels;

	std::atomic<std::size_t> backtrace_size = 0;

	std::atomic<bool> color_enabled = true;

	std::atomic<bool> logger_alive = false;

	thread_local thread_role t_thread_role = thread_role::unknown;

	thread_local std::size_t t_thread_index = no_thread_index;

	thread_local std::string t_context;

	class console_sink : public sink {
	public:
		auto write(
			const record& rec
		) -> void override;

		auto write_raw(
			std::string_view text
		) -> void override;

		auto flush() -> void override;
	};

	class file_sink : public sink {
	public:
		file_sink(
			std::filesystem::path path,
			std::size_t max_files
		);

		auto write(
			const record& rec
		) -> void override;

		auto write_raw(
			std::string_view text
		) -> void override;

		auto flush() -> void override;

	private:
		std::ofstream m_file;
	};

	struct queued_record {
		enum class kind : std::uint8_t {
			log,
			flush,
			terminate
		};

		kind type = kind::log;
		level lvl = level::info;
		category cat = category::general;
		std::uint64_t token = 0;
		std::string timestamp;
		std::string thread;
		std::string prefix;
		std::string message;
	};

	class logger {
	public:
		logger();
		~logger();

		auto write_line(
			level lvl,
			category cat,
			std::string_view extra_prefix,
			std::string_view fmt,
			std::format_args args
		) -> void;

		auto add_sink(
			std::unique_ptr<sink> s
		) -> sink*;

		auto set_async(
			bool enabled
		) -> void;

		auto flush() -> void;

		auto dump_backtrace() -> void;

		auto clear_backtrace() -> void;

	private:
		auto dispatch(
			const record& rec
		) -> void;

		auto run() -> void;

		auto process(
			const queued_record& qr
		) -> bool;

		auto dump_backtrace_locked() -> void;

		auto emit_repeat_summary() -> void;

		std::vector<std::unique_ptr<sink>> m_sinks;
		std::mutex m_sink_mutex;
		std::deque<queued_record> m_backtrace;

		level m_last_level = level::info;
		category m_last_cat = category::general;
		std::string m_last_message;
		std::uint64_t m_repeat_count = 0;
		bool m_has_last = false;

		moodycamel::ConcurrentQueue<queued_record> m_queue;
		std::counting_semaphore<> m_items{ 0 };

		std::mutex m_flush_mutex;
		std::condition_variable m_flushed;
		std::atomic<std::uint64_t> m_flush_seq = 0;
		std::atomic<std::uint64_t> m_flush_done = 0;

		std::atomic<bool> m_async = false;
		std::thread m_worker;
	};

	logger instance;
}

auto gse::log::log_file_path() -> std::filesystem::path {
	return config::logs_dir() / std::format("{}.log", config::executable_stem());
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
	return lvl >= level::error;
}

auto gse::log::thread_display() -> std::string {
	if (t_thread_role == thread_role::unknown) {
		return std::format("T{:016x}", current_thread_tag());
	}
	if (t_thread_index == no_thread_index) {
		return std::format("{}", t_thread_role);
	}
	return std::format("{}-{}", t_thread_role, t_thread_index);
}

auto gse::log::set_level(const level min) -> void {
	for (auto& slot : category_levels) {
		slot.store(min, std::memory_order_relaxed);
	}
}

auto gse::log::set_level(const category cat, const level min) -> void {
	category_levels[static_cast<std::size_t>(cat)].store(min, std::memory_order_relaxed);
}

auto gse::log::level_of(const category cat) -> level {
	return category_levels[static_cast<std::size_t>(cat)].load(std::memory_order_relaxed);
}

auto gse::log::enabled(const level lvl, const category cat) -> bool {
	return lvl >= category_levels[static_cast<std::size_t>(cat)].load(std::memory_order_relaxed);
}

auto gse::log::name_thread(const thread_role role) -> void {
	t_thread_role = role;
	t_thread_index = no_thread_index;
}

auto gse::log::name_thread(const thread_role role, const std::size_t index) -> void {
	t_thread_role = role;
	t_thread_index = index;
}

gse::log::sampler::sampler(const std::uint64_t every) : m_mode(mode::count), m_every(every == 0 ? 1 : every) {}

gse::log::sampler::sampler(const std::chrono::steady_clock::duration min_interval) : m_mode(mode::interval), m_interval(min_interval), m_last((std::chrono::steady_clock::now() - min_interval).time_since_epoch().count()) {}

auto gse::log::sampler::tick() -> bool {
	if (m_mode == mode::count) {
		const auto n = m_count.fetch_add(1, std::memory_order_relaxed);
		return n % m_every == 0;
	}

	const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
	auto last = m_last.load(std::memory_order_relaxed);
	for (;;) {
		if (now - last < m_interval.count()) {
			return false;
		}
		if (m_last.compare_exchange_weak(last, now, std::memory_order_relaxed)) {
			return true;
		}
	}
}

auto gse::log::format_line(const record& rec) -> std::string {
	return std::format("[{}][{}][{}][{}] {}{}", rec.timestamp, rec.lvl, rec.cat, rec.thread, rec.prefix, rec.message);
}

auto gse::log::json_escape(const std::string_view text) -> std::string {
	std::string out;
	out.reserve(text.size() + 8);
	for (const char c : text) {
		switch (c) {
			case '"':
				out += "\\\"";
				break;
			case '\\':
				out += "\\\\";
				break;
			case '\n':
				out += "\\n";
				break;
			case '\r':
				out += "\\r";
				break;
			case '\t':
				out += "\\t";
				break;
			default:
				if (static_cast<unsigned char>(c) < 0x20) {
					out += std::format("\\u{:04x}", static_cast<unsigned>(static_cast<unsigned char>(c)));
					break;
				}
				out += c;
				break;
		}
	}
	return out;
}

gse::log::json_sink::json_sink(const std::filesystem::path path) {
	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);
	m_file.open(path, std::ios::out | std::ios::trunc);
}

auto gse::log::json_sink::write(const record& rec) -> void {
	if (!m_file.is_open()) {
		return;
	}
	std::print(
		m_file,
		R"({{"time":"{}","level":"{}","category":"{}","thread":"{}","message":"{}"}})"
		"\n",
		rec.timestamp,
		rec.lvl,
		rec.cat,
		rec.thread,
		json_escape(std::format("{}{}", rec.prefix, rec.message))
	);
}

auto gse::log::json_sink::write_raw(const std::string_view text) -> void {
	if (!m_file.is_open()) {
		return;
	}
	std::print(m_file, R"({{"raw":"{}"}})" "\n", json_escape(text));
}

auto gse::log::json_sink::flush() -> void {
	if (m_file.is_open()) {
		m_file.flush();
	}
}

gse::log::scope::scope(const std::string_view label) {
	m_restore = t_context.size();
	t_context += label;
	t_context += ' ';
}

gse::log::scope::~scope() {
	t_context.resize(m_restore);
}

auto gse::log::level_sgr(const level lvl) -> int {
	return gse::annotation_from_enum<ansi_sgr>(lvl, ansi_sgr{ 0 }).code;
}

gse::log::sink::~sink() = default;

auto gse::log::console_sink::write(const record& rec) -> void {
	auto& os = should_flush(rec.lvl) ? static_cast<std::ostream&>(std::cerr) : static_cast<std::ostream&>(std::cout);
	const auto line = format_line(rec);
	if (color_enabled.load(std::memory_order_relaxed)) {
		std::print(os, "\033[{}m{}\033[0m\n", level_sgr(rec.lvl), line);
		return;
	}
	std::print(os, "{}\n", line);
}

auto gse::log::console_sink::write_raw(const std::string_view text) -> void {
	std::print(std::cout, "{}\n", text);
}

auto gse::log::console_sink::flush() -> void {
	std::cout.flush();
	std::cerr.flush();
}

auto gse::log::rotate_logs(const std::filesystem::path& path, const std::size_t max_files) -> void {
	if (max_files <= 1) {
		return;
	}

	const auto dir = path.parent_path();
	const auto stem = path.stem().native_encoded_string();
	const auto ext = path.extension().native_encoded_string();

	auto nth = [&](const std::size_t i) -> std::filesystem::path {
		if (i == 0) {
			return path;
		}
		return dir / std::format("{}.{}{}", stem, i, ext);
	};

	std::error_code ec;
	std::filesystem::remove(nth(max_files - 1), ec);
	for (std::size_t i = max_files - 1; i > 0; --i) {
		std::filesystem::rename(nth(i - 1), nth(i), ec);
	}
}

gse::log::file_sink::file_sink(const std::filesystem::path path, const std::size_t max_files) {
	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);
	rotate_logs(path, max_files);
	m_file.open(path, std::ios::out | std::ios::trunc);
}

auto gse::log::file_sink::write(const record& rec) -> void {
	if (m_file.is_open()) {
		std::print(m_file, "{}\n", format_line(rec));
	}
}

auto gse::log::file_sink::write_raw(const std::string_view text) -> void {
	if (m_file.is_open()) {
		std::print(m_file, "{}\n", text);
	}
}

auto gse::log::file_sink::flush() -> void {
	if (m_file.is_open()) {
		m_file.flush();
	}
}

gse::log::logger::logger() {
	m_sinks.push_back(std::make_unique<console_sink>());
	m_sinks.push_back(std::make_unique<file_sink>(log_file_path(), log_files_kept));

	const auto marker = std::format("=== Log started at {} ===", timestamp_string());
	for (auto& s : m_sinks) {
		s->write_raw(marker);
		s->flush();
	}

	logger_alive.store(true, std::memory_order_relaxed);
}

gse::log::logger::~logger() {
	logger_alive.store(false, std::memory_order_relaxed);
	set_async(false);

	std::lock_guard lock(m_sink_mutex);
	queued_record qr;
	while (m_queue.try_dequeue(qr)) {
		if (qr.type == queued_record::kind::log) {
			const record rec{
				.lvl = qr.lvl,
				.cat = qr.cat,
				.timestamp = qr.timestamp,
				.thread = qr.thread,
				.prefix = qr.prefix,
				.message = qr.message,
			};
			dispatch(rec);
		}
	}

	if (m_repeat_count > 0) {
		emit_repeat_summary();
	}

	const auto marker = std::format("=== Log ended at {} ===", timestamp_string());
	for (auto& s : m_sinks) {
		s->write_raw(marker);
		s->flush();
	}
}

auto gse::log::logger::dispatch(const record& rec) -> void {
	if (m_has_last && rec.lvl == m_last_level && rec.cat == m_last_cat && rec.message == m_last_message) {
		++m_repeat_count;
		return;
	}
	if (m_repeat_count > 0) {
		emit_repeat_summary();
	}
	for (auto& s : m_sinks) {
		if (rec.lvl >= s->min_level.load(std::memory_order_relaxed)) {
			s->write(rec);
		}
	}
	m_last_level = rec.lvl;
	m_last_cat = rec.cat;
	m_last_message = rec.message;
	m_has_last = true;
}

auto gse::log::logger::emit_repeat_summary() -> void {
	const auto ts = timestamp_string();
	const auto thread = thread_display();
	const auto message = std::format("(previous message repeated {} times)", m_repeat_count);
	const record rec{
		.lvl = m_last_level,
		.cat = m_last_cat,
		.timestamp = ts,
		.thread = thread,
		.prefix = {},
		.message = message,
	};
	for (auto& s : m_sinks) {
		if (rec.lvl >= s->min_level.load(std::memory_order_relaxed)) {
			s->write(rec);
		}
	}
	m_repeat_count = 0;
}

auto gse::log::logger::process(const queued_record& qr) -> bool {
	const bool pass = enabled(qr.lvl, qr.cat);
	if (backtrace_size.load(std::memory_order_relaxed) > 0) {
		if (pass && should_flush(qr.lvl)) {
			dump_backtrace_locked();
		}
		if (!pass) {
			m_backtrace.push_back(qr);
			while (m_backtrace.size() > backtrace_size.load(std::memory_order_relaxed)) {
				m_backtrace.pop_front();
			}
		}
	}
	if (!pass) {
		return false;
	}
	const record rec{
		.lvl = qr.lvl,
		.cat = qr.cat,
		.timestamp = qr.timestamp,
		.thread = qr.thread,
		.prefix = qr.prefix,
		.message = qr.message,
	};
	dispatch(rec);
	return should_flush(qr.lvl);
}

auto gse::log::logger::dump_backtrace_locked() -> void {
	if (m_backtrace.empty()) {
		return;
	}
	for (auto& s : m_sinks) {
		s->write_raw(std::format("====== backtrace: {} held record(s) ======", m_backtrace.size()));
	}
	for (const auto& qr : m_backtrace) {
		const record rec{
			.lvl = qr.lvl,
			.cat = qr.cat,
			.timestamp = qr.timestamp,
			.thread = qr.thread,
			.prefix = qr.prefix,
			.message = qr.message,
		};
		for (auto& s : m_sinks) {
			s->write(rec);
		}
	}
	for (auto& s : m_sinks) {
		s->write_raw("====== end backtrace ======");
	}
	m_backtrace.clear();
}

auto gse::log::logger::dump_backtrace() -> void {
	std::lock_guard sink_lock(m_sink_mutex);
	dump_backtrace_locked();
}

auto gse::log::logger::clear_backtrace() -> void {
	std::lock_guard sink_lock(m_sink_mutex);
	m_backtrace.clear();
}

auto gse::log::logger::run() -> void {
	std::vector<queued_record> batch;
	for (;;) {
		m_items.acquire();
		std::size_t pending = 1;
		while (m_items.try_acquire()) {
			++pending;
		}

		batch.clear();
		for (std::size_t i = 0; i < pending; ++i) {
			queued_record qr;
			while (!m_queue.try_dequeue(qr)) {
			}
			batch.push_back(std::move(qr));
		}

		bool needs_flush = false;
		std::uint64_t flush_token = 0;
		bool terminate = false;
		{
			std::lock_guard sink_lock(m_sink_mutex);
			for (const auto& qr : batch) {
				if (qr.type == queued_record::kind::terminate) {
					terminate = true;
					continue;
				}
				if (qr.type == queued_record::kind::flush) {
					needs_flush = true;
					flush_token = std::max(flush_token, qr.token);
					continue;
				}
				if (process(qr)) {
					needs_flush = true;
				}
			}
			if (needs_flush) {
				for (auto& s : m_sinks) {
					s->flush();
				}
			}
		}

		if (flush_token != 0) {
			{
				std::lock_guard fl(m_flush_mutex);
				m_flush_done.store(flush_token, std::memory_order_relaxed);
			}
			m_flushed.notify_all();
		}

		if (terminate) {
			break;
		}
	}
}

auto gse::log::logger::write_line(const level lvl, const category cat, const std::string_view extra_prefix, const std::string_view fmt, std::format_args args) -> void {
	auto ts = timestamp_string();
	auto thread = thread_display();
	auto message = std::vformat(fmt, args);

	std::string context_buf;
	std::string_view prefix = extra_prefix;
	if (!t_context.empty()) {
		context_buf.reserve(t_context.size() + extra_prefix.size());
		context_buf += t_context;
		context_buf += extra_prefix;
		prefix = context_buf;
	}

	if (lvl == level::fatal) {
		flush();
		std::lock_guard sink_lock(m_sink_mutex);
		dump_backtrace_locked();
		const record rec{
			.lvl = lvl,
			.cat = cat,
			.timestamp = ts,
			.thread = thread,
			.prefix = prefix,
			.message = message,
		};
		for (auto& s : m_sinks) {
			s->write(rec);
		}
		for (auto& s : m_sinks) {
			s->flush();
		}
		std::terminate();
	}

	if (m_async.load(std::memory_order_acquire)) {
		m_queue.enqueue(queued_record{
			.type = queued_record::kind::log,
			.lvl = lvl,
			.cat = cat,
			.timestamp = std::move(ts),
			.thread = std::move(thread),
			.prefix = std::string(prefix),
			.message = std::move(message),
		});
		m_items.release();
		return;
	}

	if (backtrace_size.load(std::memory_order_relaxed) > 0) {
		queued_record qr{
			.type = queued_record::kind::log,
			.lvl = lvl,
			.cat = cat,
			.timestamp = std::move(ts),
			.thread = std::move(thread),
			.prefix = std::string(prefix),
			.message = std::move(message),
		};
		std::lock_guard sink_lock(m_sink_mutex);
		if (process(qr)) {
			for (auto& s : m_sinks) {
				s->flush();
			}
		}
		return;
	}

	std::lock_guard sink_lock(m_sink_mutex);
	const record rec{
		.lvl = lvl,
		.cat = cat,
		.timestamp = ts,
		.thread = thread,
		.prefix = prefix,
		.message = message,
	};
	dispatch(rec);
	if (should_flush(lvl)) {
		for (auto& s : m_sinks) {
			s->flush();
		}
	}
}

auto gse::log::logger::add_sink(std::unique_ptr<sink> s) -> sink* {
	std::lock_guard lock(m_sink_mutex);
	return m_sinks.emplace_back(std::move(s)).get();
}

auto gse::log::logger::set_async(const bool enabled) -> void {
	if (enabled) {
		if (m_async.exchange(true)) {
			return;
		}
		m_flush_seq.store(0, std::memory_order_relaxed);
		m_flush_done.store(0, std::memory_order_relaxed);
		m_worker = std::thread([this] {
			run();
		});
		return;
	}

	if (!m_async.exchange(false)) {
		return;
	}

	m_queue.enqueue(queued_record{ .type = queued_record::kind::terminate });
	m_items.release();
	if (m_worker.joinable()) {
		m_worker.join();
	}

	std::lock_guard sink_lock(m_sink_mutex);
	queued_record qr;
	while (m_queue.try_dequeue(qr)) {
		if (qr.type == queued_record::kind::log) {
			const record rec{
				.lvl = qr.lvl,
				.cat = qr.cat,
				.timestamp = qr.timestamp,
				.thread = qr.thread,
				.prefix = qr.prefix,
				.message = qr.message,
			};
			dispatch(rec);
		}
	}
	while (m_items.try_acquire()) {
	}
}

auto gse::log::logger::flush() -> void {
	if (!m_async.load(std::memory_order_acquire)) {
		std::lock_guard sink_lock(m_sink_mutex);
		for (auto& s : m_sinks) {
			s->flush();
		}
		return;
	}

	const auto token = m_flush_seq.fetch_add(1, std::memory_order_relaxed) + 1;
	m_queue.enqueue(queued_record{
		.type = queued_record::kind::flush,
		.token = token,
	});
	m_items.release();

	std::unique_lock lk(m_flush_mutex);
	m_flushed.wait(lk, [this, token] {
		return m_flush_done.load(std::memory_order_relaxed) >= token || !m_async.load(std::memory_order_relaxed);
	});
}

auto gse::log::write_line(const level lvl, const category cat, const std::string_view extra_prefix, const std::string_view fmt, std::format_args args) -> void {
	if (!logger_alive.load(std::memory_order_relaxed)) {
		const auto message = std::vformat(fmt, args);
		std::fputs(message.c_str(), stderr);
		std::fputc('\n', stderr);
		return;
	}
	instance.write_line(lvl, cat, extra_prefix, fmt, args);
}

auto gse::log::add_sink(std::unique_ptr<sink> s) -> sink* {
	return instance.add_sink(std::move(s));
}

auto gse::log::set_async(const bool enabled) -> void {
	instance.set_async(enabled);
}

auto gse::log::flush() -> void {
	if (!logger_alive.load(std::memory_order_relaxed)) {
		return;
	}
	instance.flush();
}

auto gse::log::enable_backtrace(const std::size_t size) -> void {
	backtrace_size.store(size, std::memory_order_relaxed);
}

auto gse::log::disable_backtrace() -> void {
	backtrace_size.store(0, std::memory_order_relaxed);
	instance.clear_backtrace();
}

auto gse::log::dump_backtrace() -> void {
	instance.dump_backtrace();
}

auto gse::log::backtrace_active() -> bool {
	return backtrace_size.load(std::memory_order_relaxed) > 0;
}

auto gse::log::set_color(const bool enabled) -> void {
	color_enabled.store(enabled, std::memory_order_relaxed);
}

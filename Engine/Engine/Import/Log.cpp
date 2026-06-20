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

	constexpr auto category_count = std::define_static_array(std::meta::enumerators_of(^^category)).size();

	constexpr std::size_t no_thread_index = std::numeric_limits<std::size_t>::max();

	std::array<std::atomic<level>, category_count> g_category_levels;

	thread_local thread_role t_thread_role = thread_role::unknown;

	thread_local std::size_t t_thread_index = no_thread_index;

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
		explicit file_sink(
			std::filesystem::path path
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

	private:
		auto dispatch(
			const record& rec
		) -> void;

		auto run() -> void;

		std::vector<std::unique_ptr<sink>> m_sinks;
		std::mutex m_sink_mutex;

		moodycamel::ConcurrentQueue<queued_record> m_queue;
		std::counting_semaphore<> m_items{ 0 };

		std::mutex m_flush_mutex;
		std::condition_variable m_flushed;
		std::atomic<std::uint64_t> m_flush_seq = 0;
		std::atomic<std::uint64_t> m_flush_done = 0;

		std::atomic<bool> m_async = false;
		std::thread m_worker;
	};

	auto instance() -> logger&;
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
	for (auto& slot : g_category_levels) {
		slot.store(min, std::memory_order_relaxed);
	}
}

auto gse::log::set_level(const category cat, const level min) -> void {
	g_category_levels[static_cast<std::size_t>(cat)].store(min, std::memory_order_relaxed);
}

auto gse::log::level_of(const category cat) -> level {
	return g_category_levels[static_cast<std::size_t>(cat)].load(std::memory_order_relaxed);
}

auto gse::log::enabled(const level lvl, const category cat) -> bool {
	return lvl >= g_category_levels[static_cast<std::size_t>(cat)].load(std::memory_order_relaxed);
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

gse::log::sink::~sink() = default;

auto gse::log::console_sink::write(const record& rec) -> void {
	auto& os = should_flush(rec.lvl) ? static_cast<std::ostream&>(std::cerr) : static_cast<std::ostream&>(std::cout);
	std::print(os, "{}\n", format_line(rec));
}

auto gse::log::console_sink::write_raw(const std::string_view text) -> void {
	std::print(std::cout, "{}\n", text);
}

auto gse::log::console_sink::flush() -> void {
	std::cout.flush();
	std::cerr.flush();
}

gse::log::file_sink::file_sink(const std::filesystem::path path) {
	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);
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
	m_sinks.push_back(std::make_unique<file_sink>(log_file_path()));

	const auto marker = std::format("=== Log started at {} ===", timestamp_string());
	for (auto& s : m_sinks) {
		s->write_raw(marker);
		s->flush();
	}
}

gse::log::logger::~logger() {
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

	const auto marker = std::format("=== Log ended at {} ===", timestamp_string());
	for (auto& s : m_sinks) {
		s->write_raw(marker);
		s->flush();
	}
}

auto gse::log::logger::dispatch(const record& rec) -> void {
	for (auto& s : m_sinks) {
		if (rec.lvl >= s->min_level.load(std::memory_order_relaxed)) {
			s->write(rec);
		}
	}
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
				const record rec{
					.lvl = qr.lvl,
					.cat = qr.cat,
					.timestamp = qr.timestamp,
					.thread = qr.thread,
					.prefix = qr.prefix,
					.message = qr.message,
				};
				dispatch(rec);
				if (should_flush(qr.lvl)) {
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

	if (m_async.load(std::memory_order_acquire)) {
		m_queue.enqueue(queued_record{
			.type = queued_record::kind::log,
			.lvl = lvl,
			.cat = cat,
			.timestamp = std::move(ts),
			.thread = std::move(thread),
			.prefix = std::string(extra_prefix),
			.message = std::move(message),
		});
		m_items.release();
		return;
	}

	std::lock_guard sink_lock(m_sink_mutex);
	const record rec{
		.lvl = lvl,
		.cat = cat,
		.timestamp = ts,
		.thread = thread,
		.prefix = extra_prefix,
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

auto gse::log::instance() -> logger& {
	static logger s_logger;
	return s_logger;
}

auto gse::log::write_line(const level lvl, const category cat, const std::string_view extra_prefix, const std::string_view fmt, std::format_args args) -> void {
	instance().write_line(lvl, cat, extra_prefix, fmt, args);
}

auto gse::log::add_sink(std::unique_ptr<sink> s) -> sink* {
	return instance().add_sink(std::move(s));
}

auto gse::log::set_async(const bool enabled) -> void {
	instance().set_async(enabled);
}

auto gse::log::flush() -> void {
	instance().flush();
}

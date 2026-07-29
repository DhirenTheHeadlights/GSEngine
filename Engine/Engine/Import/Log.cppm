export module gse.log;

import std;

export namespace gse::log {
	struct ansi_sgr {
		int code;
	};

	enum class level : std::uint8_t {
		debug [[= ansi_sgr{ 90 }]],
		info [[= ansi_sgr{ 0 }]],
		warning [[= ansi_sgr{ 33 }]],
		error [[= ansi_sgr{ 31 }]],
		fatal [[= ansi_sgr{ 91 }]]
	};

	enum class category : std::uint8_t {
		general,
		runtime,
		render,
		network,
		vulkan,
		vulkan_validation,
		vulkan_memory,
		dx12,
		dx12_validation,
		assets,
		task,
		save_system,
		physics
	};

	enum class thread_role : std::uint8_t {
		unknown,
		main,
		worker,
		watchdog,
		capture,
		network
	};

	auto set_level(
		level min
	) -> void;

	auto set_level(
		category cat,
		level min
	) -> void;

	auto level_of(
		category cat
	) -> level;

	auto enabled(
		level lvl,
		category cat
	) -> bool;

	class sampler {
	public:
		explicit sampler(
			std::uint64_t every
		);

		explicit sampler(
			std::chrono::steady_clock::duration min_interval
		);

		auto tick() -> bool;

	private:
		enum class mode : std::uint8_t {
			count,
			interval
		};

		mode m_mode;
		std::uint64_t m_every = 1;
		std::chrono::steady_clock::duration m_interval = {};
		std::atomic<std::uint64_t> m_count = 0;
		std::atomic<std::chrono::steady_clock::rep> m_last = 0;
	};

	auto name_thread(
		thread_role role
	) -> void;

	auto name_thread(
		thread_role role,
		std::size_t index
	) -> void;

	struct record {
		level lvl;
		category cat;
		std::string_view timestamp;
		std::string_view thread;
		std::string_view prefix;
		std::string_view message;
	};

	auto format_line(
		const record& rec
	) -> std::string;

	class sink {
	public:
		virtual ~sink();

		virtual auto write(
			const record& rec
		) -> void = 0;

		virtual auto write_raw(
			std::string_view text
		) -> void = 0;

		virtual auto flush() -> void = 0;

		std::atomic<level> min_level = level::debug;
	};

	class json_sink : public sink {
	public:
		explicit json_sink(
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

	class scope {
	public:
		explicit scope(
			std::string_view label
		);

		~scope();

		scope(const scope&) = delete;
		auto operator=(const scope&) -> scope& = delete;

	private:
		std::size_t m_restore;
	};

	auto add_sink(
		std::unique_ptr<sink> s
	) -> sink*;

	auto set_async(
		bool enabled
	) -> void;

	auto enable_backtrace(
		std::size_t size
	) -> void;

	auto disable_backtrace() -> void;

	auto dump_backtrace() -> void;

	auto backtrace_active() -> bool;

	auto set_color(
		bool enabled
	) -> void;

	template <typename... Args>
	auto println(
		std::format_string<Args...> fmt,
		const Args&... args
	) -> void;

	template <typename... Args>
	auto println(
		level lvl,
		std::format_string<Args...> fmt,
		const Args&... args
	) -> void;

	template <typename... Args>
	auto println(
		level lvl,
		std::source_location loc,
		std::format_string<Args...> fmt,
		const Args&... args
	) -> void;

	template <typename... Args>
	auto println(
		category cat,
		std::format_string<Args...> fmt,
		const Args&... args
	) -> void;

	template <typename... Args>
	auto println(
		level lvl,
		category cat,
		std::format_string<Args...> fmt,
		const Args&... args
	) -> void;

	template <typename... Args>
	auto println(
		level lvl,
		category cat,
		std::source_location loc,
		std::format_string<Args...> fmt,
		const Args&... args
	) -> void;

	auto flush() -> void;
}

namespace gse::log {
	auto write_line(
		level lvl,
		category cat,
		std::string_view extra_prefix,
		std::string_view fmt,
		std::format_args args
	) -> void;
}

template <typename... Args>
auto gse::log::println(std::format_string<Args...> fmt, const Args&... args) -> void {
	println(level::info, fmt, args...);
}

template <typename... Args>
auto gse::log::println(const level lvl, std::format_string<Args...> fmt, const Args&... args) -> void {
	println(lvl, category::general, fmt, args...);
}

template <typename... Args>
auto gse::log::println(const level lvl, const std::source_location loc, std::format_string<Args...> fmt, const Args&... args) -> void {
	println(lvl, category::general, loc, fmt, args...);
}

template <typename... Args>
auto gse::log::println(const category cat, std::format_string<Args...> fmt, const Args&... args) -> void {
	println(level::info, cat, fmt, args...);
}

template <typename... Args>
auto gse::log::println(const level lvl, const category cat, std::format_string<Args...> fmt, const Args&... args) -> void {
	if (!enabled(lvl, cat) && !backtrace_active()) {
		return;
	}

	write_line(
		lvl,
		cat,
		{},
		fmt.get(),
		std::make_format_args(args...)
	);
}

template <typename... Args>
auto gse::log::println(const level lvl, const category cat, const std::source_location loc, std::format_string<Args...> fmt, const Args&... args) -> void {
	if (!enabled(lvl, cat) && !backtrace_active()) {
		return;
	}

	const auto loc_prefix = std::format("{}:{} - ", loc.file_name(), loc.line());
	write_line(lvl, cat, loc_prefix, fmt.get(), std::make_format_args(args...));
}

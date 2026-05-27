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

		auto flush() -> void;

	private:
		std::ofstream m_file;
		std::mutex m_mutex;
	};

	auto instance() -> logger&;
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
	instance().write_line(
		lvl,
		cat,
		{},
		fmt.get(),
		std::make_format_args(args...)
	);
}

template <typename... Args>
auto gse::log::println(const level lvl, const category cat, const std::source_location loc, std::format_string<Args...> fmt, const Args&... args) -> void {
	const auto loc_prefix = std::format("{}:{} - ", loc.file_name(), loc.line());
	instance().write_line(lvl, cat, loc_prefix, fmt.get(), std::make_format_args(args...));
}

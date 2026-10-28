module;

#include <version>

module gse.stacktrace;

import std;

auto gse::capture_stacktrace(const std::size_t skip_frames) -> std::string {
#ifdef __cpp_lib_stacktrace
	const auto trace = std::stacktrace::current(skip_frames);

	constexpr std::array<std::string_view, 8> noise = {
		"gse::assert_format_message",
		"gse::assert<",
		"gse::assert_fail",
		"gse::capture_stacktrace",
		"register_frame_ctor",
		"mainCRTStartup",
		"BaseThreadInitThunk",
		"RtlUserThreadStart",
	};

	std::string out;
	out.reserve(trace.size() * 64);
	std::size_t shown = 0;
	for (const auto& entry : trace) {
		const std::string frame = clean_symbol(std::to_string(entry));
		if (std::ranges::any_of(noise, [&frame](const std::string_view n) { return frame.find(n) != std::string::npos; })) {
			continue;
		}
		out += std::format("  #{:>2} {}\n", shown, frame);
		++shown;
	}
	return out;
#else
	return {};
#endif
}

auto gse::install_crash_handlers() -> void {}

auto gse::install_crash_responder(crash_responder) -> void {}

auto gse::report_crash(std::string, std::string) -> void {}

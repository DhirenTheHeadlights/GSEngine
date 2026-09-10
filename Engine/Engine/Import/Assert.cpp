module gse.assert;

import std;

import gse.log;
import gse.stacktrace;
import gse.win32;

namespace gse {
	auto arm_fatal_report_watchdog() -> void;
}

auto gse::arm_fatal_report_watchdog() -> void {
	std::thread([] {
		std::this_thread::sleep_for(std::chrono::seconds(5));
		std::cerr << "gse: fatal report stalled for 5 s; exiting without a trace\n";
		std::_Exit(3);
	}).detach();
}

auto gse::fatal_exit(const int code) noexcept -> void {
	if (win32::IsDebuggerPresent()) {
		win32::DebugBreak();
	}
	std::_Exit(code);
}

auto gse::assert_fail(const std::source_location loc, const std::string_view comment) noexcept -> void {
	arm_fatal_report_watchdog();

	log::println(
		log::level::fatal,
		log::category::general,
		"[Assertion Failure]\n"
		"File: {}\n"
		"Line: {}\n"
		"Function: {}\n"
		"Comment: {}",
		loc.file_name(),
		loc.line(),
		clean_symbol(loc.function_name()),
		comment
	);
	log::flush();

	log::println(log::level::fatal, log::category::general, "Stack:\n{}", capture_stacktrace(2));
	log::flush();

	fatal_exit(3);
}

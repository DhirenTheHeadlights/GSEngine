module gse.assert;

import std;

import gse.log;
import gse.stacktrace;
import gse.win32;

namespace gse {
	std::atomic<fatal_reporter> installed_fatal_reporter = nullptr;

	auto arm_fatal_report_watchdog() -> void;
}

extern "C++" auto handle_contract_violation(const std::contracts::contract_violation& violation) -> void {
	gse::assert_fail(violation.location(), violation.comment());
}

extern "C++" auto tu_has_violation(
	const std::contracts::contract_violation& violation,
	std::uint16_t semantic
) -> void asm("_Z18__tu_has_violationRK33__builtin_contract_violation_typet");

extern "C++" auto tu_has_violation(const std::contracts::contract_violation& violation, std::uint16_t) -> void {
	handle_contract_violation(violation);
}

auto gse::install_fatal_reporter(const fatal_reporter reporter) -> void {
	installed_fatal_reporter.store(reporter, std::memory_order_release);
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

	if (const fatal_reporter reporter = installed_fatal_reporter.load(std::memory_order_acquire)) {
		reporter(loc, comment);
	}

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

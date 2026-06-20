module gse.assert;

import std;

import gse.log;
import gse.stacktrace;

auto gse::assert_format_message(const std::source_location loc, const std::string_view comment) -> std::string {
	return std::format(
		"[Assertion Failure]\n"
		"File: {}\n"
		"Line: {}\n"
		"Function: {}\n"
		"Comment: {}\n"
		"Stack:\n{}",
		loc.file_name(),
		loc.line(),
		loc.function_name(),
		comment,
		capture_stacktrace(2)
	);
}

auto gse::assert_fail(const std::string_view message) noexcept -> void {
	log::println(log::level::error, log::category::general, "{}", message);
	log::flush();
	std::terminate();
}

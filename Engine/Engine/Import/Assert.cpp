module gse.assert;

import std;

import gse.log;
import gse.stacktrace;

auto gse::assert_fail(const std::source_location loc, const std::string_view comment) noexcept -> void {
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
	std::unreachable();
}

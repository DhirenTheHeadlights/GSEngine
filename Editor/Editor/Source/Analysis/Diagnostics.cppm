export module gse.ide.analysis:gcc_diagnostics;

import gse.analysis;
import std;

export namespace gse::ide::analysis {
	struct gcc_diagnostics {
		static auto is_module_unavailable(
			std::string_view message
		) -> bool;

		static auto is_module_unavailable(
			std::span<const gse::analysis::diagnostic> diagnostics
		) -> bool;
	};
}

namespace gse::ide::analysis {
	auto contains_insensitive(
		std::string_view haystack,
		std::string_view needle
	) -> bool;
}

auto gse::ide::analysis::contains_insensitive(const std::string_view haystack, const std::string_view needle) -> bool {
	return !std::ranges::search(haystack, needle, [](const char lhs, const char rhs) {
		return std::tolower(static_cast<unsigned char>(lhs)) == std::tolower(static_cast<unsigned char>(rhs));
	}).empty();
}

auto gse::ide::analysis::gcc_diagnostics::is_module_unavailable(const std::string_view message) -> bool {
	static constexpr std::string_view fragments[] = {
		"must be built",
		"failed to read compiled module",
		"unknown compiled module interface",
		"no such module",
		"returning to the gate",
		"failed to load binding",
		"failed to load pendings",
		"should have been declared inside",
		"compiled module file is",
	};
	return std::ranges::any_of(fragments, [message](const std::string_view fragment) {
		return contains_insensitive(message, fragment);
	});
}

auto gse::ide::analysis::gcc_diagnostics::is_module_unavailable(const std::span<const gse::analysis::diagnostic> diagnostics) -> bool {
	return std::ranges::any_of(diagnostics, [](const gse::analysis::diagnostic& diagnostic) {
		return is_module_unavailable(diagnostic.message);
	});
}

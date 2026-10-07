module gse.test;

import std;

import gse.config;
import gse.core;
import gse.fs;
import gse.log;
import gse.math;
import gse.meta;
import gse.process;

namespace gse::test {
	struct tally {
		int passed = 0;
		int failed = 0;
		int skipped = 0;
	};

	auto split_tags(
		std::string_view text
	) -> std::vector<std::string_view>;

	auto matches(
		const entry& candidate,
		const config& options
	) -> bool;

	auto select(
		std::span<const std::span<const entry>> tables,
		const config& options
	) -> std::vector<const entry*>;

	auto needs_child(
		const entry& candidate
	) -> bool;

	auto report_failures(
		const context& ctx
	) -> void;

	auto run_in_process(
		const entry& candidate
	) -> bool;

	auto run_child(
		const entry& candidate,
		std::string_view flag_prefix
	) -> bool;

	auto run_one(
		const entry& candidate,
		std::string_view flag_prefix,
		tally& counts
	) -> void;
}

gse::test::context::context(const std::string_view name) : m_name(name) {}

auto gse::test::context::name() const -> std::string_view {
	return m_name;
}

auto gse::test::context::failures() const -> std::span<const failure> {
	return m_failures;
}

auto gse::test::context::expect(const bool condition, const std::source_location loc) -> bool {
	if (condition) {
		return true;
	}
	return record("expectation failed", loc);
}

auto gse::test::context::record(std::string message, const std::source_location& loc) -> bool {
	m_failures.push_back({
		.location = loc,
		.message = std::move(message),
	});
	return false;
}

auto gse::test::split_tags(const std::string_view text) -> std::vector<std::string_view> {
	std::vector<std::string_view> tags;
	for (const auto part : std::views::split(text, std::string_view(","))) {
		const std::string_view tag = std::string_view(part);
		const std::size_t begin = tag.find_first_not_of(' ');
		if (begin == std::string_view::npos) {
			continue;
		}
		tags.push_back(tag.substr(begin, tag.find_last_not_of(' ') + 1 - begin));
	}
	return tags;
}

auto gse::test::matches(const entry& candidate, const config& options) -> bool {
	if (!options.filter.empty() && !candidate.name.contains(options.filter)) {
		return false;
	}
	if (options.tags.empty()) {
		return true;
	}
	const std::vector<std::string_view> declared = split_tags(candidate.spec.tags);
	for (const auto wanted : split_tags(options.tags)) {
		if (std::ranges::contains(declared, wanted)) {
			return true;
		}
	}
	return false;
}

auto gse::test::select(const std::span<const std::span<const entry>> tables, const config& options) -> std::vector<const entry*> {
	std::vector<const entry*> selected;
	for (const auto table : tables) {
		for (const entry& candidate : table) {
			if (!options.only.empty()) {
				if (candidate.name == options.only) {
					selected.push_back(&candidate);
				}
				continue;
			}
			if (matches(candidate, options)) {
				selected.push_back(&candidate);
			}
		}
	}
	return selected;
}

auto gse::test::needs_child(const entry& candidate) -> bool {
	return candidate.mode == kind::violation || candidate.spec.isolated;
}

auto gse::test::report_failures(const context& ctx) -> void {
	for (const failure& f : ctx.failures()) {
		log::println(
			log::level::error,
			log::category::test,
			"{} failed at {}:{}: {}",
			ctx.name(),
			f.location.file_name(),
			f.location.line(),
			f.message
		);
	}
}

auto gse::test::run_in_process(const entry& candidate) -> bool {
	context ctx(candidate.name);
	candidate.body(ctx);
	report_failures(ctx);
	return ctx.failures().empty();
}

auto gse::test::run_child(const entry& candidate, const std::string_view flag_prefix) -> bool {
	const std::filesystem::path output = process::temporary_path("test", "txt");
	const auto _ = make_scope_exit([&output] {
		std::error_code ec;
		std::filesystem::remove(output, ec);
	});

	const time limit = seconds(60.f);
	const process::run_outcome outcome = process::run_capture({
		.command_line = std::format(
			"\"{}\" {}-only {}",
			gse::config::executable_file().generic_native_encoded_string(),
			flag_prefix,
			candidate.name
		),
		.working_dir = gse::config::executable_file().parent_path(),
		.output_path = output,
		.limit = limit,
	});
	const std::string text = fs::read_text(output);

	if (!outcome) {
		log::println(
			log::level::error,
			log::category::test,
			"{} could not be run out of process ({})",
			candidate.name,
			outcome.error()
		);
		return false;
	}

	if (candidate.mode == kind::violation) {
		const std::string_view expected(candidate.expected.expect);
		if (*outcome != 3) {
			log::println(
				log::level::error,
				log::category::test,
				"{} expected a contract violation reported through assert_fail (exit 3), got exit {}",
				candidate.name,
				*outcome
			);
			return false;
		}
		if (!text.contains(expected)) {
			log::println(
				log::level::error,
				log::category::test,
				"{} violated a contract but the report did not mention '{}':\n{}",
				candidate.name,
				expected,
				text
			);
			return false;
		}
		return true;
	}

	if (*outcome != 0) {
		log::println(
			log::level::error,
			log::category::test,
			"{} failed in its own process with exit {}:\n{}",
			candidate.name,
			*outcome,
			text
		);
		return false;
	}
	return true;
}

auto gse::test::run_one(const entry& candidate, const std::string_view flag_prefix, tally& counts) -> void {
	if (candidate.spec.needs_gpu) {
		log::println(log::level::warning, log::category::test, "{} skipped: no device", candidate.name);
		++counts.skipped;
		return;
	}
#ifdef GSE_CONTRACTS_IGNORED
	if (candidate.mode == kind::violation) {
		log::println(log::level::warning, log::category::test, "{} skipped: contracts are ignored in this configuration", candidate.name);
		++counts.skipped;
		return;
	}
#endif
	if (needs_child(candidate) ? run_child(candidate, flag_prefix) : run_in_process(candidate)) {
		++counts.passed;
		return;
	}
	++counts.failed;
}

auto gse::test::requested(const config& options) -> bool {
	return options.all || options.list || !options.filter.empty() || !options.tags.empty() || !options.only.empty();
}

auto gse::test::run(const request& req) -> int {
	const std::vector<const entry*> selected = select(req.tables, req.options);

	if (req.options.list) {
		for (const entry* candidate : selected) {
			log::println(log::category::test, "{}", candidate->name);
		}
		return 0;
	}

	if (selected.empty()) {
		log::println(log::level::error, log::category::test, "no test matched the selection");
		return 1;
	}

	if (!req.options.only.empty()) {
		return run_in_process(*selected.front()) ? 0 : 1;
	}

	tally counts;
	for (int pass = 0; pass < req.options.repeat; ++pass) {
		for (const entry* candidate : selected) {
			run_one(*candidate, req.flag_prefix, counts);
		}
	}

	log::println(
		counts.failed == 0 ? log::level::info : log::level::error,
		log::category::test,
		"{} passed, {} failed, {} skipped",
		counts.passed,
		counts.failed,
		counts.skipped
	);
	return counts.failed == 0 ? 0 : 1;
}

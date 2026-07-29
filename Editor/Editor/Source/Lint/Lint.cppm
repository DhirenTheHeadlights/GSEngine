export module gse.ide.lint;

import std;
import gse.ide.diagnostic;
import gse.ide.analysis;

export namespace gse::ide::lint {
	auto analyze_check(analysis::diagnostics_check& check) -> void;
}

auto gse::ide::lint::analyze_check(analysis::diagnostics_check& check) -> void {
	check.lint.clear();
	if (check.quals.empty() && check.template_args.empty()) {
		return;
	}

	std::unordered_set<std::uint64_t> seen;
	check.lint.reserve(check.quals.size() + check.template_args.size());

	auto add = [&](std::span<const analysis::qualified_use> uses, const std::string_view rule, const std::string_view message, const std::string_view fix_title) {
		for (const analysis::qualified_use& q : uses) {
			if (q.line == 0 || q.column == 0 || q.end_line == 0 || q.end_column == 0) {
				continue;
			}
			const std::uint64_t key = (static_cast<std::uint64_t>(q.line) << 32) | q.column;
			if (!seen.insert(key).second) {
				continue;
			}

			const std::uint32_t line = q.line - 1;
			const std::uint32_t end_line = std::max(q.end_line, q.line) - 1;
			const std::uint32_t start = q.column - 1;
			const std::uint32_t end = q.end_column - 1;
			if (end_line == line && end <= start) {
				continue;
			}

			check.lint.push_back(diagnostic{
				.line = line,
				.end_line = end_line,
				.start_col = start,
				.end_col = end,
				.level = severity::hint,
				.source = diagnostic_source::lint,
				.rule = std::string(rule),
				.message = std::string(message),
				.file = q.file,
				.fix = fix_it{
					.title = std::string(fix_title),
					.edits = { text_edit{ .line = line, .end_line = end_line, .start_col = start, .end_col = end } },
				},
			});
		}
	};

	add(check.quals, "redundant-namespace-qualifier", "redundant namespace qualifier", "Remove redundant qualifier");
	add(check.template_args, "redundant-template-arguments", "redundant template arguments (deducible from the call)", "Remove redundant template arguments");
}

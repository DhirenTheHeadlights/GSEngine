export module gse.ide.lint;

import std;
import gse;
import gse.ide.diagnostic;
import gse.ide.analysis;

export namespace gse::ide::lint {
	struct records {
		std::span<const analysis::qualified_use> quals;
		std::span<const analysis::qualified_use> template_args;
		std::span<const analysis::unused_local> unused_locals;
	};

	auto findings(
		records emitted
	) -> std::vector<lint_finding>;

	auto message_for(
		lint_rule rule,
		const text_edit& edit
	) -> std::string;

	auto fix_title_for(
		lint_rule rule,
		const text_edit& edit
	) -> std::string;

	auto as_diagnostic(
		const lint_finding& finding
	) -> diagnostic;

	auto analyze_check(
		analysis::diagnostics_check& check
	) -> void;
}

namespace gse::ide::lint {
	auto edit_from(
		const analysis::qualified_use& use
	) -> std::optional<text_edit>;
}

auto gse::ide::lint::edit_from(const analysis::qualified_use& use) -> std::optional<text_edit> {
	if (use.line == 0 || use.column == 0 || use.end_line == 0 || use.end_column == 0) {
		return std::nullopt;
	}
	const std::uint32_t line = use.line - 1;
	const std::uint32_t end_line = std::max(use.end_line, use.line) - 1;
	const std::uint32_t start_col = use.column - 1;
	const std::uint32_t end_col = use.end_column - 1;
	if (end_line == line && end_col <= start_col) {
		return std::nullopt;
	}
	return text_edit{
		.line = line,
		.end_line = end_line,
		.start_col = start_col,
		.end_col = end_col,
	};
}

auto gse::ide::lint::findings(const records emitted) -> std::vector<lint_finding> {
	std::vector<lint_finding> out;
	out.reserve(emitted.quals.size() + emitted.template_args.size() + emitted.unused_locals.size());

	const auto add_uses = [&out](const std::span<const analysis::qualified_use> uses, const lint_rule rule) {
		for (const analysis::qualified_use& use : uses) {
			if (const std::optional<text_edit> edit = edit_from(use)) {
				out.push_back({
					.file = use.file,
					.rule = rule,
					.edit = *edit,
				});
			}
		}
	};

	add_uses(emitted.quals, lint_rule::redundant_namespace_qualifier);
	add_uses(emitted.template_args, lint_rule::redundant_template_arguments);

	for (const analysis::unused_local& local : emitted.unused_locals) {
		if (local.line == 0 || local.column == 0 || local.end_column <= local.column || local.name.empty()) {
			continue;
		}
		out.push_back({
			.file = local.file,
			.rule = lint_rule::unused_name_placeholder,
			.edit = {
				.line = local.line - 1,
				.end_line = local.line - 1,
				.start_col = local.column - 1,
				.end_col = local.end_column - 1,
				.expected = local.name,
				.replacement = "_",
			},
		});
	}
	return out;
}

auto gse::ide::lint::message_for(const lint_rule rule, const text_edit& edit) -> std::string {
	const lint_rule_info info = annotation_from_enum<lint_rule_info>(rule, {});
	return std::vformat(info.message, std::make_format_args(edit.expected));
}

auto gse::ide::lint::fix_title_for(const lint_rule rule, const text_edit& edit) -> std::string {
	const lint_rule_info info = annotation_from_enum<lint_rule_info>(rule, {});
	return std::vformat(info.fix_title, std::make_format_args(edit.expected));
}

auto gse::ide::lint::as_diagnostic(const lint_finding& finding) -> diagnostic {
	return {
		.line = finding.edit.line,
		.end_line = finding.edit.end_line,
		.start_col = finding.edit.start_col,
		.end_col = finding.edit.end_col,
		.level = severity::hint,
		.rule = finding.rule,
		.message = message_for(finding.rule, finding.edit),
		.file = finding.file,
		.fix = fix_it{
			.title = fix_title_for(finding.rule, finding.edit),
			.edits = { finding.edit },
		},
	};
}

auto gse::ide::lint::analyze_check(analysis::diagnostics_check& check) -> void {
	check.lint.clear();

	const std::vector<lint_finding> found = findings({
		.quals = check.quals,
		.template_args = check.template_args,
		.unused_locals = check.unused_locals,
	});

	std::unordered_set<std::uint64_t> seen;
	check.lint.reserve(found.size());
	for (const lint_finding& finding : found) {
		const std::uint64_t key = (static_cast<std::uint64_t>(finding.edit.line) << 32) | finding.edit.start_col;
		if (!seen.insert(key).second) {
			continue;
		}
		check.lint.push_back(as_diagnostic(finding));
	}
}

export module gse.complete:hover;

import std;

import gse.meta;
import gse.syntax;

import :context;
import :model;
import :query;

export namespace gse::complete {
	struct hover_info {
		position from;
		position to;
		entity_kind kind = entity_kind::symbol;
		std::string title;
		std::string detail;
	};

	auto describe_at(
		const sources& from,
		position at
	) -> std::optional<hover_info>;
}

namespace gse::complete {
	auto title_of(
		const entity& e
	) -> std::string;

	auto owner_of(
		const environment& env,
		const target& owner,
		access_op via
	) -> std::string;
}

auto gse::complete::title_of(const entity& e) -> std::string {
	if (info_of(e.kind).callable) {
		return signature_of(e).label;
	}
	if (e.type.display.empty()) {
		return e.name;
	}
	if (e.kind == entity_kind::type) {
		return e.type.display;
	}
	return std::format("{} {}", e.type.display, e.name);
}

auto gse::complete::owner_of(const environment& env, const target& owner, const access_op via) -> std::string {
	if (owner.name_space) {
		return std::format(" in {}", *owner.name_space);
	}
	if (owner.type) {
		return std::format(" of {}", via == access_op::arrow ? dereference(env, *owner.type).key : owner.type->key);
	}
	return {};
}

auto gse::complete::describe_at(const sources& from, const position at) -> std::optional<hover_info> {
	if (at.line >= from.lines.size()) {
		return std::nullopt;
	}
	const std::string_view row = from.lines[at.line];
	std::uint32_t end = std::min<std::uint32_t>(at.column, static_cast<std::uint32_t>(row.size()));
	while (end < row.size() && syntax::is_ident_char(row[end])) {
		++end;
	}

	const position caret{ .line = at.line, .column = end };
	const context where = context_at(from.lines, caret);
	if (where.kind == trigger::none || where.prefix.empty() || at.column < where.replace_from.column || !where.receiver_resolvable) {
		return std::nullopt;
	}

	std::vector<link> links = where.receiver;
	links.push_back({ .via = where.op, .name = where.prefix });

	const syntax::lex_result lexed = syntax::tokenize(from.lines);
	const environment env = environment_at(from, lexed.tokens, caret);
	const name_lookup resolved = lookup_last(env, links);
	const target& found = resolved.found;

	hover_info out{
		.from = where.replace_from,
		.to = caret,
	};

	if (found.name_space) {
		out.kind = entity_kind::name_space;
		out.title = std::format("namespace {}", *found.name_space);
		return out;
	}

	if (found.candidates.empty()) {
		if (!found.names_type) {
			return std::nullopt;
		}
		out.kind = entity_kind::type;
		out.title = found.type->display;
		out.detail = std::format("{}{}", entity_kind::type, owner_of(env, resolved.owner, where.op));
		return out;
	}

	const entity& first = *found.candidates.front();
	out.kind = first.kind;
	out.title = title_of(first);
	out.detail = std::format("{}{}", first.kind, owner_of(env, resolved.owner, where.op));
	if (const std::size_t others = found.candidates.size() - 1; others > 0 && info_of(first.kind).callable) {
		out.detail += std::format("  +{} overload{}", others, others == 1 ? "" : "s");
	}
	return out;
}

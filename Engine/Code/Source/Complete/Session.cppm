export module gse.complete:session;

import std;

import :context;
import :model;
import :query;

export namespace gse::complete {
	struct session_input {
		const sources& from;
		position caret;
		bool edited = false;
		bool requested = false;
		std::uint64_t generation = 0;
	};

	struct session {
		bool open = false;
		completion current;
		std::optional<signature_help> signature;
		bool tail_empty = false;
		position caret;
		std::uint64_t generation = 0;
		std::uint64_t serial = 0;
		bool primed = false;
		std::optional<position> accepted_at;
	};

	struct accept_edit {
		position from;
		position to;
		std::string text;
	};

	constexpr std::size_t identifier_trigger = 2;

	auto update(
		session& s,
		const session_input& in
	) -> void;

	auto dismiss(
		session& s
	) -> void;

	auto accept(
		session& s,
		std::size_t index
	) -> std::optional<accept_edit>;

	auto ghost_of(
		const session& s,
		std::size_t index
	) -> std::string_view;

	auto signature_counter(
		const signature_help& help
	) -> std::string;
}

auto gse::complete::update(session& s, const session_input& in) -> void {
	const bool own_edit = s.accepted_at == in.caret;
	s.accepted_at.reset();
	const bool edited = in.edited && !own_edit;
	const bool moved = !s.primed || in.caret != s.caret;
	const bool refreshed = in.generation != s.generation;
	s.primed = true;
	s.caret = in.caret;
	s.generation = in.generation;
	if (!edited && !in.requested && !moved && !refreshed) {
		return;
	}

	s.signature = signature_at(in.from, in.caret);

	if (!s.open && !edited && !in.requested) {
		return;
	}
	if (s.open && !edited && !in.requested && moved) {
		s.open = false;
		return;
	}

	completion found = complete_at(in.from, in.caret);
	const context& where = found.where;
	bool wanted = s.open || in.requested;
	if (edited) {
		if (where.kind == trigger::member || where.kind == trigger::scope) {
			wanted = true;
		}
		else if (where.kind == trigger::identifier) {
			wanted = in.requested || (s.open ? !where.prefix.empty() : where.prefix.size() >= identifier_trigger);
		}
		else {
			wanted = false;
		}
	}
	if (where.kind == trigger::none || found.items.empty()) {
		wanted = false;
	}

	s.open = wanted;
	if (!wanted) {
		return;
	}
	s.current = std::move(found);
	++s.serial;

	s.tail_empty = true;
	if (in.caret.line < in.from.lines.size()) {
		const std::string_view line = in.from.lines[in.caret.line];
		for (std::size_t i = std::min<std::size_t>(s.current.where.replace_to.column, line.size()); i < line.size(); ++i) {
			if (line[i] != ' ' && line[i] != '\t' && line[i] != ';' && line[i] != ')') {
				s.tail_empty = false;
				break;
			}
		}
	}
}

auto gse::complete::dismiss(session& s) -> void {
	s.open = false;
}

auto gse::complete::accept(session& s, const std::size_t index) -> std::optional<accept_edit> {
	if (!s.open || index >= s.current.items.size()) {
		return std::nullopt;
	}
	const candidate& picked = s.current.items[index];
	accept_edit out{
		.from = s.current.where.replace_from,
		.to = s.current.where.replace_to,
		.text = picked.label,
	};
	if (info_of(picked.kind).callable) {
		out.text += '(';
	}
	s.open = false;
	s.accepted_at = position{
		.line = out.from.line,
		.column = out.from.column + static_cast<std::uint32_t>(out.text.size()),
	};
	return out;
}

auto gse::complete::ghost_of(const session& s, const std::size_t index) -> std::string_view {
	if (!s.open || !s.tail_empty || index >= s.current.items.size()) {
		return {};
	}
	const std::string_view label = s.current.items[index].label;
	const std::string_view prefix = s.current.where.prefix;
	if (s.current.where.replace_to != s.caret || !label.starts_with(prefix) || label.size() == prefix.size()) {
		return {};
	}
	return label.substr(prefix.size());
}

auto gse::complete::signature_counter(const signature_help& help) -> std::string {
	if (help.overloads.size() < 2) {
		return {};
	}
	return std::format("{}/{}", help.active + 1, help.overloads.size());
}

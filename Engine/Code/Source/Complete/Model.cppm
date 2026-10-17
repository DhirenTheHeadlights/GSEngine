export module gse.complete:model;

import std;

import gse.core;
import gse.meta;
import gse.syntax;

export namespace gse::complete {
	struct position {
		std::uint32_t line = 0;
		std::uint32_t column = 0;

		friend auto operator<=>(const position&, const position&) = default;
	};

	struct entity_info {
		char badge[4] = "";
		std::uint32_t color = 0;
		bool callable = false;
	};

	enum class entity_kind : std::uint8_t {
		variable [[= entity_info{ .badge = "v", .color = 0x9cdcfe }]],
		parameter [[= entity_info{ .badge = "p", .color = 0x9cdcfe }]],
		field [[= entity_info{ .badge = "m", .color = 0x7fc8c6 }]],
		method [[= entity_info{ .badge = "f", .color = 0xdcdcaa, .callable = true }]],
		function [[= entity_info{ .badge = "f", .color = 0xdcdcaa, .callable = true }]],
		type [[= entity_info{ .badge = "T", .color = 0x4ec9b0 }]],
		enumerator [[= entity_info{ .badge = "e", .color = 0xb5cea8 }]],
		name_space [[= entity_info{ .badge = "n", .color = 0xc586c0 }]],
		symbol [[= entity_info{ .badge = "s", .color = 0xa0b4c8 }]],
		keyword [[= entity_info{ .badge = "k", .color = 0x569cd6 }]],
		word [[= entity_info{ .badge = "w", .color = 0x9ca5b8 }]]
	};

	auto info_of(
		entity_kind kind
	) -> entity_info;

	enum class member_access : std::uint8_t {
		everyone,
		derived,
		owner
	};

	struct type_ref {
		std::string key;
		std::uint8_t indirection = 0;
		std::string display;
	};

	struct parameter {
		std::string type;
		std::string name;
	};

	struct entity {
		std::string name;
		entity_kind kind = entity_kind::symbol;
		type_ref type;
		std::vector<parameter> params;
	};

	struct member {
		entity what;
		member_access access = member_access::everyone;
		bool is_static = false;
		bool is_const = false;
	};

	struct type_entry {
		std::string key;
		std::vector<std::string> bases;
		std::vector<member> members;
	};

	enum class scope_kind : std::uint8_t {
		name_space,
		record,
		function,
		block
	};

	struct scope {
		std::optional<std::uint32_t> parent;
		scope_kind kind = scope_kind::block;
		position from;
		position to;
		std::string owner;
		type_ref result;
		std::vector<std::string> usings;
	};

	struct declaration {
		std::uint32_t scope = 0;
		position at;
		entity what;
	};

	struct namespace_entry {
		std::string name_space;
		entity what;
	};

	struct model {
		std::vector<type_entry> types;
		std::vector<scope> scopes;
		std::vector<declaration> declarations;
		std::vector<namespace_entry> namespace_entries;
		std::unordered_map<std::string, std::uint32_t, transparent_hash, transparent_equal> types_by_key;
		std::unordered_map<std::string, std::vector<std::uint32_t>, transparent_hash, transparent_equal> entries_by_namespace;
		std::vector<std::vector<std::uint32_t>> declarations_by_scope;

		[[nodiscard]] auto find_type(
			std::string_view key
		) const -> const type_entry*;

		[[nodiscard]] auto namespace_members(
			std::string_view name_space
		) const -> std::span<const std::uint32_t>;

		[[nodiscard]] auto innermost_scope(
			position at
		) const -> std::optional<std::uint32_t>;

		auto index() -> void;
	};

	auto parse_model(
		std::string_view text
	) -> model;

	auto parse_module_exports(
		std::string_view source
	) -> model;
}

namespace gse::complete {
	constexpr char parameter_separator = '\x1f';
	constexpr char parameter_field_separator = '\x1e';

	auto parse_number(
		std::string_view text
	) -> std::uint32_t;

	auto parse_position(
		std::string_view line,
		std::string_view column
	) -> position;

	auto parse_type(
		std::string_view key,
		std::string_view indirection,
		std::string_view display
	) -> type_ref;

	auto parse_params(
		std::string_view text
	) -> std::vector<parameter>;

	auto parse_kind(
		std::string_view text
	) -> entity_kind;

	auto parse_scope_kind(
		std::string_view text
	) -> scope_kind;
}

auto gse::complete::info_of(const entity_kind kind) -> entity_info {
	return annotation_from_enum(kind, entity_info{ .badge = "?", .color = 0x9ca5b8 });
}

auto gse::complete::model::find_type(const std::string_view key) const -> const type_entry* {
	const auto found = types_by_key.find(key);
	return found == types_by_key.end() ? nullptr : &types[found->second];
}

auto gse::complete::model::namespace_members(const std::string_view name_space) const -> std::span<const std::uint32_t> {
	const auto found = entries_by_namespace.find(name_space);
	return found == entries_by_namespace.end() ? std::span<const std::uint32_t>{} : std::span<const std::uint32_t>(found->second);
}

auto gse::complete::model::innermost_scope(const position at) const -> std::optional<std::uint32_t> {
	std::optional<std::uint32_t> best;
	for (std::uint32_t i = 0; i < scopes.size(); ++i) {
		const scope& s = scopes[i];
		if (at < s.from || s.to < at) {
			continue;
		}
		if (!best || scopes[*best].from <= s.from) {
			best = i;
		}
	}
	return best;
}

auto gse::complete::model::index() -> void {
	types_by_key.clear();
	for (std::uint32_t i = 0; i < types.size(); ++i) {
		types_by_key.try_emplace(types[i].key, i);
	}

	entries_by_namespace.clear();
	for (std::uint32_t i = 0; i < namespace_entries.size(); ++i) {
		entries_by_namespace[namespace_entries[i].name_space].push_back(i);
	}

	declarations_by_scope.assign(scopes.size(), {});
	for (std::uint32_t i = 0; i < declarations.size(); ++i) {
		if (declarations[i].scope < declarations_by_scope.size()) {
			declarations_by_scope[declarations[i].scope].push_back(i);
		}
	}
}

auto gse::complete::parse_number(const std::string_view text) -> std::uint32_t {
	std::uint32_t value = 0;
	std::from_chars(text.data(), text.data() + text.size(), value);
	return value;
}

auto gse::complete::parse_position(const std::string_view line, const std::string_view column) -> position {
	const std::uint32_t l = parse_number(line);
	const std::uint32_t c = parse_number(column);
	return {
		.line = l > 0 ? l - 1 : 0,
		.column = c > 0 ? c - 1 : 0,
	};
}

auto gse::complete::parse_type(const std::string_view key, const std::string_view indirection, const std::string_view display) -> type_ref {
	return {
		.key = std::string(key),
		.indirection = static_cast<std::uint8_t>(std::min<std::uint32_t>(parse_number(indirection), 255)),
		.display = std::string(display),
	};
}

auto gse::complete::parse_params(const std::string_view text) -> std::vector<parameter> {
	std::vector<parameter> out;
	if (text.empty()) {
		return out;
	}
	for (const auto piece : std::views::split(text, parameter_separator)) {
		const std::string_view entry(piece.begin(), piece.end());
		const std::size_t split = entry.find(parameter_field_separator);
		out.push_back({
			.type = std::string(entry.substr(0, split)),
			.name = split == std::string_view::npos ? std::string() : std::string(entry.substr(split + 1)),
		});
	}
	return out;
}

auto gse::complete::parse_kind(const std::string_view text) -> entity_kind {
	entity_kind kind = entity_kind::symbol;
	enum_from_string(text, kind);
	return kind;
}

auto gse::complete::parse_scope_kind(const std::string_view text) -> scope_kind {
	scope_kind kind = scope_kind::block;
	enum_from_string(text, kind);
	return kind;
}

auto gse::complete::parse_model(const std::string_view text) -> model {
	model out;
	std::unordered_map<std::string, std::uint32_t, transparent_hash, transparent_equal> type_slots;
	std::unordered_map<std::uint32_t, std::uint32_t> scope_slots;

	auto type_slot = [&](const std::string_view key) -> type_entry& {
		const auto [it, inserted] = type_slots.try_emplace(std::string(key), static_cast<std::uint32_t>(out.types.size()));
		if (inserted) {
			out.types.push_back({ .key = std::string(key) });
		}
		return out.types[it->second];
	};

	std::array<std::string_view, 12> fields;
	std::size_t cursor = 0;
	while (const std::optional<std::string_view> record = next_line(text, cursor)) {
		const std::string_view line = *record;
		if (!line.starts_with("GSEC")) {
			continue;
		}
		const std::size_t count = split_fields(line, '\t', fields);
		const std::string_view tag = fields[0];

		if (tag == "GSECTYPE" && count >= 2) {
			type_slot(fields[1]);
		}
		else if (tag == "GSECBASE" && count >= 3) {
			type_slot(fields[1]).bases.emplace_back(fields[2]);
		}
		else if (tag == "GSECMEMBER" && count >= 9) {
			const std::string_view flags = fields[4];
			type_slot(fields[1]).members.push_back({
				.what = {
					.name = std::string(fields[2]),
					.kind = parse_kind(fields[3]),
					.type = parse_type(fields[5], fields[6], fields[7]),
					.params = parse_params(fields[8]),
				},
				.access = flags.contains('p') ? member_access::owner : flags.contains('r') ? member_access::derived : member_access::everyone,
				.is_static = flags.contains('s'),
				.is_const = flags.contains('c'),
			});
		}
		else if (tag == "GSECSCOPE" && count >= 12) {
			const std::uint32_t id = parse_number(fields[1]);
			const std::optional<std::uint32_t> parent = fields[2] == "-1" ? std::nullopt : std::optional(parse_number(fields[2]));
			scope_slots.try_emplace(id, static_cast<std::uint32_t>(out.scopes.size()));
			out.scopes.push_back({
				.parent = parent,
				.kind = parse_scope_kind(fields[3]),
				.from = parse_position(fields[4], fields[5]),
				.to = parse_position(fields[6], fields[7]),
				.owner = std::string(fields[8]),
				.result = parse_type(fields[9], fields[10], fields[11]),
			});
		}
		else if (tag == "GSECUSING" && count >= 3) {
			if (const auto found = scope_slots.find(parse_number(fields[1])); found != scope_slots.end()) {
				out.scopes[found->second].usings.emplace_back(fields[2]);
			}
		}
		else if (tag == "GSECDECL" && count >= 10) {
			const auto found = scope_slots.find(parse_number(fields[1]));
			if (found == scope_slots.end()) {
				continue;
			}
			out.declarations.push_back({
				.scope = found->second,
				.at = parse_position(fields[4], fields[5]),
				.what = {
					.name = std::string(fields[2]),
					.kind = parse_kind(fields[3]),
					.type = parse_type(fields[6], fields[7], fields[8]),
					.params = parse_params(fields[9]),
				},
			});
		}
		else if (tag == "GSECNS" && count >= 8) {
			out.namespace_entries.push_back({
				.name_space = std::string(fields[1]),
				.what = {
					.name = std::string(fields[2]),
					.kind = parse_kind(fields[3]),
					.type = parse_type(fields[4], fields[5], fields[6]),
					.params = parse_params(fields[7]),
				},
			});
		}
	}

	for (scope& s : out.scopes) {
		if (s.parent) {
			const auto found = scope_slots.find(*s.parent);
			s.parent = found == scope_slots.end() ? std::nullopt : std::optional(found->second);
		}
	}

	out.index();
	return out;
}

auto gse::complete::parse_module_exports(const std::string_view source) -> model {
	model out;
	const std::vector<std::string_view> lines = syntax::split_lines(source);
	const syntax::lex_result lexed = syntax::tokenize(lines);
	const std::span<const syntax::token> tokens = lexed.tokens;

	std::vector<std::string> stack;
	std::unordered_set<std::string> seen;

	auto current = [&] {
		std::string joined;
		for (const std::string& part : stack) {
			if (part.empty()) {
				continue;
			}
			if (!joined.empty()) {
				joined += "::";
			}
			joined += part;
		}
		return joined;
	};

	auto punctuation_has = [](const syntax::token& t, const char c) {
		return t.type == syntax::token_type::punctuation && t.text.contains(c);
	};

	for (std::size_t i = 0; i < tokens.size(); ++i) {
		const syntax::token& t = tokens[i];
		if (t.type == syntax::token_type::identifier && t.text == "namespace" && i + 1 < tokens.size() && tokens[i + 1].type == syntax::token_type::identifier) {
			std::string name(tokens[i + 1].text);
			std::size_t j = i + 2;
			while (j + 1 < tokens.size() && tokens[j].text == "::" && tokens[j + 1].type == syntax::token_type::identifier) {
				name += "::";
				name += tokens[j + 1].text;
				j += 2;
			}
			if (j < tokens.size() && punctuation_has(tokens[j], '{')) {
				stack.push_back(std::move(name));
				i = j;
			}
			continue;
		}
		if (t.type == syntax::token_type::punctuation) {
			for (const char c : t.text) {
				if (c == '{') {
					stack.emplace_back();
				}
				else if (c == '}' && !stack.empty()) {
					stack.pop_back();
				}
			}
			continue;
		}
		if (t.type != syntax::token_type::identifier || t.text != "using" || stack.empty()) {
			continue;
		}
		if (i + 1 < tokens.size() && tokens[i + 1].text == "namespace") {
			continue;
		}
		std::string_view last;
		std::size_t j = i + 1;
		for (; j < tokens.size() && !punctuation_has(tokens[j], ';'); ++j) {
			if (tokens[j].type == syntax::token_type::identifier) {
				last = tokens[j].text;
			}
		}
		i = j;
		if (last.empty() || last == "operator") {
			continue;
		}
		std::string name_space = current();
		std::string key = name_space + "::" + std::string(last);
		if (!seen.insert(std::move(key)).second) {
			continue;
		}
		out.namespace_entries.push_back({
			.name_space = std::move(name_space),
			.what = {
				.name = std::string(last),
				.kind = entity_kind::symbol,
			},
		});
	}

	std::vector<std::string> namespaces;
	for (const namespace_entry& entry : out.namespace_entries) {
		namespaces.push_back(entry.name_space);
	}
	std::ranges::sort(namespaces);
	namespaces.erase(std::ranges::unique(namespaces).begin(), namespaces.end());

	for (const std::string& qualified : namespaces) {
		std::size_t from = 0;
		while (from < qualified.size()) {
			const std::size_t next = qualified.find("::", from);
			std::string parent = from == 0 ? std::string() : qualified.substr(0, from - 2);
			std::string child = qualified.substr(from, next == std::string::npos ? std::string::npos : next - from);
			from = next == std::string::npos ? qualified.size() : next + 2;
			if (!seen.insert(parent + "::" + child).second) {
				continue;
			}
			out.namespace_entries.push_back({
				.name_space = std::move(parent),
				.what = {
					.name = std::move(child),
					.kind = entity_kind::name_space,
				},
			});
		}
	}

	out.index();
	return out;
}

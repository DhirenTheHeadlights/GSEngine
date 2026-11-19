export module gse.complete:context;

import std;

import gse.syntax;

import :model;

export namespace gse::complete {
	enum class trigger : std::uint8_t {
		none,
		identifier,
		member,
		scope
	};

	enum class access_op : std::uint8_t {
		none,
		dot,
		arrow,
		scope
	};

	enum class postfix : std::uint8_t {
		call,
		subscript
	};

	struct link {
		access_op via = access_op::none;
		std::string name;
		std::vector<postfix> postfixes;
	};

	struct call_site {
		std::vector<link> callee;
		std::uint32_t argument = 0;
		position open;
	};

	struct context {
		trigger kind = trigger::none;
		access_op op = access_op::none;
		std::vector<link> receiver;
		bool receiver_resolvable = true;
		std::string prefix;
		position replace_from;
		position replace_to;
		std::optional<call_site> call;
		std::vector<link> assigned;
		bool after_return = false;
	};

	auto context_at(
		std::span<const std::string_view> lines,
		position caret
	) -> context;
}

namespace gse::complete {
	enum class item_type : std::uint8_t {
		identifier,
		number,
		text,
		op
	};

	struct item {
		item_type type = item_type::op;
		std::string_view text;
		position from;
	};

	struct chain_parse {
		std::vector<link> links;
		bool resolvable = true;
	};

	constexpr std::size_t call_scan_limit = 4096;

	auto items_of(
		std::span<const syntax::token> tokens
	) -> std::vector<item>;

	auto is_op(
		const item& it,
		std::string_view text
	) -> bool;

	auto skip_back_balanced(
		std::span<const item> items,
		std::size_t close,
		std::string_view open_text,
		std::string_view close_text
	) -> std::optional<std::size_t>;

	auto parse_chain_back(
		std::span<const item> items,
		std::size_t last
	) -> chain_parse;

	auto find_call_site(
		std::span<const item> items,
		std::size_t end
	) -> std::optional<call_site>;

	auto inside_literal(
		std::span<const syntax::token> tokens,
		position caret
	) -> bool;
}

auto gse::complete::items_of(const std::span<const syntax::token> tokens) -> std::vector<item> {
	std::vector<item> out;
	out.reserve(tokens.size() * 2);
	for (const syntax::token& t : tokens) {
		switch (t.type) {
			case syntax::token_type::identifier:
				out.push_back({ .type = item_type::identifier, .text = t.text, .from = { .line = t.line, .column = t.start } });
				break;
			case syntax::token_type::number:
				out.push_back({ .type = item_type::number, .text = t.text, .from = { .line = t.line, .column = t.start } });
				break;
			case syntax::token_type::string:
				out.push_back({ .type = item_type::text, .text = t.text, .from = { .line = t.line, .column = t.start } });
				break;
			case syntax::token_type::punctuation: {
				std::size_t i = 0;
				while (i < t.text.size()) {
					const std::string_view rest = t.text.substr(i);
					const std::size_t length = rest.starts_with("::") || rest.starts_with("->") ? 2 : 1;
					out.push_back({
						.type = item_type::op,
						.text = rest.substr(0, length),
						.from = { .line = t.line, .column = t.start + static_cast<std::uint32_t>(i) },
					});
					i += length;
				}
				break;
			}
			default:
				break;
		}
	}
	return out;
}

auto gse::complete::is_op(const item& it, const std::string_view text) -> bool {
	return it.type == item_type::op && it.text == text;
}

auto gse::complete::skip_back_balanced(const std::span<const item> items, const std::size_t close, const std::string_view open_text, const std::string_view close_text) -> std::optional<std::size_t> {
	std::uint32_t depth = 0;
	for (std::size_t i = close + 1; i-- > 0;) {
		const item& it = items[i];
		if (is_op(it, close_text)) {
			++depth;
		}
		else if (is_op(it, open_text)) {
			if (--depth == 0) {
				return i;
			}
		}
		else if (is_op(it, ";") || is_op(it, "{") || is_op(it, "}")) {
			return std::nullopt;
		}
	}
	return std::nullopt;
}

auto gse::complete::parse_chain_back(const std::span<const item> items, std::size_t last) -> chain_parse {
	chain_parse out;
	if (last >= items.size()) {
		return out;
	}

	while (true) {
		std::vector<postfix> postfixes;
		std::size_t at = last;
		while (is_op(items[at], ")") || is_op(items[at], "]")) {
			const bool call = is_op(items[at], ")");
			const std::optional<std::size_t> open = call ? skip_back_balanced(items, at, "(", ")") : skip_back_balanced(items, at, "[", "]");
			if (!open || *open == 0) {
				out.resolvable = false;
				return out;
			}
			postfixes.insert(postfixes.begin(), call ? postfix::call : postfix::subscript);
			at = *open - 1;
		}
		if (is_op(items[at], ">")) {
			const std::optional<std::size_t> open = skip_back_balanced(items, at, "<", ">");
			if (!open || *open == 0) {
				out.resolvable = false;
				return out;
			}
			at = *open - 1;
		}
		if (items[at].type != item_type::identifier) {
			out.resolvable = false;
			return out;
		}

		link current{
			.name = std::string(items[at].text),
			.postfixes = std::move(postfixes),
		};

		if (at == 0) {
			out.links.insert(out.links.begin(), std::move(current));
			return out;
		}
		const item& before = items[at - 1];
		const access_op via = is_op(before, ".") ? access_op::dot : is_op(before, "->") ? access_op::arrow : is_op(before, "::") ? access_op::scope : access_op::none;
		current.via = via;
		out.links.insert(out.links.begin(), std::move(current));
		if (via == access_op::none) {
			return out;
		}
		if (at < 2) {
			return out;
		}
		if (via == access_op::scope && items[at - 2].type != item_type::identifier && !is_op(items[at - 2], ">")) {
			return out;
		}
		last = at - 2;
	}
}

auto gse::complete::find_call_site(const std::span<const item> items, const std::size_t end) -> std::optional<call_site> {
	std::uint32_t depth = 0;
	std::uint32_t argument = 0;
	const std::size_t floor = end > call_scan_limit ? end - call_scan_limit : 0;
	for (std::size_t i = end; i-- > floor;) {
		const item& it = items[i];
		if (it.type != item_type::op) {
			continue;
		}
		if (it.text == ")" || it.text == "]" || it.text == "}") {
			++depth;
			continue;
		}
		if (it.text == "(" || it.text == "[" || it.text == "{") {
			if (depth > 0) {
				--depth;
				continue;
			}
			if (it.text != "(" || i == 0) {
				return std::nullopt;
			}
			chain_parse callee = parse_chain_back(items, i - 1);
			if (!callee.resolvable || callee.links.empty()) {
				return std::nullopt;
			}
			return call_site{
				.callee = std::move(callee.links),
				.argument = argument,
				.open = it.from,
			};
		}
		if (depth == 0 && it.text == ",") {
			++argument;
		}
		else if (depth == 0 && it.text == ";") {
			return std::nullopt;
		}
	}
	return std::nullopt;
}

auto gse::complete::inside_literal(const std::span<const syntax::token> tokens, const position caret) -> bool {
	for (const syntax::token& t : tokens | std::views::reverse) {
		if (t.line != caret.line) {
			return t.type == syntax::token_type::block_comment && !t.text.ends_with("*/");
		}
		if (t.type == syntax::token_type::line_comment) {
			return true;
		}
		if (t.end < caret.column) {
			continue;
		}
		if (t.type == syntax::token_type::block_comment) {
			return !t.text.ends_with("*/") || t.text.size() < 4;
		}
		if (t.type == syntax::token_type::string) {
			return t.text.size() < 2 || (t.text.back() != '"' && t.text.back() != '\'');
		}
		if (t.type == syntax::token_type::preprocessor) {
			return true;
		}
	}
	return false;
}

auto gse::complete::context_at(const std::span<const std::string_view> lines, const position caret) -> context {
	context out{
		.replace_from = caret,
		.replace_to = caret,
	};
	if (caret.line >= lines.size()) {
		return out;
	}

	const std::string_view caret_line = lines[caret.line];
	const std::uint32_t column = std::min<std::uint32_t>(caret.column, static_cast<std::uint32_t>(caret_line.size()));

	std::vector<std::string_view> window(lines.begin(), lines.begin() + caret.line + 1);
	window.back() = caret_line.substr(0, column);
	const syntax::lex_result lexed = syntax::tokenize(window);

	if (inside_literal(lexed.tokens, { .line = caret.line, .column = column })) {
		return out;
	}
	const auto first = std::ranges::find_if(lexed.tokens, [&](const syntax::token& t) {
		return t.line == caret.line;
	});
	if (first != lexed.tokens.end() && first->type == syntax::token_type::preprocessor && (first->text.contains("include") || first->text.contains("import"))) {
		return out;
	}

	const std::vector<item> items = items_of(lexed.tokens);
	std::size_t end = items.size();

	std::uint32_t tail = column;
	while (tail < caret_line.size() && syntax::is_ident_char(caret_line[tail])) {
		++tail;
	}
	out.replace_to = { .line = caret.line, .column = tail };

	if (end > 0) {
		const item& last = items[end - 1];
		const bool touches = last.from.line == caret.line && last.from.column + last.text.size() == column;
		if (touches && last.type == item_type::number) {
			return out;
		}
		if (touches && last.type == item_type::identifier) {
			out.prefix = std::string(last.text);
			out.replace_from = last.from;
			--end;
		}
	}

	out.kind = trigger::identifier;
	if (end > 0) {
		const item& before = items[end - 1];
		const access_op op = is_op(before, ".") ? access_op::dot : is_op(before, "->") ? access_op::arrow : is_op(before, "::") ? access_op::scope : access_op::none;
		if (op != access_op::none) {
			out.kind = op == access_op::scope ? trigger::scope : trigger::member;
			out.op = op;
			if (end >= 2 && (op != access_op::scope || items[end - 2].type == item_type::identifier || is_op(items[end - 2], ">"))) {
				chain_parse receiver = parse_chain_back(items, end - 2);
				out.receiver = std::move(receiver.links);
				out.receiver_resolvable = receiver.resolvable;
			}
			else if (op != access_op::scope) {
				out.receiver_resolvable = false;
			}
		}
	}

	if (out.kind == trigger::identifier && end > 0) {
		const item& before = items[end - 1];
		const bool comparison = end >= 2 && items[end - 2].type == item_type::op && (items[end - 2].text == "=" || items[end - 2].text == "!" || items[end - 2].text == "<" || items[end - 2].text == ">");
		if (is_op(before, "=") && end >= 2 && !comparison) {
			chain_parse assigned = parse_chain_back(items, end - 2);
			if (assigned.resolvable) {
				out.assigned = std::move(assigned.links);
			}
		}
		out.after_return = before.type == item_type::identifier && before.text == "return";
	}

	out.call = find_call_site(items, end);
	return out;
}

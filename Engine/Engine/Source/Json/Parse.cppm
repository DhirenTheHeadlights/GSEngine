export module gse.json:parse;

import std;

import gse.meta;

import :value;

export namespace gse::json {
	struct error_info {
		char message[48];
	};

	enum struct error : std::uint8_t {
		unexpected_end [[= error_info{
			.message = "unexpected end of input",
		}]],
		unexpected_token [[= error_info{
			.message = "unexpected token",
		}]],
		bad_escape [[= error_info{
			.message = "invalid escape sequence",
		}]],
		bad_number [[= error_info{
			.message = "invalid number",
		}]],
		bad_string [[= error_info{
			.message = "invalid string",
		}]],
		bad_surrogate [[= error_info{
			.message = "invalid surrogate pair",
		}]],
		depth_exceeded [[= error_info{
			.message = "nesting too deep",
		}]],
		trailing_content [[= error_info{
			.message = "trailing content after value",
		}]],
		type_mismatch [[= error_info{
			.message = "value does not match the target type",
		}]],
		missing_field [[= error_info{
			.message = "a required field is missing",
		}]]
	};

	struct parse_error {
		error code = error::unexpected_token;
		std::size_t offset = 0;
	};

	auto parse(
		std::string_view text
	) -> std::expected<value, parse_error>;

	constexpr auto error_of(
		error code
	) -> error_info;

	auto message(
		error code
	) -> std::string;
}

namespace gse::json {
	constexpr int max_depth = 200;

	auto append_utf8(
		std::string& out,
		std::uint32_t code_point
	) -> void;

	struct parser {
		std::string_view source;
		std::size_t cursor = 0;
		error code = error::unexpected_token;
		std::size_t failed_at = 0;

		auto fail(
			error reason
		) -> bool;

		auto skip_whitespace() -> void;

		auto peek() const -> char;

		auto expect(
			char c
		) -> bool;

		auto read_hex4(
			std::uint32_t& out
		) -> bool;

		auto read_string(
			std::string& out
		) -> bool;

		auto read_number(
			value& out
		) -> bool;

		auto read_literal(
			std::string_view word,
			value literal,
			value& out
		) -> bool;

		auto read_array(
			value& out,
			int depth
		) -> bool;

		auto read_object(
			value& out,
			int depth
		) -> bool;

		auto read_value(
			value& out,
			int depth
		) -> bool;
	};
}

constexpr auto gse::json::error_of(const error code) -> error_info {
	return annotation_from_enum(code, error_info{
		.message = "unknown error",
	});
}

auto gse::json::message(const error code) -> std::string {
	const error_info info = error_of(code);
	return info.message;
}

auto gse::json::append_utf8(std::string& out, const std::uint32_t code_point) -> void {
	if (code_point <= 0x7fu) {
		out.push_back(static_cast<char>(code_point));
	}
	else if (code_point <= 0x7ffu) {
		out.push_back(static_cast<char>(0xc0u | (code_point >> 6)));
		out.push_back(static_cast<char>(0x80u | (code_point & 0x3fu)));
	}
	else if (code_point <= 0xffffu) {
		out.push_back(static_cast<char>(0xe0u | (code_point >> 12)));
		out.push_back(static_cast<char>(0x80u | ((code_point >> 6) & 0x3fu)));
		out.push_back(static_cast<char>(0x80u | (code_point & 0x3fu)));
	}
	else {
		out.push_back(static_cast<char>(0xf0u | (code_point >> 18)));
		out.push_back(static_cast<char>(0x80u | ((code_point >> 12) & 0x3fu)));
		out.push_back(static_cast<char>(0x80u | ((code_point >> 6) & 0x3fu)));
		out.push_back(static_cast<char>(0x80u | (code_point & 0x3fu)));
	}
}

auto gse::json::parser::fail(const error reason) -> bool {
	code = reason;
	failed_at = cursor;
	return false;
}

auto gse::json::parser::skip_whitespace() -> void {
	while (cursor < source.size()) {
		const char c = source[cursor];
		if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
			return;
		}
		++cursor;
	}
}

auto gse::json::parser::peek() const -> char {
	return cursor < source.size() ? source[cursor] : '\0';
}

auto gse::json::parser::expect(const char c) -> bool {
	if (cursor >= source.size()) {
		return fail(error::unexpected_end);
	}
	if (source[cursor] != c) {
		return fail(error::unexpected_token);
	}
	++cursor;
	return true;
}

auto gse::json::parser::read_hex4(std::uint32_t& out) -> bool {
	if (cursor + 4 > source.size()) {
		return fail(error::unexpected_end);
	}
	out = 0;
	for (int n = 0; n < 4; ++n) {
		const char c = source[cursor++];
		out <<= 4;
		if (c >= '0' && c <= '9') {
			out |= static_cast<std::uint32_t>(c - '0');
		}
		else if (c >= 'a' && c <= 'f') {
			out |= static_cast<std::uint32_t>(c - 'a' + 10);
		}
		else if (c >= 'A' && c <= 'F') {
			out |= static_cast<std::uint32_t>(c - 'A' + 10);
		}
		else {
			return fail(error::bad_escape);
		}
	}
	return true;
}

auto gse::json::parser::read_string(std::string& out) -> bool {
	if (!expect('"')) {
		return false;
	}

	const std::size_t start = cursor;
	while (cursor < source.size() && source[cursor] != '"' && source[cursor] != '\\') {
		if (static_cast<unsigned char>(source[cursor]) < 0x20u) {
			return fail(error::bad_string);
		}
		++cursor;
	}

	out.assign(source.substr(start, cursor - start));

	if (cursor < source.size() && source[cursor] == '"') {
		++cursor;
		return true;
	}

	while (cursor < source.size()) {
		const char c = source[cursor++];
		if (c == '"') {
			return true;
		}
		if (static_cast<unsigned char>(c) < 0x20u) {
			return fail(error::bad_string);
		}
		if (c != '\\') {
			out.push_back(c);
			continue;
		}
		if (cursor >= source.size()) {
			return fail(error::unexpected_end);
		}
		const char escape = source[cursor++];
		switch (escape) {
			case '"':
				out.push_back('"');
				break;
			case '\\':
				out.push_back('\\');
				break;
			case '/':
				out.push_back('/');
				break;
			case 'b':
				out.push_back('\b');
				break;
			case 'f':
				out.push_back('\f');
				break;
			case 'n':
				out.push_back('\n');
				break;
			case 'r':
				out.push_back('\r');
				break;
			case 't':
				out.push_back('\t');
				break;
			case 'u': {
				std::uint32_t unit = 0;
				if (!read_hex4(unit)) {
					return false;
				}
				if (unit >= 0xd800u && unit <= 0xdbffu) {
					if (cursor + 1 >= source.size() || source[cursor] != '\\' || source[cursor + 1] != 'u') {
						return fail(error::bad_surrogate);
					}
					cursor += 2;
					std::uint32_t low = 0;
					if (!read_hex4(low)) {
						return false;
					}
					if (low < 0xdc00u || low > 0xdfffu) {
						return fail(error::bad_surrogate);
					}
					unit = 0x10000u + ((unit - 0xd800u) << 10) + (low - 0xdc00u);
				}
				else if (unit >= 0xdc00u && unit <= 0xdfffu) {
					return fail(error::bad_surrogate);
				}
				append_utf8(out, unit);
				break;
			}
			default:
				return fail(error::bad_escape);
		}
	}

	return fail(error::unexpected_end);
}

auto gse::json::parser::read_number(value& out) -> bool {
	const std::size_t start = cursor;
	bool integral = true;

	if (peek() == '-') {
		++cursor;
	}
	while (cursor < source.size()) {
		const char c = source[cursor];
		if (c >= '0' && c <= '9') {
			++cursor;
			continue;
		}
		if (c == '.' || c == 'e' || c == 'E') {
			integral = false;
			++cursor;
			continue;
		}
		if ((c == '+' || c == '-') && !integral && cursor > start) {
			const char previous = source[cursor - 1];
			if (previous == 'e' || previous == 'E') {
				++cursor;
				continue;
			}
		}
		break;
	}

	const std::string_view token = source.substr(start, cursor - start);
	if (token.empty()) {
		return fail(error::bad_number);
	}

	if (integral) {
		std::int64_t parsed = 0;
		const auto result = std::from_chars(token.data(), token.data() + token.size(), parsed);
		if (result.ec == std::errc{} && result.ptr == token.data() + token.size()) {
			out = value(parsed);
			return true;
		}
	}

	double parsed = 0.0;
	const auto result = std::from_chars(token.data(), token.data() + token.size(), parsed);
	if (result.ec != std::errc{} || result.ptr != token.data() + token.size()) {
		cursor = start;
		return fail(error::bad_number);
	}
	out = value(parsed);
	return true;
}

auto gse::json::parser::read_literal(const std::string_view word, value literal, value& out) -> bool {
	if (source.substr(cursor).starts_with(word)) {
		cursor += word.size();
		out = std::move(literal);
		return true;
	}
	return fail(error::unexpected_token);
}

auto gse::json::parser::read_array(value& out, const int depth) -> bool {
	if (!expect('[')) {
		return false;
	}
	out = value::make_array();
	skip_whitespace();
	if (peek() == ']') {
		++cursor;
		return true;
	}

	while (true) {
		value element;
		if (!read_value(element, depth + 1)) {
			return false;
		}
		out.push_back(std::move(element));
		skip_whitespace();
		if (peek() == ',') {
			++cursor;
			continue;
		}
		if (peek() == ']') {
			++cursor;
			return true;
		}
		return fail(cursor >= source.size() ? error::unexpected_end : error::unexpected_token);
	}
}

auto gse::json::parser::read_object(value& out, const int depth) -> bool {
	if (!expect('{')) {
		return false;
	}
	out = value::make_object();
	skip_whitespace();
	if (peek() == '}') {
		++cursor;
		return true;
	}

	while (true) {
		skip_whitespace();
		std::string key;
		if (!read_string(key)) {
			return false;
		}
		skip_whitespace();
		if (!expect(':')) {
			return false;
		}
		value member;
		if (!read_value(member, depth + 1)) {
			return false;
		}
		out.insert(std::move(key), std::move(member));
		skip_whitespace();
		if (peek() == ',') {
			++cursor;
			continue;
		}
		if (peek() == '}') {
			++cursor;
			return true;
		}
		return fail(cursor >= source.size() ? error::unexpected_end : error::unexpected_token);
	}
}

auto gse::json::parser::read_value(value& out, const int depth) -> bool {
	if (depth > max_depth) {
		return fail(error::depth_exceeded);
	}
	skip_whitespace();
	if (cursor >= source.size()) {
		return fail(error::unexpected_end);
	}

	const char c = source[cursor];
	switch (c) {
		case '{':
			return read_object(out, depth);
		case '[':
			return read_array(out, depth);
		case '"': {
			std::string text;
			if (!read_string(text)) {
				return false;
			}
			out = value(std::move(text));
			return true;
		}
		case 't':
			return read_literal("true", value(true), out);
		case 'f':
			return read_literal("false", value(false), out);
		case 'n':
			return read_literal("null", value{}, out);
		default:
			if (c == '-' || (c >= '0' && c <= '9')) {
				return read_number(out);
			}
			return fail(error::unexpected_token);
	}
}

auto gse::json::parse(const std::string_view text) -> std::expected<value, parse_error> {
	parser state{
		.source = text,
	};

	value root;
	if (!state.read_value(root, 0)) {
		return std::unexpected(parse_error{
			.code = state.code,
			.offset = state.failed_at,
		});
	}

	state.skip_whitespace();
	if (state.cursor != text.size()) {
		return std::unexpected(parse_error{
			.code = error::trailing_content,
			.offset = state.cursor,
		});
	}

	return root;
}
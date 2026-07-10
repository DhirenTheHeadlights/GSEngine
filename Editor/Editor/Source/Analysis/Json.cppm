export module gse.ide.analysis:json;

import std;

export namespace gse::ide::analysis::json {
	struct value {
		enum class kind { null, boolean, number, string, array, object };

		kind type = kind::null;
		bool boolean = false;
		double number = 0.0;
		std::string text;
		std::vector<std::string> keys;
		std::vector<value> children;

		auto find(std::string_view key) const -> const value* {
			for (std::size_t i = 0; i < keys.size(); ++i) {
				if (keys[i] == key) {
					return &children[i];
				}
			}
			return nullptr;
		}

		auto is_array() const -> bool {
			return type == kind::array;
		}

		auto is_object() const -> bool {
			return type == kind::object;
		}

		auto as_string() const -> std::string_view {
			return type == kind::string ? std::string_view(text) : std::string_view{};
		}

		auto as_int() const -> std::int64_t {
			return type == kind::number ? static_cast<std::int64_t>(number) : 0;
		}
	};

	auto parse(std::string_view text) -> std::optional<value>;
}

namespace gse::ide::analysis::json {
	auto append_utf8(std::string& out, std::uint32_t cp) -> void {
		if (cp <= 0x7fu) {
			out.push_back(static_cast<char>(cp));
		}
		else if (cp <= 0x7ffu) {
			out.push_back(static_cast<char>(0xc0u | (cp >> 6)));
			out.push_back(static_cast<char>(0x80u | (cp & 0x3fu)));
		}
		else if (cp <= 0xffffu) {
			out.push_back(static_cast<char>(0xe0u | (cp >> 12)));
			out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3fu)));
			out.push_back(static_cast<char>(0x80u | (cp & 0x3fu)));
		}
		else {
			out.push_back(static_cast<char>(0xf0u | (cp >> 18)));
			out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3fu)));
			out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3fu)));
			out.push_back(static_cast<char>(0x80u | (cp & 0x3fu)));
		}
	}

	struct parser {
		std::string_view s;
		std::size_t i = 0;

		auto skip_ws() -> void {
			while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) {
				++i;
			}
		}

		auto hex4(std::uint32_t& out) -> bool {
			if (i + 4 > s.size()) {
				return false;
			}
			out = 0;
			for (int n = 0; n < 4; ++n) {
				const char c = s[i++];
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
					return false;
				}
			}
			return true;
		}

		auto parse_string(std::string& out) -> bool {
			if (i >= s.size() || s[i] != '"') {
				return false;
			}
			++i;
			while (i < s.size()) {
				const char c = s[i++];
				if (c == '"') {
					return true;
				}
				if (c != '\\') {
					out.push_back(c);
					continue;
				}
				if (i >= s.size()) {
					return false;
				}
				const char e = s[i++];
				switch (e) {
					case '"': out.push_back('"'); break;
					case '\\': out.push_back('\\'); break;
					case '/': out.push_back('/'); break;
					case 'b': out.push_back('\b'); break;
					case 'f': out.push_back('\f'); break;
					case 'n': out.push_back('\n'); break;
					case 'r': out.push_back('\r'); break;
					case 't': out.push_back('\t'); break;
					case 'u': {
						std::uint32_t cp = 0;
						if (!hex4(cp)) {
							return false;
						}
						if (cp >= 0xd800u && cp <= 0xdbffu && i + 1 < s.size() && s[i] == '\\' && s[i + 1] == 'u') {
							i += 2;
							std::uint32_t lo = 0;
							if (!hex4(lo)) {
								return false;
							}
							cp = 0x10000u + ((cp - 0xd800u) << 10) + (lo - 0xdc00u);
						}
						append_utf8(out, cp);
						break;
					}
					default:
						return false;
				}
			}
			return false;
		}

		auto parse_number(value& out) -> bool {
			const std::size_t start = i;
			if (i < s.size() && (s[i] == '-' || s[i] == '+')) {
				++i;
			}
			while (i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '.' || s[i] == 'e' || s[i] == 'E' || s[i] == '+' || s[i] == '-')) {
				++i;
			}
			double d = 0.0;
			const auto [ptr, ec] = std::from_chars(s.data() + start, s.data() + i, d);
			if (ec != std::errc{}) {
				return false;
			}
			out.type = value::kind::number;
			out.number = d;
			return true;
		}

		auto parse_value(value& out) -> bool {
			skip_ws();
			if (i >= s.size()) {
				return false;
			}
			const char c = s[i];
			if (c == '{') {
				return parse_object(out);
			}
			if (c == '[') {
				return parse_array(out);
			}
			if (c == '"') {
				out.type = value::kind::string;
				return parse_string(out.text);
			}
			if (c == 't') {
				if (s.substr(i, 4) == "true") {
					i += 4;
					out.type = value::kind::boolean;
					out.boolean = true;
					return true;
				}
				return false;
			}
			if (c == 'f') {
				if (s.substr(i, 5) == "false") {
					i += 5;
					out.type = value::kind::boolean;
					out.boolean = false;
					return true;
				}
				return false;
			}
			if (c == 'n') {
				if (s.substr(i, 4) == "null") {
					i += 4;
					out.type = value::kind::null;
					return true;
				}
				return false;
			}
			return parse_number(out);
		}

		auto parse_array(value& out) -> bool {
			out.type = value::kind::array;
			++i;
			skip_ws();
			if (i < s.size() && s[i] == ']') {
				++i;
				return true;
			}
			while (i < s.size()) {
				value child;
				if (!parse_value(child)) {
					return false;
				}
				out.children.push_back(std::move(child));
				skip_ws();
				if (i >= s.size()) {
					return false;
				}
				if (s[i] == ',') {
					++i;
					continue;
				}
				if (s[i] == ']') {
					++i;
					return true;
				}
				return false;
			}
			return false;
		}

		auto parse_object(value& out) -> bool {
			out.type = value::kind::object;
			++i;
			skip_ws();
			if (i < s.size() && s[i] == '}') {
				++i;
				return true;
			}
			while (i < s.size()) {
				skip_ws();
				std::string key;
				if (!parse_string(key)) {
					return false;
				}
				skip_ws();
				if (i >= s.size() || s[i] != ':') {
					return false;
				}
				++i;
				value child;
				if (!parse_value(child)) {
					return false;
				}
				out.keys.push_back(std::move(key));
				out.children.push_back(std::move(child));
				skip_ws();
				if (i >= s.size()) {
					return false;
				}
				if (s[i] == ',') {
					++i;
					continue;
				}
				if (s[i] == '}') {
					++i;
					return true;
				}
				return false;
			}
			return false;
		}
	};
}

auto gse::ide::analysis::json::parse(std::string_view text) -> std::optional<value> {
	parser p{ text };
	value root;
	if (!p.parse_value(root)) {
		return std::nullopt;
	}
	return root;
}
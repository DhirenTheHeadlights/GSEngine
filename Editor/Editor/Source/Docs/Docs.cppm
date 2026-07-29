export module gse.ide.docs;

import std;
import gse;

export namespace gse::ide::docs {
	struct doc_card {
		std::string title;
		std::string body;
		bool from_cppref = false;
		std::filesystem::path source;
		std::uint32_t indent = 0;
		std::vector<std::uint32_t> line_source;
		std::vector<std::string> line_render;
	};

	struct cppref_entry {
		std::string kind;
		std::string page;
		std::string brief;
	};

	struct cppref_hit {
		std::string_view kind;
		std::string_view page;
		std::string_view brief;
	};

	struct cppref_index {
		std::unordered_map<std::string, cppref_entry, gse::transparent_hash, gse::transparent_equal> entries;
		bool loaded = false;
		auto load(const std::filesystem::path& file) -> void;
		auto find(std::string_view qualified) const -> std::optional<cppref_hit>;
	};

	auto cppref_url(std::string_view page) -> std::string;

	auto normalize_qualified(std::string_view raw) -> std::string;
	auto expand_qualified(std::string_view line, std::size_t ident_start) -> std::string;
	auto extract_header_doc(const std::filesystem::path& file, std::uint32_t line, std::string_view title) -> std::optional<doc_card>;
	auto extract_definition(const std::filesystem::path& file, std::uint32_t line, std::string_view title, bool signature_only) -> std::optional<doc_card>;
}

namespace gse::ide::docs {
	auto is_ident_char(char c) -> bool {
		return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
	}

	auto trim(std::string_view s) -> std::string_view {
		const std::size_t a = s.find_first_not_of(" \t");
		if (a == std::string_view::npos) {
			return {};
		}
		const std::size_t b = s.find_last_not_of(" \t");
		return s.substr(a, b - a + 1);
	}

	auto trim_full(std::string_view s) -> std::string_view {
		const std::size_t a = s.find_first_not_of(" \t\r\n");
		if (a == std::string_view::npos) {
			return {};
		}
		const std::size_t b = s.find_last_not_of(" \t\r\n");
		return s.substr(a, b - a + 1);
	}

	auto rtrim_ws(std::string_view s) -> std::string_view {
		const std::size_t b = s.find_last_not_of(" \t\r\n");
		if (b == std::string_view::npos) {
			return {};
		}
		return s.substr(0, b + 1);
	}

	auto head_record_kind(std::string_view text) -> int {
		int angle = 0;
		std::size_t i = 0;
		while (i < text.size()) {
			const char c = text[i];
			const char n = i + 1 < text.size() ? text[i + 1] : '\0';
			if (c == '/' && n == '/') {
				const std::size_t nl = text.find('\n', i);
				if (nl == std::string_view::npos) {
					break;
				}
				i = nl + 1;
				continue;
			}
			if (c == '/' && n == '*') {
				const std::size_t e = text.find("*/", i + 2);
				i = e == std::string_view::npos ? text.size() : e + 2;
				continue;
			}
			if (c == '"' || c == '\'') {
				const char q = c;
				++i;
				while (i < text.size()) {
					if (text[i] == '\\') {
						i += 2;
						continue;
					}
					const bool close = text[i] == q;
					++i;
					if (close) {
						break;
					}
				}
				continue;
			}
			if (c == '<') {
				++angle;
				++i;
				continue;
			}
			if (c == '>') {
				if (angle > 0) {
					--angle;
				}
				++i;
				continue;
			}
			if (angle == 0 && is_ident_char(c) && (i == 0 || !is_ident_char(text[i - 1]))) {
				std::size_t j = i;
				while (j < text.size() && is_ident_char(text[j])) {
					++j;
				}
				const std::string_view w = text.substr(i, j - i);
				if (w == "enum") {
					return 0;
				}
				if (w == "struct" || w == "union") {
					return 1;
				}
				if (w == "class") {
					return 2;
				}
				i = j;
				continue;
			}
			++i;
		}
		return 0;
	}

	auto collapse_record(std::string_view src, bool default_public) -> std::string {
		std::string out;
		std::string member;
		int paren = 0;
		bool started = false;
		bool closed = false;
		int access = default_public ? 0 : 2;
		bool in_str = false;
		char strq = '\0';
		bool saw_paren = false;

		auto skip_brace_group = [&](std::size_t k) -> std::size_t {
			int d = 0;
			bool s = false;
			char q = '\0';
			for (; k < src.size(); ++k) {
				const char c = src[k];
				if (s) {
					if (c == '\\') {
						++k;
						continue;
					}
					if (c == q) {
						s = false;
					}
					continue;
				}
				if (c == '"' || c == '\'') {
					s = true;
					q = c;
					continue;
				}
				if (c == '{') {
					++d;
				}
				else if (c == '}') {
					--d;
					if (d == 0) {
						return k + 1;
					}
				}
			}
			return k;
		};
		auto copy_brace_group = [&](std::size_t k) -> std::size_t {
			int d = 0;
			bool s = false;
			char q = '\0';
			for (; k < src.size(); ++k) {
				const char c = src[k];
				member.push_back(c);
				if (s) {
					if (c == '\\') {
						++k;
						if (k < src.size()) {
							member.push_back(src[k]);
						}
						continue;
					}
					if (c == q) {
						s = false;
					}
					continue;
				}
				if (c == '"' || c == '\'') {
					s = true;
					q = c;
					continue;
				}
				if (c == '{') {
					++d;
				}
				else if (c == '}') {
					--d;
					if (d == 0) {
						return k + 1;
					}
				}
			}
			return k;
		};
		auto swallow_semi = [&](std::size_t k) -> std::size_t {
			while (k < src.size() && (src[k] == ' ' || src[k] == '\t')) {
				++k;
			}
			if (k < src.size() && src[k] == ';') {
				++k;
			}
			return k;
		};
		auto emit = [&] {
			if (access == 0) {
				out += rtrim_ws(member);
				out.push_back(';');
			}
			member.clear();
			saw_paren = false;
		};

		std::size_t i = 0;
		while (i < src.size()) {
			const char c = src[i];
			const char n = i + 1 < src.size() ? src[i + 1] : '\0';
			if (in_str) {
				std::string& dst = started ? member : out;
				dst.push_back(c);
				if (c == '\\') {
					if (n != '\0') {
						dst.push_back(n);
					}
					i += 2;
					continue;
				}
				if (c == strq) {
					in_str = false;
				}
				++i;
				continue;
			}
			if (!closed && c == '/' && n == '/') {
				const std::size_t nl = src.find('\n', i);
				const std::size_t stop = nl == std::string_view::npos ? src.size() : nl;
				const std::string_view seg = src.substr(i, stop - i);
				if (started) {
					member.append(seg);
				}
				else {
					out.append(seg);
				}
				i = stop;
				continue;
			}
			if (!closed && c == '/' && n == '*') {
				const std::size_t e = src.find("*/", i + 2);
				const std::size_t stop = e == std::string_view::npos ? src.size() : e + 2;
				const std::string_view seg = src.substr(i, stop - i);
				if (started) {
					member.append(seg);
				}
				else {
					out.append(seg);
				}
				i = stop;
				continue;
			}
			if (closed) {
				out.push_back(c);
				++i;
				continue;
			}
			if (!started) {
				out.push_back(c);
				if (c == '"' || c == '\'') {
					in_str = true;
					strq = c;
				}
				else if (c == '(') {
					++paren;
				}
				else if (c == ')') {
					if (paren > 0) {
						--paren;
					}
				}
				else if (c == '{' && paren == 0) {
					started = true;
					access = default_public ? 0 : 2;
					member.clear();
					saw_paren = false;
				}
				++i;
				continue;
			}
			if (c == '"' || c == '\'') {
				in_str = true;
				strq = c;
				member.push_back(c);
				++i;
				continue;
			}
			if (c == '(') {
				++paren;
				member.push_back(c);
				++i;
				continue;
			}
			if (c == ')') {
				if (paren > 0) {
					--paren;
					if (paren == 0) {
						saw_paren = true;
					}
				}
				member.push_back(c);
				++i;
				continue;
			}
			if (paren > 0) {
				member.push_back(c);
				++i;
				continue;
			}
			if (c == '{') {
				if (saw_paren) {
					const std::size_t j = skip_brace_group(i);
					emit();
					i = swallow_semi(j);
					continue;
				}
				i = copy_brace_group(i);
				continue;
			}
			if (c == '}') {
				out.append(member);
				member.clear();
				out.push_back('}');
				closed = true;
				++i;
				continue;
			}
			if (c == ';') {
				emit();
				++i;
				continue;
			}
			if (c == ':') {
				if (n == ':') {
					member.append("::");
					i += 2;
					continue;
				}
				const std::string_view t = trim_full(member);
				if (t == "public") {
					access = 0;
					member.clear();
					saw_paren = false;
					++i;
					continue;
				}
				if (t == "protected") {
					access = 1;
					member.clear();
					saw_paren = false;
					++i;
					continue;
				}
				if (t == "private") {
					access = 2;
					member.clear();
					saw_paren = false;
					++i;
					continue;
				}
				if (saw_paren) {
					std::size_t k = i + 1;
					char last_ns = ')';
					while (k < src.size()) {
						const char d = src[k];
						if (d == ' ' || d == '\t' || d == '\n' || d == '\r') {
							++k;
							continue;
						}
						if (d == '{') {
							const bool is_body = !(is_ident_char(last_ns) || last_ns == '>');
							k = skip_brace_group(k);
							if (is_body) {
								break;
							}
							last_ns = '}';
							continue;
						}
						if (d == '(') {
							int dd = 0;
							for (; k < src.size(); ++k) {
								if (src[k] == '(') {
									++dd;
								}
								else if (src[k] == ')') {
									--dd;
									if (dd == 0) {
										++k;
										break;
									}
								}
							}
							last_ns = ')';
							continue;
						}
						last_ns = d;
						++k;
					}
					emit();
					i = swallow_semi(k);
					continue;
				}
				member.push_back(c);
				++i;
				continue;
			}
			member.push_back(c);
			++i;
		}
		return out;
	}
}

auto gse::ide::docs::normalize_qualified(std::string_view raw) -> std::string {
	std::string out;
	int depth = 0;
	for (const char c : raw) {
		if (c == '<') {
			++depth;
			continue;
		}
		if (c == '>') {
			if (depth > 0) {
				--depth;
			}
			continue;
		}
		if (depth > 0 || c == ' ' || c == '\t') {
			continue;
		}
		out.push_back(c);
	}
	while (out.starts_with("::")) {
		out.erase(0, 2);
	}
	return out;
}

auto gse::ide::docs::expand_qualified(std::string_view line, std::size_t ident_start) -> std::string {
	std::size_t end = ident_start;
	while (end < line.size() && is_ident_char(line[end])) {
		++end;
	}
	std::size_t start = ident_start;
	while (start >= 2 && line[start - 1] == ':' && line[start - 2] == ':') {
		std::size_t p = start - 2;
		while (p > 0 && is_ident_char(line[p - 1])) {
			--p;
		}
		if (p == start - 2) {
			break;
		}
		start = p;
	}
	if (end <= start) {
		return {};
	}
	return std::string(line.substr(start, end - start));
}

auto gse::ide::docs::extract_header_doc(const std::filesystem::path& file, std::uint32_t line, std::string_view title) -> std::optional<doc_card> {
	std::ifstream in(file, std::ios::binary);
	if (!in) {
		return std::nullopt;
	}
	std::vector<std::string> lines;
	std::string s;
	while (std::getline(in, s)) {
		if (!s.empty() && s.back() == '\r') {
			s.pop_back();
		}
		lines.push_back(std::move(s));
		s.clear();
	}
	if (line >= lines.size()) {
		return std::nullopt;
	}

	const std::string decl = std::string(trim(lines[line]));

	std::vector<std::string> comment;
	long i = static_cast<long>(line) - 1;
	while (i >= 0) {
		const std::string_view t = trim(lines[static_cast<std::size_t>(i)]);
		if (t.starts_with("//") || t.starts_with("*") || t.starts_with("/*") || t.ends_with("*/")) {
			comment.emplace_back(t);
			--i;
			continue;
		}
		break;
	}
	std::ranges::reverse(comment);

	std::string body;
	for (std::string c : comment) {
		if (c.starts_with("/**")) {
			c.erase(0, 3);
		}
		else if (c.starts_with("///")) {
			c.erase(0, 3);
		}
		else if (c.starts_with("/*")) {
			c.erase(0, 2);
		}
		else if (c.starts_with("//")) {
			c.erase(0, 2);
		}
		if (const std::size_t e = c.rfind("*/"); e != std::string::npos) {
			c.erase(e);
		}
		std::string_view v = trim(c);
		if (v.starts_with("*")) {
			v = trim(v.substr(1));
		}
		if (v.starts_with("@brief ") || v.starts_with("\\brief ")) {
			v = trim(v.substr(7));
		}
		if (v.starts_with("@") || v.starts_with("\\")) {
			continue;
		}
		if (v.empty()) {
			continue;
		}
		if (!body.empty()) {
			body.push_back(' ');
		}
		body += v;
	}

	return doc_card{
        .title = title.empty() ? decl : std::string(title),
        .body = body.empty() ? decl : body
    };
}

auto gse::ide::docs::extract_definition(const std::filesystem::path& file, std::uint32_t line, std::string_view title, const bool signature_only) -> std::optional<doc_card> {
	std::ifstream in(file, std::ios::binary);
	if (!in) {
		return std::nullopt;
	}
	std::vector<std::string> lines;
	std::string s;
	while (std::getline(in, s)) {
		if (!s.empty() && s.back() == '\r') {
			s.pop_back();
		}
		lines.push_back(std::move(s));
		s.clear();
	}
	if (line >= lines.size()) {
		return std::nullopt;
	}

	std::size_t start = line;
	while (start > 0) {
		const std::string_view prev = trim(lines[start - 1]);
		if (prev.starts_with("template") || prev.starts_with("requires") || prev.starts_with("[[") || prev.starts_with("//") || prev.starts_with("/*") || prev.starts_with("*")) {
			--start;
		}
		else {
			break;
		}
	}

	int paren = 0;
	std::size_t brace_line = std::string::npos;
	std::size_t brace_col = std::string::npos;
	std::size_t semi_line = std::string::npos;
	bool head_done = false;
	for (std::size_t i = line; i < lines.size() && i < line + 400 && !head_done; ++i) {
		const std::string& ln = lines[i];
		for (std::size_t c = 0; c < ln.size(); ++c) {
			const char ch = ln[c];
			if (ch == '(') {
				++paren;
			}
			else if (ch == ')') {
				if (paren > 0) {
					--paren;
				}
			}
			else if (paren == 0 && ch == '{') {
				brace_line = i;
				brace_col = c;
				head_done = true;
				break;
			}
			else if (paren == 0 && ch == ';') {
				semi_line = i;
				head_done = true;
				break;
			}
		}
	}

	std::size_t end = line;
	std::size_t cut = std::string::npos;
	bool is_record = false;
	bool default_public = true;
	if (signature_only) {
		if (brace_line != std::string::npos) {
			end = brace_line;
			cut = brace_col;
		}
		else if (semi_line != std::string::npos) {
			end = semi_line;
		}
	}
	else if (brace_line != std::string::npos) {
		int depth = 0;
		bool in_str = false;
		bool in_block = false;
		char q = '\0';
		for (std::size_t i = brace_line; i < lines.size() && i < line + 400; ++i) {
			const std::string& ln = lines[i];
			for (std::size_t c = 0; c < ln.size(); ++c) {
				const char ch = ln[c];
				const char nx = c + 1 < ln.size() ? ln[c + 1] : '\0';
				if (in_block) {
					if (ch == '*' && nx == '/') {
						in_block = false;
						++c;
					}
					continue;
				}
				if (in_str) {
					if (ch == '\\') {
						++c;
						continue;
					}
					if (ch == q) {
						in_str = false;
					}
					continue;
				}
				if (ch == '/' && nx == '/') {
					break;
				}
				if (ch == '/' && nx == '*') {
					in_block = true;
					++c;
					continue;
				}
				if (ch == '"' || ch == '\'') {
					in_str = true;
					q = ch;
					continue;
				}
				if (ch == '{') {
					++depth;
				}
				else if (ch == '}') {
					--depth;
				}
			}
			end = i;
			if (!in_block && !in_str && depth <= 0) {
				break;
			}
		}
		std::string head;
		for (std::size_t i = start; i <= brace_line && i < lines.size(); ++i) {
			if (i != start) {
				head.push_back('\n');
			}
			head += lines[i];
		}
		if (const int kind = head_record_kind(head); kind != 0) {
			is_record = true;
			default_public = kind == 1;
		}
	}
	else if (semi_line != std::string::npos) {
		end = semi_line;
	}

	std::vector<std::string> render;
	if (is_record) {
		std::string src;
		for (std::size_t i = start; i <= end && i < lines.size(); ++i) {
			if (i != start) {
				src.push_back('\n');
			}
			src += lines[i];
		}
		const std::string collapsed = collapse_record(src, default_public);
		std::size_t p = 0;
		while (p <= collapsed.size()) {
			const std::size_t nl = collapsed.find('\n', p);
			render.emplace_back(collapsed.substr(p, (nl == std::string::npos ? collapsed.size() : nl) - p));
			if (nl == std::string::npos) {
				break;
			}
			p = nl + 1;
		}
	}
	else {
		for (std::size_t i = start; i <= end; ++i) {
			std::string_view raw = lines[i];
			if (i == end && cut != std::string::npos && cut <= raw.size()) {
				raw = raw.substr(0, cut);
			}
			render.emplace_back(raw);
		}
	}

	std::size_t indent = std::string::npos;
	for (const std::string& l : render) {
		const std::size_t first = l.find_first_not_of(" \t");
		if (first != std::string::npos) {
			indent = std::min(indent, first);
		}
	}
	if (indent == std::string::npos) {
		indent = 0;
	}

	auto norm_key = [](const std::string_view s) {
		std::string o;
		for (const char c : s) {
			if (c == '{' || c == ';') {
				break;
			}
			if (c == ' ' || c == '\t') {
				continue;
			}
			o.push_back(c);
		}
		return o;
	};

	std::string body;
	std::vector<std::uint32_t> line_source;
	std::vector<std::string> line_render;
	std::size_t cursor = start;
	std::size_t count = 0;
	for (std::size_t i = 0; i < render.size() && count < 120; ++i, ++count) {
		if (!body.empty()) {
			body.push_back('\n');
		}
		const std::string_view raw = render[i];
		const std::string_view content = raw.size() >= indent ? raw.substr(indent) : raw;
		for (const char c : content) {
			if (c == '\t') {
				body.append(4, ' ');
			}
			else {
				body.push_back(c);
			}
		}
		line_render.emplace_back(raw);
		std::uint32_t src = std::numeric_limits<std::uint32_t>::max();
		if (is_record) {
			const std::string key = norm_key(raw);
			if (!key.empty()) {
				for (std::size_t s = cursor; s <= end && s < lines.size(); ++s) {
					if (norm_key(lines[s]) == key) {
						src = static_cast<std::uint32_t>(s);
						cursor = s + 1;
						break;
					}
				}
			}
		}
		else if (start + i < lines.size()) {
			src = static_cast<std::uint32_t>(start + i);
		}
		line_source.push_back(src);
	}
	while (!body.empty() && (body.back() == ' ' || body.back() == '\t' || body.back() == '\n')) {
		body.pop_back();
	}

	doc_card card;
	card.title = title.empty() ? std::string(trim(lines[line])) : std::string(title);
	card.body = std::move(body);
	card.source = file;
	card.indent = static_cast<std::uint32_t>(indent);
	card.line_source = std::move(line_source);
	card.line_render = std::move(line_render);
	return card;
}

auto gse::ide::docs::cppref_index::load(const std::filesystem::path& file) -> void {
	loaded = true;
	std::ifstream in(file, std::ios::binary);
	if (!in) {
		return;
	}
	std::string line;
	while (std::getline(in, line)) {
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		const std::size_t t1 = line.find('\t');
		const std::size_t t2 = t1 == std::string::npos ? std::string::npos : line.find('\t', t1 + 1);
		const std::size_t t3 = t2 == std::string::npos ? std::string::npos : line.find('\t', t2 + 1);
		if (t3 == std::string::npos) {
			continue;
		}
		std::string key = line.substr(0, t1);
		cppref_entry entry{ .kind = line.substr(t1 + 1, t2 - t1 - 1), .page = line.substr(t2 + 1, t3 - t2 - 1) };
		const std::string_view raw = std::string_view(line).substr(t3 + 1);
		entry.brief.reserve(raw.size());
		for (std::size_t i = 0; i < raw.size(); ++i) {
			if (raw[i] == '\\' && i + 1 < raw.size()) {
				const char n = raw[i + 1];
				if (n == 'n') {
					entry.brief.push_back('\n');
					++i;
					continue;
				}
				if (n == 't') {
					entry.brief.push_back('\t');
					++i;
					continue;
				}
				if (n == '\\') {
					entry.brief.push_back('\\');
					++i;
					continue;
				}
			}
			entry.brief.push_back(raw[i]);
		}
		entries.insert_or_assign(std::move(key), std::move(entry));
	}
}

auto gse::ide::docs::cppref_index::find(std::string_view qualified) const -> std::optional<cppref_hit> {
	const auto it = entries.find(qualified);
	if (it == entries.end()) {
		return std::nullopt;
	}
	return cppref_hit{ .kind = it->second.kind, .page = it->second.page, .brief = it->second.brief };
}

auto gse::ide::docs::cppref_url(std::string_view page) -> std::string {
	return "https://en.cppreference.com/w/" + std::string(page);
}
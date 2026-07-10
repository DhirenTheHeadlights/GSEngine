export module gse.ide.docs;

import std;
import gse;

export namespace gse::ide::docs {
	struct doc_card {
		std::string title;
		std::string body;
		bool from_cppref = false;
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
		for (std::size_t i = brace_line; i < lines.size() && i < line + 400; ++i) {
			for (const char c : lines[i]) {
				if (c == '{') {
					++depth;
				}
				else if (c == '}') {
					--depth;
				}
			}
			end = i;
			if (depth <= 0) {
				break;
			}
		}
	}
	else if (semi_line != std::string::npos) {
		end = semi_line;
	}

	std::size_t indent = std::string::npos;
	for (std::size_t i = start; i <= end; ++i) {
		const std::size_t first = lines[i].find_first_not_of(" \t");
		if (first != std::string::npos) {
			indent = std::min(indent, first);
		}
	}
	if (indent == std::string::npos) {
		indent = 0;
	}

	std::string body;
	std::size_t count = 0;
	for (std::size_t i = start; i <= end && count < 120; ++i, ++count) {
		if (!body.empty()) {
			body.push_back('\n');
		}
		std::string_view raw = lines[i];
		if (i == end && cut != std::string::npos && cut <= raw.size()) {
			raw = raw.substr(0, cut);
		}
		const std::string_view content = raw.size() >= indent ? raw.substr(indent) : raw;
		for (const char c : content) {
			if (c == '\t') {
				body.append(4, ' ');
			}
			else {
				body.push_back(c);
			}
		}
	}
	while (!body.empty() && (body.back() == ' ' || body.back() == '\t' || body.back() == '\n')) {
		body.pop_back();
	}

	doc_card card;
	card.title = title.empty() ? std::string(trim(lines[line])) : std::string(title);
	card.body = std::move(body);
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
module gse.ide.navigation;

import std;
import gse.ide.config;

auto gse::ide::path_link_at(const std::string_view row, const std::uint32_t column, const std::filesystem::path& base) -> std::optional<link_hit> {
	auto is_link_char = [](const char c) -> bool {
		return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.' || c == '-' || c == '/' || c == '\\' || c == ':' || c == '~' || c == '+';
	};

	if (row.empty()) {
		return std::nullopt;
	}
	std::size_t a = std::min<std::size_t>(column, row.size());
	if (a == row.size() || !is_link_char(row[a])) {
		if (a == 0 || !is_link_char(row[a - 1])) {
			return std::nullopt;
		}
		--a;
	}
	std::size_t b = a;
	while (a > 0 && is_link_char(row[a - 1])) {
		--a;
	}
	while (b < row.size() && is_link_char(row[b])) {
		++b;
	}

	std::optional<link_hit> hit = resolve_link(row.substr(a, b - a), base);
	if (hit) {
		hit->start_col = static_cast<std::uint32_t>(a);
		hit->end_col = static_cast<std::uint32_t>(b);
	}
	return hit;
}

auto gse::ide::resolve_link(const std::string_view token, const std::filesystem::path& base) -> std::optional<link_hit> {
	std::uint32_t peeled[2] = { 0, 0 };
	std::size_t peeled_count = 0;
	std::size_t end = token.size();
	while (end > 0 && token[end - 1] == ':') {
		--end;
	}
	while (peeled_count < 2) {
		std::size_t digits = end;
		while (digits > 0 && std::isdigit(static_cast<unsigned char>(token[digits - 1]))) {
			--digits;
		}
		if (digits == end || digits == 0 || token[digits - 1] != ':') {
			break;
		}
		std::uint32_t value = 0;
		std::from_chars(token.data() + digits, token.data() + end, value);
		peeled[peeled_count++] = value;
		end = digits - 1;
	}

	const std::string_view path_str = token.substr(0, end);
	if (path_str.empty()) {
		return std::nullopt;
	}

	const std::uint32_t line = peeled_count == 2 ? peeled[1] : (peeled_count == 1 ? peeled[0] : 0);
	const std::uint32_t col = peeled_count == 2 ? peeled[0] : 0;

	const std::filesystem::path candidate(path_str);
	std::error_code ec;
	std::filesystem::path resolved;
	if (candidate.is_absolute()) {
		resolved = candidate;
	}
	else {
		const auto holds = [&](const std::filesystem::path& root) {
			return !root.empty() && std::filesystem::is_regular_file(root / candidate, ec);
		};
		if (holds(base)) {
			resolved = base / candidate;
		}
		else if (const auto tree = std::ranges::find_if(config::worktrees(), holds, &config::worktree::project_root); tree != config::worktrees().end()) {
			resolved = tree->project_root / candidate;
		}
	}

	if (resolved.empty() || !std::filesystem::is_regular_file(resolved, ec)) {
		return std::nullopt;
	}

	return link_hit{
		.path = std::move(resolved),
		.line = line > 0 ? line - 1 : 0,
		.column = col > 0 ? col - 1 : 0,
		.end_col = static_cast<std::uint32_t>(token.size()),
	};
}

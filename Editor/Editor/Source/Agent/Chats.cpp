module gse.ide.agent:chats_impl;

import std;
import gse;

import gse.ide.analysis;
import gse.ide.build;
import gse.ide.config;

import :chats;
import :model;
import :session;
import :stream;

auto gse::ide::agent::transcript_path(const std::string& agent_id) -> std::filesystem::path {
	if (agent_id.empty()) {
		return {};
	}

	const std::filesystem::path home = claude_home();
	if (home.empty()) {
		return {};
	}

	const std::filesystem::path projects = home / "projects";
	const std::string file = agent_id + ".jsonl";
	std::error_code ec;

	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(projects, ec)) {
		if (!entry.is_directory(ec)) {
			continue;
		}
		if (std::filesystem::path candidate = entry.path() / file; std::filesystem::is_regular_file(candidate, ec)) {
			return candidate;
		}
	}

	return {};
}

auto gse::ide::agent::transcript_dir(const std::filesystem::path& cwd) -> std::filesystem::path {
	const std::filesystem::path home = claude_home();
	if (home.empty() || cwd.empty()) {
		return {};
	}

	std::string mangled = cwd.generic_native_encoded_string();
	for (char& c : mangled) {
		if (!std::isalnum(static_cast<unsigned char>(c))) {
			c = '-';
		}
	}
	while (!mangled.empty() && mangled.back() == '-') {
		mangled.pop_back();
	}
	if (mangled.empty()) {
		return {};
	}

	return home / "projects" / mangled;
}

auto gse::ide::agent::chat_summary(const std::filesystem::path& file) -> std::string {
	std::ifstream in(file, std::ios::binary);
	if (!in) {
		return {};
	}

	std::size_t scanned_lines = 0;
	std::size_t scanned_bytes = 0;
	for (std::string line; std::getline(in, line); ) {
		if (++scanned_lines > summary_scan_lines || scanned_bytes > summary_scan_bytes) {
			break;
		}
		scanned_bytes += line.size();
		if (line.find(R"("type":"user")") == std::string::npos) {
			continue;
		}
		const std::optional<analysis::json::value> event = analysis::json::parse(line);
		if (!event || string_at(*event, "type") != "user") {
			continue;
		}
		for (const transcript_row& row : user_rows(*event)) {
			if (row.text.starts_with('<')) {
				continue;
			}
			std::string_view head = std::string_view(row.text).substr(0, row.text.find_first_of("\r\n"));
			while (!head.empty() && head.front() == ' ') {
				head.remove_prefix(1);
			}
			while (!head.empty() && head.back() == ' ') {
				head.remove_suffix(1);
			}
			if (!head.empty()) {
				return std::string(head);
			}
		}
	}

	return {};
}

auto gse::ide::agent::chat_title(const std::string_view summary) -> std::string {
	std::string_view head = summary;
	if (head.size() > chat_title_columns) {
		head = head.substr(0, chat_title_columns);
		if (const std::size_t space = head.find_last_of(' '); space != std::string_view::npos) {
			head = head.substr(0, space);
		}
	}

	while (!head.empty() && head.back() == ' ') {
		head.remove_suffix(1);
	}

	return head.empty() ? std::string("Restored chat") : std::string(head);
}

auto gse::ide::agent::past_chats(const std::filesystem::path& cwd) -> std::vector<past_chat> {
	std::vector<past_chat> out;

	const std::filesystem::path dir = transcript_dir(cwd);
	std::error_code ec;
	if (dir.empty() || !std::filesystem::is_directory(dir, ec)) {
		return out;
	}

	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(dir, ec)) {
		if (!entry.is_regular_file(ec) || entry.path().extension() != ".jsonl") {
			continue;
		}
		const std::filesystem::file_time_type modified = entry.last_write_time(ec);
		out.push_back({
			.agent_id = entry.path().stem().native_encoded_string(),
			.path = entry.path(),
			.modified = ec ? 0 : modified.time_since_epoch().count(),
		});
	}

	std::ranges::sort(out, std::ranges::greater{}, &past_chat::modified);
	return out;
}

auto gse::ide::agent::restore_rows(session& s) -> bool {
	const std::filesystem::path file = transcript_path(s.info.agent_id);
	if (file.empty()) {
		return false;
	}

	std::ifstream in(file, std::ios::binary);
	if (!in) {
		return false;
	}

	for (std::string line; std::getline(in, line); ) {
		if (line.empty()) {
			continue;
		}
		const std::optional<analysis::json::value> event = analysis::json::parse(line);
		if (!event) {
			continue;
		}

		const std::string_view kind = string_at(*event, "type");
		if (kind == "user") {
			for (transcript_row& row : user_rows(*event)) {
				s.rows.push_back(std::move(row));
			}
			continue;
		}
		if (kind == "system" && string_at(*event, "subtype") != "init") {
			continue;
		}
		if (kind != "assistant" && kind != "system") {
			continue;
		}

		const std::string_view uuid = anchorable(*event) ? string_at(*event, "uuid") : std::string_view{};
		for (transcript_row& row : summarize(*event, s.info)) {
			row.uuid = uuid;
			s.rows.push_back(std::move(row));
		}
	}

	return true;
}

auto gse::ide::agent::hydrate_session(session& s) -> void {
	if (s.hydrated) {
		return;
	}

	s.hydrated = true;
	if (s.info.agent_id.empty() || !s.rows.empty()) {
		return;
	}

	if (!restore_rows(s)) {
		append_row(s, {
			.kind = row_kind::note,
			.text = std::format("no transcript on disk for {}", s.info.agent_id),
		});
	}
}

auto gse::ide::agent::restore_chat(data& d, const past_chat& chat) -> void {
	const auto open = std::ranges::find_if(d.sessions, [&](const session& s) {
		return s.info.agent_id == chat.agent_id;
	});
	if (open != d.sessions.end()) {
		d.active = open->id;
		return;
	}

	const auto named = d.chat_names.find(chat.agent_id);

	session& restored = create_session(d, config::primary().project_root);
	restored.name = named != d.chat_names.end() && !named->second.empty()
		? named->second
		: chat_title(chat.summary);
	restored.info.agent_id = chat.agent_id;
	restored.hydrated = true;

	if (!restore_rows(restored)) {
		log::println(log::level::warning, log::category::task, "agent: could not read '{}' - the tab resumes without its scrollback", chat.path.generic_display_string());
	}
}

auto gse::ide::agent::entry_uuid(const std::string_view line) -> std::string_view {
	constexpr std::string_view key = R"("uuid":")";
	constexpr std::string_view tail = R"(","timestamp":")";

	std::string_view first;
	std::string_view stamped;

	for (std::size_t at = line.find(key); at != std::string_view::npos; at = line.find(key, at + key.size())) {
		const std::size_t start = at + key.size();
		if (start + uuid_length > line.size()) {
			break;
		}
		if (first.empty()) {
			first = line.substr(start, uuid_length);
		}
		if (start + uuid_length + tail.size() <= line.size() && line.substr(start + uuid_length, tail.size()) == tail) {
			stamped = line.substr(start, uuid_length);
		}
	}

	return stamped.empty() ? first : stamped;
}

auto gse::ide::agent::entry_parent(const std::string_view line) -> std::string_view {
	constexpr std::string_view key = R"("parentUuid":")";

	const std::size_t at = line.find(key);
	if (at == std::string_view::npos || at + key.size() + uuid_length > line.size()) {
		return {};
	}

	return line.substr(at + key.size(), uuid_length);
}

auto gse::ide::agent::forked_session_id() -> std::string {
	std::random_device source;
	std::mt19937_64 engine((static_cast<std::uint64_t>(source()) << 32) ^ source());
	std::uniform_int_distribution<std::uint64_t> bits;

	const std::uint64_t high = bits(engine);
	const std::uint64_t low = bits(engine);

	return std::format(
		"{:08x}-{:04x}-4{:03x}-{:04x}-{:012x}",
		static_cast<std::uint32_t>(high >> 32),
		static_cast<std::uint32_t>((high >> 16) & 0xffff),
		static_cast<std::uint32_t>(high & 0x0fff),
		static_cast<std::uint32_t>(((low >> 48) & 0x3fff) | 0x8000),
		low & 0xffffffffffffull
	);
}

auto gse::ide::agent::substituted(const std::string_view line, const std::string_view from, const std::string_view to) -> std::string {
	if (from.empty()) {
		return std::string(line);
	}

	std::string out;
	out.reserve(line.size());

	std::size_t at = 0;
	while (at < line.size()) {
		const std::size_t found = line.find(from, at);
		if (found == std::string_view::npos) {
			out.append(line.substr(at));
			break;
		}
		out.append(line.substr(at, found - at));
		out.append(to);
		at = found + from.size();
	}

	return out;
}

auto gse::ide::agent::write_fork(const std::filesystem::path& source, const std::filesystem::path& target, const std::string_view leaf, const std::string_view forked_id) -> bool {
	std::vector<std::string> lines;
	{
		std::ifstream in(source, std::ios::binary);
		if (!in) {
			return false;
		}
		for (std::string line; std::getline(in, line); ) {
			if (!line.empty() && line.back() == '\r') {
				line.pop_back();
			}
			if (!line.empty()) {
				lines.push_back(std::move(line));
			}
		}
	}

	std::unordered_map<std::string_view, std::size_t> by_uuid;
	for (std::size_t i = 0; i < lines.size(); ++i) {
		if (const std::string_view uuid = entry_uuid(lines[i]); !uuid.empty()) {
			by_uuid.emplace(uuid, i);
		}
	}

	std::vector<std::size_t> chain;
	std::unordered_set<std::string_view> walked;
	for (std::string_view at = leaf; !at.empty() && walked.insert(at).second; ) {
		const auto found = by_uuid.find(at);
		if (found == by_uuid.end()) {
			return false;
		}
		chain.push_back(found->second);
		at = entry_parent(lines[found->second]);
	}

	if (chain.empty()) {
		return false;
	}

	std::ranges::sort(chain);

	const std::string original = source.stem().native_encoded_string();
	std::filesystem::path temporary = target;
	temporary += ".tmp";

	bool written = false;
	{
		std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
		if (out) {
			for (const std::size_t index : chain) {
				out << substituted(lines[index], original, forked_id) << '\n';
			}
			out << std::format(R"({{"type":"last-prompt","leafUuid":"{}","explicit":true,"rewound":true,"sessionId":"{}"}})", leaf, forked_id) << '\n';
			written = static_cast<bool>(out);
		}
	}

	std::error_code ec;
	if (!written) {
		std::filesystem::remove(temporary, ec);
		return false;
	}

	std::filesystem::rename(temporary, target, ec);
	if (ec) {
		std::filesystem::remove(temporary, ec);
		return false;
	}

	return true;
}

auto gse::ide::agent::rewind_anchor(const session& s, const std::uint32_t row) -> std::optional<std::uint32_t> {
	for (std::uint32_t at = std::min(row, static_cast<std::uint32_t>(s.rows.size())); at > 0; --at) {
		if (!s.rows[at - 1].uuid.empty()) {
			return at - 1;
		}
	}

	return std::nullopt;
}

auto gse::ide::agent::rewind_session(session& s, const std::uint32_t row) -> bool {
	if (row >= s.rows.size() || s.rows[row].kind != row_kind::user) {
		return false;
	}

	const std::optional<std::uint32_t> anchor = rewind_anchor(s, row);
	if (!anchor || s.info.agent_id.empty()) {
		return false;
	}

	const std::filesystem::path source = transcript_path(s.info.agent_id);
	if (source.empty()) {
		log::println(log::level::warning, log::category::task, "agent: no transcript on disk for session '{}' - nothing was rewound", s.info.agent_id);
		return false;
	}

	const std::string forked = forked_session_id();
	const std::filesystem::path target = source.parent_path() / (forked + ".jsonl");
	if (!write_fork(source, target, s.rows[*anchor].uuid, forked)) {
		log::println(log::level::warning, log::category::task, "agent: could not fork '{}' - nothing was rewound", source.generic_display_string());
		return false;
	}

	if (s.running) {
		spawn::terminate(s.process, s.job);
		s.running = false;
	}
	close_session(s);
	s.pending.clear();
	s.think_clock.reset();
	s.action.clear();

	s.rows.resize(*anchor + 1);
	std::erase_if(s.diffs, [cut = *anchor](const diff_view& view) {
		return view.row > cut;
	});
	std::erase_if(s.expanded_groups, [cut = *anchor](const std::uint32_t group) {
		return group > cut;
	});

	s.buffer.lines.clear();
	s.spans.clear();
	s.blocks.clear();
	s.links.clear();
	s.line_rows.clear();
	s.groups.clear();
	s.flushed_rows = 0;
	s.wrap_width = 0.f;
	for (diff_view& view : s.diffs) {
		view.line = unplaced_line;
	}

	s.info.agent_id = forked;
	s.info.failure.clear();
	s.retry = {};

	if (!launch_session(s)) {
		append_row(s, {
			.kind = row_kind::failure,
			.text = "rewound, but claude could not be relaunched",
			.detail = "press + to start a new session, or reopen the panel",
		});
	}

	return true;
}

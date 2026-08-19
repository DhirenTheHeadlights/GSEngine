export module gse.ide.agent;

import std;
import gse;
import gse.win32;
import gse.win32.environment;

import gse.ide.analysis;
import gse.ide.build;
import gse.ide.config;
import gse.ide.navigation;

export namespace gse::ide::agent {
	constexpr std::string_view panel_name = "Agent";

	enum class row_kind : std::uint8_t {
		note,
		user,
		text,
		tool,
		denial,
		failure,
	};

	struct transcript_row {
		row_kind kind = row_kind::note;
		std::string text;
		std::string detail;
		std::filesystem::path file;
		std::vector<std::string> removed;
		std::vector<std::string> added;
	};

	struct start_request {
		std::string prompt;
		std::filesystem::path cwd;
	};

	struct session_info {
		std::string agent_id;
		std::string model;
		std::int64_t turns = 0;
		time api_time{};
		double cost = 0.0;
	};

	struct group_marker {
		std::uint32_t row = 0;
		std::uint32_t line = 0;
		std::uint32_t rows = 0;
	};

	struct session {
		std::uint32_t id = 0;
		std::string name;
		std::filesystem::path cwd;
		session_info info;
		void* process = nullptr;
		void* job = nullptr;
		void* output = nullptr;
		void* input = nullptr;
		std::string pending;
		std::vector<transcript_row> rows;
		std::size_t flushed_rows = 0;
		float wrap_width = 0.f;
		gui::text_buffer buffer;
		gui::text_area_state view;
		gui::text_input_state name_state;
		std::vector<gui::text_span> spans;
		std::vector<std::uint32_t> line_rows;
		std::vector<group_marker> groups;
		std::vector<std::uint32_t> expanded_groups;
		gse::id log_id;
		bool running = false;
		bool announced = false;
		std::shared_ptr<spawn::output_stream> stream;
	};

	struct [[= system_state<"Agent">{}]] data {
		std::vector<session> sessions;
		std::uint32_t next_id = 0;
		std::uint32_t active = 0;
		std::uint32_t renaming = 0;
		std::string input;
		gui::text_input_state input_state;
		gui::interaction::click_state tab_click;
		rectf info_anchor;
		bool info_open = false;
		bool initialized = false;
	};

	template <archive A>
	auto serialize(
		A& ar,
		session& s
	) -> void;

	auto draw_panel(
		gui::builder& ui,
		data& d,
		vec2f mouse,
		channel_write<build_runner::stream_opened, gui::menu_content, jump_to_request> jump_out
	) -> void;

	[[= system_run<>{}]]
	auto run(
		context& ctx,
		data& d,
		channel_read<start_request> requests_in,
		channel_write<build_runner::stream_opened, gui::menu_content, jump_to_request> events_out,
		shared_view<input::data> input_d
	) -> async::task<>;

	[[= system_shutdown{}]]
	auto shutdown(
		data& d
	) -> void;
}

namespace gse::ide::agent {
	constexpr std::string_view agent_command = "claude -p --output-format stream-json --input-format stream-json --verbose --permission-mode auto";
	constexpr std::wstring_view resume_option = L" --resume ";
	constexpr std::wstring_view oauth_token_name = L"CLAUDE_CODE_OAUTH_TOKEN";
	constexpr std::uint32_t sessions_magic = 0x47534147;
	constexpr std::uint32_t sessions_version = 1;

	struct credentials {
		std::vector<wchar_t> environment;
		bool token = false;
	};

	struct transcript_metrics {
		const font& face;
		float width = 0.f;
		float scale = 0.f;
	};

	enum class markup : std::uint8_t {
		body,
		strong,
		code,
		heading,
	};

	struct markup_run {
		std::size_t start = 0;
		std::size_t end = 0;
		markup kind = markup::body;
	};

	struct markup_line {
		std::string text;
		std::vector<markup_run> runs;
		markup kind = markup::body;
	};

	struct transcript_line {
		std::string_view prefix;
		std::string_view text;
		vec4f color;
		std::uint32_t row = 0;
		bool markdown = false;
	};

	auto agent_credentials() -> credentials;

	auto sessions_path() -> std::filesystem::path;

	auto save_sessions(
		const data& d
	) -> void;

	auto load_sessions(
		data& d
	) -> void;

	auto escape_json(
		std::string_view text
	) -> std::string;

	auto user_message(
		std::string_view prompt
	) -> std::string;

	auto string_at(
		const analysis::json::value& parent,
		std::string_view key
	) -> std::string_view;

	auto summarize(
		const analysis::json::value& event,
		session_info& info
	) -> std::vector<transcript_row>;

	auto collect_denials(
		const analysis::json::value& event,
		std::vector<transcript_row>& out
	) -> void;

	auto to_lines(
		std::string_view text
	) -> std::vector<std::string>;

	auto tool_row(
		const analysis::json::value& block
	) -> transcript_row;

	auto jump_line_for(
		const transcript_row& row
	) -> std::uint32_t;

	auto attach_runtime(
		session& s
	) -> void;

	auto create_session(
		data& d,
		const std::filesystem::path& cwd
	) -> session&;

	auto session_command(
		const session& s
	) -> std::wstring;

	auto launch_session(
		session& s
	) -> bool;

	auto append_row(
		session& s,
		transcript_row row
	) -> void;

	auto pump_session(
		session& s
	) -> void;

	auto close_session(
		session& s
	) -> void;

	auto send_to_session(
		session& s,
		std::string_view prompt
	) -> void;

	auto active_session(
		data& d
	) -> session*;

	auto row_prefix(
		row_kind kind
	) -> std::string_view;

	auto row_color(
		const gui::style& sty,
		row_kind kind
	) -> vec4f;

	auto parse_markup(
		std::string_view line
	) -> markup_line;

	auto markup_color(
		const gui::style& sty,
		markup kind,
		const vec4f& base
	) -> vec4f;

	auto push_transcript_line(
		session& s,
		const gui::style& sty,
		const transcript_line& line,
		const transcript_metrics& metrics
	) -> void;

	auto command_row(
		const transcript_row& row
	) -> bool;

	auto group_expanded(
		const session& s,
		std::uint32_t row
	) -> bool;

	auto toggle_group(
		session& s,
		std::size_t index
	) -> void;

	auto truncate_transcript(
		session& s,
		std::uint32_t line
	) -> void;

	auto push_row(
		session& s,
		const gui::style& sty,
		const transcript_row& row,
		std::uint32_t index,
		const transcript_metrics& metrics
	) -> void;

	auto sync_transcript(
		session& s,
		const gui::style& sty,
		const transcript_metrics& metrics
	) -> void;

	auto draw_session_info(
		const gui::draw_context& ctx,
		data& d,
		const rectf& body
	) -> void;

	auto draw_session_tabs(
		gui::builder& ui,
		data& d,
		const rectf& strip
	) -> void;

	auto draw_transcript(
		gui::builder& ui,
		data& d,
		const rectf& area,
		vec2f mouse,
		channel_write<build_runner::stream_opened, gui::menu_content, jump_to_request> jump_out
	) -> void;

	auto draw_input(
		gui::builder& ui,
		data& d,
		const rectf& area,
		channel_write<build_runner::stream_opened, gui::menu_content, jump_to_request> stream_out
	) -> void;
}

auto gse::ide::agent::escape_json(const std::string_view text) -> std::string {
	std::string out;
	out.reserve(text.size() + 16);
	for (const char ch : text) {
		switch (ch) {
			case '"':
				out += "\\\"";
				break;
			case '\\':
				out += "\\\\";
				break;
			case '\n':
				out += "\\n";
				break;
			case '\r':
				out += "\\r";
				break;
			case '\t':
				out += "\\t";
				break;
			default:
				if (static_cast<unsigned char>(ch) < 0x20) {
					out += std::format("\\u{:04x}", static_cast<unsigned int>(static_cast<unsigned char>(ch)));
				}
				else {
					out.push_back(ch);
				}
				break;
		}
	}
	return out;
}

auto gse::ide::agent::user_message(const std::string_view prompt) -> std::string {
	std::string out = R"({"type":"user","message":{"role":"user","content":[{"type":"text","text":")";
	out += escape_json(prompt);
	out += R"("}]}})";
	out += '\n';
	return out;
}

auto gse::ide::agent::string_at(const analysis::json::value& parent, const std::string_view key) -> std::string_view {
	const analysis::json::value* found = parent.find(key);
	return found ? found->as_string() : std::string_view{};
}

auto gse::ide::agent::collect_denials(const analysis::json::value& event, std::vector<transcript_row>& out) -> void {
	const analysis::json::value* denials = event.find("permission_denials");
	if (!denials || !denials->is_array()) {
		return;
	}
	for (const analysis::json::value& denial : denials->children) {
		const std::string_view tool = string_at(denial, "tool_name");
		out.push_back({
			.kind = row_kind::denial,
			.text = std::format("denied: {}", tool.empty() ? std::string_view("tool") : tool),
			.detail = std::string(string_at(denial, "message")),
		});
	}
}

auto gse::ide::agent::to_lines(const std::string_view text) -> std::vector<std::string> {
	std::vector<std::string> lines;
	std::size_t start = 0;
	while (start <= text.size()) {
		const std::size_t newline = text.find('\n', start);
		const std::size_t end = newline == std::string_view::npos ? text.size() : newline;
		std::string_view line = text.substr(start, end - start);
		if (!line.empty() && line.back() == '\r') {
			line.remove_suffix(1);
		}
		lines.emplace_back(line);
		if (newline == std::string_view::npos) {
			break;
		}
		start = newline + 1;
	}
	return lines;
}

auto gse::ide::agent::tool_row(const analysis::json::value& block) -> transcript_row {
	const std::string_view name = string_at(block, "name");
	transcript_row row = {
		.kind = row_kind::tool,
		.text = std::string(name),
	};

	const analysis::json::value* input = block.find("input");
	if (!input) {
		return row;
	}

	const std::string_view path = string_at(*input, "file_path");
	if (!path.empty()) {
		row.file = std::filesystem::path(path);
		row.detail = row.file.filename().generic_display_string();
	}

	if (name == "Edit") {
		row.removed = to_lines(string_at(*input, "old_string"));
		row.added = to_lines(string_at(*input, "new_string"));
		return row;
	}

	if (name == "Write") {
		row.added = to_lines(string_at(*input, "content"));
		return row;
	}

	if (name == "Bash" || name == "PowerShell") {
		row.detail = std::string(string_at(*input, "command"));
		return row;
	}

	return row;
}

auto gse::ide::agent::jump_line_for(const transcript_row& row) -> std::uint32_t {
	if (row.file.empty() || row.added.empty()) {
		return 0;
	}

	const std::string& needle = row.added.front();
	if (needle.empty()) {
		return 0;
	}

	std::ifstream in(row.file);
	if (!in) {
		return 0;
	}

	std::string line;
	std::uint32_t index = 0;
	while (std::getline(in, line)) {
		if (line.find(needle) != std::string::npos) {
			return index;
		}
		++index;
	}
	return 0;
}

auto gse::ide::agent::summarize(const analysis::json::value& event, session_info& info) -> std::vector<transcript_row> {
	std::vector<transcript_row> out;
	const std::string_view kind = string_at(event, "type");

	if (kind == "system") {
		const std::string_view subtype = string_at(event, "subtype");
		if (subtype == "thinking_tokens" || subtype == "post_turn_summary" || subtype == "hook_started") {
			return out;
		}
		if (subtype == "hook_response") {
			const analysis::json::value* code = event.find("exit_code");
			if (code && code->as_int() != 0) {
				out.push_back({
					.kind = row_kind::failure,
					.text = std::format("hook {} failed", string_at(event, "hook_name")),
					.detail = std::string(string_at(event, "stderr")),
				});
			}
			return out;
		}
		if (subtype == "init") {
			info.agent_id = std::string(string_at(event, "session_id"));
			info.model = std::string(string_at(event, "model"));
			return out;
		}
		if (subtype == "api_retry") {
			const analysis::json::value* status = event.find("error_status");
			out.push_back({
				.kind = row_kind::failure,
				.text = std::format("retry {}", status ? status->as_int() : 0),
				.detail = std::string(string_at(event, "error")),
			});
			return out;
		}
		out.push_back({
			.kind = row_kind::note,
			.text = std::format("[{}]", subtype.empty() ? kind : subtype),
		});
		return out;
	}

	if (kind == "rate_limit_event") {
		const analysis::json::value* info = event.find("rate_limit_info");
		if (!info) {
			return out;
		}
		const analysis::json::value* overage = info->find("isUsingOverage");
		const std::string_view status = string_at(*info, "status");
		if (status == "allowed" && !(overage && overage->boolean)) {
			return out;
		}
		out.push_back({
			.kind = row_kind::failure,
			.text = std::format("{} limit: {}{}", string_at(*info, "rateLimitType"), status, overage && overage->boolean ? " (overage)" : ""),
		});
		return out;
	}

	if (kind == "control_request") {
		const analysis::json::value* request = event.find("request");
		out.push_back({
			.kind = row_kind::note,
			.text = std::format("control request: {}", request ? string_at(*request, "subtype") : std::string_view{}),
		});
		return out;
	}

	if (kind == "assistant") {
		const analysis::json::value* message = event.find("message");
		const analysis::json::value* content = message ? message->find("content") : nullptr;
		if (!content || !content->is_array()) {
			return out;
		}
		for (const analysis::json::value& block : content->children) {
			const std::string_view block_kind = string_at(block, "type");
			if (block_kind == "text") {
				out.push_back({
					.kind = row_kind::text,
					.text = std::string(string_at(block, "text")),
				});
			}
			else if (block_kind == "tool_use") {
				out.push_back(tool_row(block));
			}
		}
		return out;
	}

	if (kind == "result") {
		collect_denials(event, out);

		const analysis::json::value* error = event.find("is_error");
		if (error && error->boolean) {
			const analysis::json::value* status = event.find("api_error_status");
			out.push_back({
				.kind = row_kind::failure,
				.text = status && status->as_int() == 401 ? "authentication failed - set CLAUDE_CODE_OAUTH_TOKEN" : std::format("error {}", status ? status->as_int() : 0),
				.detail = std::string(string_at(event, "result")),
			});
			return out;
		}

		const analysis::json::value* turns = event.find("num_turns");
		const analysis::json::value* duration = event.find("duration_api_ms");
		const analysis::json::value* cost = event.find("total_cost_usd");
		if (turns) {
			info.turns = turns->as_int();
		}
		if (duration) {
			info.api_time += milliseconds(static_cast<float>(duration->as_int()));
		}
		if (cost) {
			info.cost += cost->number;
		}
		return out;
	}

	out.push_back({
		.kind = row_kind::note,
		.text = std::format("[{}]", kind),
	});
	return out;
}

auto gse::ide::agent::agent_credentials() -> credentials {
	std::array<wchar_t, 2> probe{};
	if (win32::GetEnvironmentVariableW(oauth_token_name.data(), probe.data(), static_cast<win32::DWORD>(probe.size())) != 0) {
		return { .token = true };
	}

	const std::wstring value = win32::user_environment_value(oauth_token_name);
	if (value.empty()) {
		return {};
	}

	return {
		.environment = win32::environment_with_variable(oauth_token_name, value),
		.token = true,
	};
}

template <gse::archive A>
auto gse::ide::agent::serialize(A& ar, session& s) -> void {
	ar & s.id & s.name & s.cwd & s.info & s.rows;
}

auto gse::ide::agent::sessions_path() -> std::filesystem::path {
	return config::project_state_dir() / "agent_sessions.bin";
}

auto gse::ide::agent::save_sessions(const data& d) -> void {
	const std::filesystem::path path = sessions_path();
	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);
	if (ec) {
		log::println(log::level::error, log::category::task, "agent: could not create '{}': {}", path.parent_path().generic_display_string(), ec.message());
		return;
	}

	std::filesystem::path temporary = path;
	temporary += ".tmp";
	{
		std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
		if (!out) {
			log::println(log::level::error, log::category::task, "agent: could not open '{}'", temporary.generic_display_string());
			return;
		}
		binary_writer writer(out, sessions_magic, sessions_version);
		writer & d.sessions & d.active;
	}

	std::filesystem::rename(temporary, path, ec);
	if (ec) {
		log::println(log::level::error, log::category::task, "agent: could not replace '{}': {}", path.generic_display_string(), ec.message());
		std::filesystem::remove(temporary, ec);
	}
}

auto gse::ide::agent::load_sessions(data& d) -> void {
	const std::filesystem::path path = sessions_path();
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		return;
	}

	binary_reader reader(in);
	std::uint32_t magic = 0;
	std::uint32_t version = 0;
	std::uint32_t epoch = 0;
	reader & magic & version & epoch;
	if (magic != sessions_magic || version != sessions_version || epoch != archive_format_epoch) {
		return;
	}

	std::vector<session> restored;
	std::uint32_t active = 0;
	reader & restored & active;
	if (!reader.valid()) {
		log::println(log::level::error, log::category::task, "agent: could not read '{}' - previous sessions were dropped", path.generic_display_string());
		return;
	}

	d.sessions = std::move(restored);
	d.active = active;
	for (session& s : d.sessions) {
		attach_runtime(s);
		d.next_id = std::max(d.next_id, s.id);
	}
}

auto gse::ide::agent::attach_runtime(session& s) -> void {
	s.log_id = generate_temp_id(hash_combine(stable_id("agent_log"), s.id));
	s.stream = std::make_shared<spawn::output_stream>();
}

auto gse::ide::agent::create_session(data& d, const std::filesystem::path& cwd) -> session& {
	const std::uint32_t session_id = ++d.next_id;
	d.sessions.push_back({
		.id = session_id,
		.name = std::format("Agent {}", session_id),
		.cwd = cwd.empty() ? config::primary().project_root : cwd,
	});
	attach_runtime(d.sessions.back());
	d.active = session_id;
	return d.sessions.back();
}

auto gse::ide::agent::session_command(const session& s) -> std::wstring {
	std::wstring command(agent_command.begin(), agent_command.end());
	if (s.info.agent_id.empty()) {
		return command;
	}
	command += resume_option;
	command.append(s.info.agent_id.begin(), s.info.agent_id.end());
	return command;
}

auto gse::ide::agent::launch_session(session& s) -> bool {
	const credentials creds = agent_credentials();

	const spawn::launched child = spawn::launch_streamed(session_command(s), s.cwd.wstring(), creds.environment);
	if (!win32::valid_handle(child.process)) {
		return false;
	}

	s.process = child.process;
	s.job = child.job;
	s.output = child.output;
	s.input = child.input;
	s.pending.clear();
	s.running = true;
	s.stream->running.store(true, std::memory_order_release);

	if (!s.info.agent_id.empty()) {
		append_row(s, {
			.kind = row_kind::note,
			.text = std::format("resuming {}", s.info.agent_id),
		});
	}

	if (!creds.token) {
		append_row(s, {
			.kind = row_kind::failure,
			.text = "no CLAUDE_CODE_OAUTH_TOKEN in this process or in the user environment",
			.detail = "run `claude setup-token` and start a new session - headless claude cannot use the interactive login",
		});
	}

	return true;
}

auto gse::ide::agent::append_row(session& s, transcript_row row) -> void {
	spawn::emit(*s.stream, row.detail.empty() ? row.text : row.text + " - " + row.detail);
	s.rows.push_back(std::move(row));
}

auto gse::ide::agent::pump_session(session& s) -> void {
	std::vector<std::string> lines;
	const bool open = spawn::read_lines(s.output, s.pending, lines);

	for (const std::string& line : lines) {
		if (line.empty()) {
			continue;
		}
		const std::optional<analysis::json::value> event = analysis::json::parse(line);
		if (!event) {
			append_row(s, {
				.kind = row_kind::note,
				.text = line,
			});
			continue;
		}
		for (transcript_row& row : summarize(*event, s.info)) {
			append_row(s, std::move(row));
		}
	}

	if (!open) {
		append_row(s, {
			.kind = row_kind::note,
			.text = "agent exited",
		});
		s.running = false;
		s.stream->running.store(false, std::memory_order_release);
	}
}

auto gse::ide::agent::close_session(session& s) -> void {
	if (win32::valid_handle(s.input)) {
		win32::CloseHandle(s.input);
		s.input = nullptr;
	}
	if (win32::valid_handle(s.output)) {
		win32::CloseHandle(s.output);
		s.output = nullptr;
	}
	if (win32::valid_handle(s.job)) {
		win32::CloseHandle(s.job);
		s.job = nullptr;
	}
	if (win32::valid_handle(s.process)) {
		win32::CloseHandle(s.process);
		s.process = nullptr;
	}
}

auto gse::ide::agent::send_to_session(session& s, const std::string_view prompt) -> void {
	if (!s.running || !win32::valid_handle(s.input)) {
		return;
	}

	append_row(s, {
		.kind = row_kind::user,
		.text = std::string(prompt),
	});

	const std::string message = user_message(prompt);
	win32::DWORD written = 0;
	if (!win32::WriteFile(s.input, message.data(), static_cast<win32::DWORD>(message.size()), &written, nullptr)) {
		append_row(s, {
			.kind = row_kind::failure,
			.text = "failed to send to the agent",
		});
	}
}

auto gse::ide::agent::active_session(data& d) -> session* {
	if (d.sessions.empty()) {
		return nullptr;
	}
	const auto found = std::ranges::find(d.sessions, d.active, &session::id);
	return found != d.sessions.end() ? &*found : &d.sessions.back();
}

auto gse::ide::agent::row_prefix(const row_kind kind) -> std::string_view {
	switch (kind) {
		case row_kind::user:
			return "> ";
		case row_kind::tool:
			return "- ";
		case row_kind::denial:
			return "x ";
		case row_kind::failure:
			return "! ";
		default:
			return "  ";
	}
}

auto gse::ide::agent::row_color(const gui::style& sty, const row_kind kind) -> vec4f {
	switch (kind) {
		case row_kind::user:
			return sty.color_text;
		case row_kind::text:
			return sty.color_accent;
		case row_kind::tool:
			return sty.color_accent_dim;
		case row_kind::denial:
			return sty.color_warning;
		case row_kind::failure:
			return sty.color_error;
		default:
			return sty.color_text_secondary;
	}
}

auto gse::ide::agent::parse_markup(const std::string_view line) -> markup_line {
	const std::size_t indent = line.find_first_not_of(' ');
	if (indent == std::string_view::npos) {
		return { .text = std::string(line) };
	}

	const std::string_view lead = line.substr(0, indent);
	std::string_view rest = line.substr(indent);

	if (rest.starts_with("```")) {
		return {
			.text = std::string(line),
			.kind = markup::code,
		};
	}

	markup_line out;
	std::size_t hashes = 0;
	while (hashes < rest.size() && rest[hashes] == '#') {
		++hashes;
	}
	if (hashes > 0 && hashes < rest.size() && rest[hashes] == ' ') {
		out.kind = markup::heading;
		rest.remove_prefix(hashes + 1);
	}
	else if (rest.starts_with("* ") || rest.starts_with("+ ")) {
		rest.remove_prefix(2);
		out.text = "- ";
	}

	out.text.insert(0, lead);

	for (std::size_t i = 0; i < rest.size();) {
		if (rest.compare(i, 2, "**") == 0) {
			if (const std::size_t close = rest.find("**", i + 2); close != std::string_view::npos) {
				const std::size_t begin = out.text.size();
				out.text.append(rest.substr(i + 2, close - i - 2));
				out.runs.push_back({ .start = begin, .end = out.text.size(), .kind = markup::strong });
				i = close + 2;
				continue;
			}
		}
		if (rest[i] == '`') {
			if (const std::size_t close = rest.find('`', i + 1); close != std::string_view::npos) {
				const std::size_t begin = out.text.size();
				out.text.append(rest.substr(i + 1, close - i - 1));
				out.runs.push_back({ .start = begin, .end = out.text.size(), .kind = markup::code });
				i = close + 1;
				continue;
			}
		}
		out.text.push_back(rest[i]);
		++i;
	}

	return out;
}

auto gse::ide::agent::markup_color(const gui::style& sty, const markup kind, const vec4f& base) -> vec4f {
	switch (kind) {
		case markup::strong:
			return sty.color_text;
		case markup::code:
			return sty.color_file;
		case markup::heading:
			return sty.color_section_header;
		default:
			return base;
	}
}

auto gse::ide::agent::push_transcript_line(session& s, const gui::style& sty, const transcript_line& line, const transcript_metrics& metrics) -> void {
	const std::string indent(line.prefix.size(), ' ');
	const float available = metrics.width - metrics.face.width(line.prefix, metrics.scale);
	bool first = true;

	for (const std::string& source : to_lines(line.text)) {
		const markup_line parsed = line.markdown ? parse_markup(source) : markup_line{ .text = source };
		const vec4f base = markup_color(sty, parsed.kind, line.color);

		for (std::string_view segment : metrics.face.wrap(parsed.text, available, metrics.scale)) {
			while (!segment.empty() && segment.back() == ' ') {
				segment.remove_suffix(1);
			}

			const auto offset = static_cast<std::size_t>(segment.data() - parsed.text.data());
			const std::size_t last = offset + segment.size();
			std::string text = first ? std::string(line.prefix) : indent;
			const auto column = static_cast<std::uint32_t>(text.size());
			const auto index = static_cast<std::uint32_t>(s.buffer.lines.size());
			text += segment;

			if (column > 0) {
				s.spans.push_back({
					.line = index,
					.start_col = 0,
					.end_col = column,
					.color = line.color,
				});
			}

			std::size_t cursor = offset;
			for (const markup_run& run : parsed.runs) {
				if (run.end <= cursor) {
					continue;
				}
				if (run.start >= last) {
					break;
				}
				const std::size_t from = std::max(run.start, cursor);
				const std::size_t to = std::min(run.end, last);
				if (to <= from) {
					continue;
				}
				if (from > cursor) {
					s.spans.push_back({
						.line = index,
						.start_col = column + static_cast<std::uint32_t>(cursor - offset),
						.end_col = column + static_cast<std::uint32_t>(from - offset),
						.color = base,
					});
				}
				s.spans.push_back({
					.line = index,
					.start_col = column + static_cast<std::uint32_t>(from - offset),
					.end_col = column + static_cast<std::uint32_t>(to - offset),
					.color = markup_color(sty, run.kind, base),
				});
				cursor = to;
			}
			if (cursor < last) {
				s.spans.push_back({
					.line = index,
					.start_col = column + static_cast<std::uint32_t>(cursor - offset),
					.end_col = static_cast<std::uint32_t>(text.size()),
					.color = base,
				});
			}

			s.buffer.lines.push_back(std::move(text));
			s.line_rows.push_back(line.row);
			first = false;
		}
	}
}

auto gse::ide::agent::sync_transcript(session& s, const gui::style& sty, const transcript_metrics& metrics) -> void {
	if (std::abs(s.wrap_width - metrics.width) > 0.5f) {
		s.wrap_width = metrics.width;
		s.buffer.lines.clear();
		s.spans.clear();
		s.line_rows.clear();
		s.flushed_rows = 0;
	}

	for (; s.flushed_rows < s.rows.size(); ++s.flushed_rows) {
		const transcript_row& row = s.rows[s.flushed_rows];
		const auto row_index = static_cast<std::uint32_t>(s.flushed_rows);

		push_transcript_line(s, sty, {
			.prefix = row_prefix(row.kind),
			.text = row.text,
			.color = row_color(sty, row.kind),
			.row = row_index,
			.markdown = row.kind == row_kind::text,
		}, metrics);

		if (!row.detail.empty()) {
			push_transcript_line(s, sty, {
				.prefix = "    ",
				.text = row.detail,
				.color = sty.color_text_secondary,
				.row = row_index,
			}, metrics);
		}
		for (const std::string& line : row.removed) {
			push_transcript_line(s, sty, {
				.prefix = "  - ",
				.text = line,
				.color = sty.color_removed,
				.row = row_index,
			}, metrics);
		}
		for (const std::string& line : row.added) {
			push_transcript_line(s, sty, {
				.prefix = "  + ",
				.text = line,
				.color = sty.color_added,
				.row = row_index,
			}, metrics);
		}
	}
}

auto gse::ide::agent::draw_session_info(const gui::draw_context& ctx, data& d, const rectf& body) -> void {
	if (!d.info_open) {
		return;
	}

	const session* s = active_session(d);
	if (!s) {
		d.info_open = false;
		return;
	}

	const gui::style& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const float line_h = text_view->line_height(fs) * 1.35f;

	const std::array<std::pair<std::string_view, std::string>, 6> rows = { {
		{ "status", s->running ? "running" : "exited" },
		{ "model", s->info.model.empty() ? "-" : s->info.model },
		{ "session", s->info.agent_id.empty() ? "-" : s->info.agent_id },
		{ "turns", std::format("{}", s->info.turns) },
		{ "api time", std::format("{:.1f}s", s->info.api_time.as<seconds>()) },
		{ "api equivalent", std::format("${:.4f}", s->info.cost) },
	} };

	float label_w = 0.f;
	float value_w = 0.f;
	for (const auto& [label, value] : rows) {
		label_w = std::max(label_w, text_view->width(label, fs));
		value_w = std::max(value_w, text_view->width(value, fs));
	}

	const float pw = std::min(body.width(), pad * 3.f + label_w + value_w);
	const float ph = line_h * static_cast<float>(rows.size()) + pad * 2.f;

	float px = d.info_anchor.left();
	if (px + pw > body.right()) {
		px = body.right() - pw;
	}
	px = std::max(px, body.left());
	const float top_y = std::min(body.top(), d.info_anchor.bottom() - pad * 0.5f);

	const rectf panel = rectf::from_position_size({ px, top_y }, { pw, ph });
	const auto scope = ctx.scoped_layer(render_layer::popup);

	ctx.queue_sprite({
		.rect = rectf::from_position_size({ px + 4.f * sty.scale_factor, top_y - 4.f * sty.scale_factor }, { pw, ph }),
		.color = sty.color_shadow,
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius_menu,
	});
	ctx.queue_sprite({
		.rect = panel,
		.color = { vec3f(sty.color_menu_body), 1.f },
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius_menu,
	});

	for (std::size_t i = 0; i < rows.size(); ++i) {
		const float center_y = top_y - pad - line_h * (static_cast<float>(i) + 0.5f) + text_view->vertical_center_offset(fs);
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = rows[i].first,
			.position = { px + pad, center_y },
			.scale = fs,
			.color = sty.color_text_secondary,
			.clip_rect = panel,
		});
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = rows[i].second,
			.position = { panel.right() - pad - text_view->width(rows[i].second, fs), center_y },
			.scale = fs,
			.color = sty.color_text,
			.clip_rect = panel,
		});
	}

	ctx.register_hit_region(render_layer::popup, panel);

	const vec2f mouse = ctx.mouse_position();
	if (ctx.mouse_pressed() && !panel.contains(mouse) && !d.info_anchor.contains(mouse)) {
		d.info_open = false;
	}
}

auto gse::ide::agent::draw_session_tabs(gui::builder& ui, data& d, const rectf& strip) -> void {
	const gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;

	ctx.queue_sprite({
		.rect = strip,
		.color = sty.color_panel_alt,
		.texture = ctx.blank_texture,
	});

	const float button_extent = strip.height() * 0.7f;
	const float button_top = strip.center().y() + button_extent * 0.5f;
	const rectf info = rectf::from_position_size({ strip.right() - pad - button_extent, button_top }, { button_extent, button_extent });
	const float tab_limit = info.left() - pad;

	float x = strip.left() + pad;
	for (session& s : d.sessions) {
		const gse::id input_id = gui::ids::make(std::format("##agent_name_{}", s.id));
		const bool renaming = d.renaming == s.id;
		const float width = std::min(
			std::max(text_view->width(s.name, sty.font_size) + pad * 2.f, sty.font_size * 5.f),
			std::max(0.f, tab_limit - x)
		);
		if (width <= 0.f) {
			break;
		}

		const rectf tab = rectf::from_position_size({ x, strip.top() }, { width, strip.height() });
		const bool selected = s.id == d.active;

		if (renaming) {
			gui::draw::text_input_in_rect(
				ctx,
				input_id,
				s.name,
				s.name_state,
				tab,
				ui.hot_widget_id,
				ui.focus_widget_id,
				ctx.fonts.text
			);

			if (ui.focus_widget_id != input_id) {
				if (s.name.empty()) {
					s.name = std::format("Agent {}", s.id);
				}
				d.renaming = 0;
			}
		}
		else {
			ctx.queue_sprite({
				.rect = tab,
				.color = selected ? sty.color_tab_active : sty.color_tab_background,
				.texture = ctx.blank_texture,
			});
			ctx.queue_text({
				.font = ctx.fonts.text,
				.text = s.name,
				.position = { tab.left() + pad, tab.center().y() + text_view->vertical_center_offset(sty.font_size) },
				.scale = sty.font_size,
				.color = s.running ? sty.color_text : sty.color_text_disabled,
				.clip_rect = tab,
			});

			if (ctx.mouse_pressed_for(tab)) {
				const bool second_click = gui::interaction::register_click(d.tab_click, ctx.mouse_position()) >= 2;
				if (second_click && selected) {
					d.renaming = s.id;
					s.name_state.anchor = 0;
					s.name_state.caret = static_cast<int>(s.name.size());
					ui.focus_widget_id = input_id;
				}
				d.active = s.id;
			}
		}

		x += width;
	}

	if (const rectf add = rectf::from_position_size({ x + pad, button_top }, { button_extent, button_extent }); add.right() <= tab_limit) {
		if (gui::caption_button(ui, add, "##agent_new", gui::symbol::plus(), sty.color_tab_hovered)) {
			create_session(d, config::primary().project_root);
		}
	}

	session* s = active_session(d);
	if (!s) {
		d.info_anchor = {};
		d.info_open = false;
		return;
	}

	if (gui::caption_button(ui, info, std::format("##agent_info_{}", s->id), gui::symbol::info(), sty.color_widget_hovered)) {
		d.info_open = !d.info_open;
	}
	d.info_anchor = info;
}

auto gse::ide::agent::draw_transcript(gui::builder& ui, data& d, const rectf& area, const vec2f mouse, const channel_write<build_runner::stream_opened, gui::menu_content, jump_to_request> jump_out) -> void {
	const gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;

	session* s = active_session(d);
	if (!s || s->rows.empty()) {
		float y = area.top() - pad - sty.font_size;
		const std::string hint = s
			? "Type a prompt below to start claude in " + s->cwd.generic_display_string() + "."
			: "No agents yet. Type a prompt below, or press + to add a session, in " + config::primary().name + ".";
		for (const std::string_view line : ctx.fonts.text.resolve()->wrap(hint, area.width() - pad * 2.f, sty.font_size)) {
			ctx.queue_text({
				.font = ctx.fonts.text,
				.text = line,
				.position = { area.left() + pad, y },
				.scale = sty.font_size,
				.color = sty.color_text_secondary,
				.clip_rect = area,
			});
			y -= sty.font_size * 1.45f;
		}
		return;
	}

	const auto code_view = ctx.fonts.code.resolve();
	sync_transcript(*s, sty, {
		.face = *code_view,
		.width = std::max(0.f, area.width() - pad * 2.f - gui::scroll_config{}.scrollbar_width),
		.scale = sty.font_size,
	});

	if (s->buffer.lines.empty()) {
		s->buffer.lines.emplace_back();
		s->line_rows.push_back(0);
	}

	if (ctx.mouse_pressed_for(area)) {
		const gui::buffer_position at = gui::draw::text_area_position_at(ctx, s->buffer, s->view, area, false, 4, mouse);
		const std::size_t line = std::min<std::size_t>(at.line, s->line_rows.size() - 1);
		const std::size_t owner = std::min<std::size_t>(s->line_rows[line], s->rows.empty() ? 0 : s->rows.size() - 1);
		if (!s->rows.empty() && !s->rows[owner].file.empty()) {
			jump_out.push<jump_to_request>({
				.path = s->rows[owner].file,
				.line = jump_line_for(s->rows[owner]),
				.column = 0,
			});
		}
	}

	gui::draw::text_area_in_rect(
		ctx,
		s->log_id,
		{
			.buffer = s->buffer,
			.state = s->view,
			.spans = s->spans,
			.rect = area,
			.read_only = true,
			.blink_interval = time{},
		},
		ui.hot_widget_id,
		ui.focus_widget_id
	);
}

auto gse::ide::agent::draw_input(gui::builder& ui, data& d, const rectf& area, const channel_write<build_runner::stream_opened, gui::menu_content, jump_to_request> stream_out) -> void {
	const gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;

	const id input_id = gui::ids::make("##agent_input");
	const bool submit = ui.focus_widget_id == input_id && !d.input.empty() && ctx.key_pressed_for(key::enter);

	ctx.queue_sprite({
		.rect = area,
		.color = sty.color_input_background,
		.texture = ctx.blank_texture,
	});

	const auto code_view = ctx.fonts.code.resolve();
	constexpr std::string_view marker = ">";
	const float marker_width = code_view->width(marker, sty.font_size) + pad;

	ctx.queue_text({
		.font = ctx.fonts.code,
		.text = marker,
		.position = { area.left() + pad, area.center().y() + code_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_accent,
		.clip_rect = area,
	});

	const rectf box = rectf::from_position_size(
		{ area.left() + pad + marker_width, area.top() },
		{ std::max(0.f, area.width() - pad * 2.f - marker_width), area.height() }
	);
	gui::draw::text_input_in_rect(ctx, input_id, d.input, d.input_state, box, ui.hot_widget_id, ui.focus_widget_id, ctx.fonts.code);

	if (!submit) {
		return;
	}

	const std::string prompt = d.input;
	d.input.clear();
	d.input_state = {};

	session* s = active_session(d);
	if (!s) {
		s = &create_session(d, config::primary().project_root);
	}

	if (!s->running && !launch_session(*s)) {
		append_row(*s, {
			.kind = row_kind::failure,
			.text = "failed to launch 'claude' - is it on PATH?",
		});
		return;
	}

	if (!s->announced) {
		s->announced = true;
		stream_out.push<build_runner::stream_opened>({
			.name = s->name,
			.slot = build_runner::stream_slot::none,
			.stream = s->stream,
		});
	}

	send_to_session(*s, prompt);
}

auto gse::ide::agent::draw_panel(gui::builder& ui, data& d, const vec2f mouse, const channel_write<build_runner::stream_opened, gui::menu_content, jump_to_request> jump_out) -> void {
	const gui::draw_context& ctx = ui.ctx;
	if (ctx.clip_stack.empty()) {
		return;
	}

	const gui::style& sty = ctx.style;
	const rectf body = ctx.clip_stack.back();
	const float strip_h = sty.font_size * 2.f;
	const float input_h = sty.font_size * 2.f;

	const rectf strip = rectf::from_position_size({ body.left(), body.top() }, { body.width(), strip_h });
	const rectf input_area = rectf::from_position_size({ body.left(), body.bottom() + input_h }, { body.width(), input_h });
	const rectf transcript = rectf::from_position_size(
		{ body.left(), body.top() - strip_h },
		{ body.width(), std::max(0.f, body.height() - strip_h - input_h) }
	);

	draw_session_tabs(ui, d, strip);
	draw_input(ui, d, input_area, jump_out);
	draw_transcript(ui, d, transcript, mouse, jump_out);
	draw_session_info(ctx, d, body);
}

auto gse::ide::agent::run(context& ctx, data& d, const channel_read<start_request> requests_in, const channel_write<build_runner::stream_opened, gui::menu_content, jump_to_request> events_out, const shared_view<input::data> input_d) -> async::task<> {
	if (!d.initialized) {
		load_sessions(d);
		d.initialized = true;
	}

	if (d.sessions.empty()) {
		create_session(d, config::primary().project_root);
	}

	for (const start_request& request : requests_in.of<start_request>()) {
		session& started = create_session(d, request.cwd);
		if (!launch_session(started)) {
			log::println(log::level::error, log::category::task, "agent: failed to launch 'claude' - is it on PATH?");
			d.sessions.pop_back();
			continue;
		}
		started.announced = true;
		events_out.push<build_runner::stream_opened>({
			.name = started.name,
			.slot = build_runner::stream_slot::none,
			.stream = started.stream,
		});
		send_to_session(started, request.prompt);
	}

	for (session& s : d.sessions) {
		if (s.running) {
			pump_session(s);
		}
		else if (win32::valid_handle(s.process)) {
			close_session(s);
		}
	}

	events_out.push<gui::menu_content>({
		.menu = std::string(panel_name),
		.layer = render_layer::content,
		.build = [d = &d, mouse = input::current_state(input_d).mouse_position(), jump_out = events_out](gui::builder& b) {
			draw_panel(b, *d, mouse, jump_out);
		},
	});

	return {};
}

auto gse::ide::agent::shutdown(data& d) -> void {
	save_sessions(d);
	for (session& s : d.sessions) {
		close_session(s);
	}
	d.sessions.clear();
}

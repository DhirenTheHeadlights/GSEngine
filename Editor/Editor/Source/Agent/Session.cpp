module gse.ide.agent:session_impl;

import std;
import gse;
import gse.win32;
import gse.win32.environment;

import gse.ide.analysis;
import gse.ide.build;
import gse.ide.config;

import :blame;
import :chats;
import :layout;
import :model;
import :phase;
import :session;
import :stream;

namespace {
	std::atomic_bool token_prompt_requested = false;
}

auto gse::ide::agent::request_token_prompt() -> void {
	token_prompt_requested.store(true);
}

auto gse::ide::agent::take_token_prompt_request() -> bool {
	return token_prompt_requested.exchange(false);
}

auto gse::ide::agent::save_agent_token(const std::string_view token) -> bool {
	const std::wstring value(token.begin(), token.end());
	if (!win32::write_user_registry_string(L"Environment", oauth_token_name.data(), value.c_str())) {
		return false;
	}
	return win32::SetEnvironmentVariableW(oauth_token_name.data(), value.c_str()) != 0;
}

auto gse::ide::agent::agent_credentials() -> credentials {
	// Prefer the saved user value so an editor launched by an older parent
	// process still picks up a token changed in the editor.
	std::wstring value = win32::user_environment_value(oauth_token_name);
	if (value.empty()) {
		value = environment_value(oauth_token_name);
	}
	if (value.empty()) {
		return {};
	}

	return {
		.environment = win32::environment_with_variable(oauth_token_name, value),
		.token_value = std::string(value.begin(), value.end()),
		.token = true,
	};
}

auto gse::ide::agent::sessions_path() -> std::filesystem::path {
	return config::project_state_dir() / "agent_sessions.bin";
}

auto gse::ide::agent::save_sessions(const data& d) -> void {
	std::ostringstream out(std::ios::binary);
	binary_writer writer(out, sessions_magic, sessions_version);
	writer & d.sessions & d.active & d.chat_names & d.overview_active;
	layout_store::replace(sessions_path(), std::move(out).str());
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
	if (magic != sessions_magic || epoch != archive_format_epoch) {
		log::println(log::level::warning, log::category::task, "agent: '{}' is magic {:#x} epoch {}, expected magic {:#x} epoch {} - previous sessions were dropped", path.generic_display_string(), magic, epoch, sessions_magic, archive_format_epoch);
		return;
	}
	if (version > sessions_version) {
		log::println(log::level::warning, log::category::task, "agent: '{}' is version {}, newer than this editor's version {} - previous sessions were dropped", path.generic_display_string(), version, sessions_version);
		return;
	}
	if (version < first_schema_sessions_version) {
		log::println(log::level::warning, log::category::task, "agent: '{}' is version {}, written before the schema was embedded at version {} - previous sessions were dropped", path.generic_display_string(), version, first_schema_sessions_version);
		return;
	}

	std::vector<session> restored;
	std::uint32_t active = 0;
	std::unordered_map<std::string, std::string> names;
	bool overview = true;
	reader & restored & active;
	if (version >= chat_names_sessions_version) {
		reader & names;
	}
	if (version >= overview_sessions_version) {
		reader & overview;
	}
	if (!reader.valid()) {
		log::println(log::level::error, log::category::task, "agent: could not read '{}' - previous sessions were dropped", path.generic_display_string());
		return;
	}

	for (const std::string& skipped : reader.skipped_fields()) {
		log::println(log::level::warning, log::category::task, "agent: '{}' was written at version {}, dropping field {}", path.generic_display_string(), version, skipped);
	}

	d.sessions = std::move(restored);
	d.active = active;
	d.chat_names = std::move(names);
	d.overview_active = overview;
	for (session& s : d.sessions) {
		std::erase_if(s.unbuilt, [](const auto& entry) {
			return entry.second == 0;
		});
		std::erase_if(s.touched, [](const auto& entry) {
			return entry.second.mtime == 0;
		});
		attach_runtime(s);
		d.next_id = std::max(d.next_id, s.id);
	}
}

auto gse::ide::agent::adopt_inherited(data& d) -> void {
	for (const handoff& adopted : inherited_handoffs()) {
		if (!inherited_handle(adopted.output) || !inherited_handle(adopted.input)) {
			log::println(log::level::warning, log::category::task, "agent: session {} was handed off with handles this process never inherited - ignoring the stale argument", adopted.session);
			continue;
		}

		const auto match = std::ranges::find(d.sessions, adopted.session, &session::id);
		if (match != d.sessions.end() && !match->running && adopt_session(*match, adopted)) {
			continue;
		}
		discard_handoff(adopted);
	}

	std::erase_if(d.sessions, [](const session& s) {
		return s.sync.has_value() && !s.running;
	});

	if (!d.overview_active && std::ranges::none_of(d.sessions, [&](const session& s) {
		return s.id == d.active;
	})) {
		d.overview_active = true;
	}
}

auto gse::ide::agent::inherited_handle(void* handle) -> bool {
	win32::DWORD flags = 0;
	return win32::valid_handle(handle) && win32::GetHandleInformation(handle, &flags) != 0;
}

auto gse::ide::agent::discard_handoff(const handoff& adopted) -> void {
	spawn::terminate(adopted.process, adopted.job);
	for (void* handle : { adopted.process, adopted.job, adopted.output, adopted.input }) {
		if (inherited_handle(handle)) {
			win32::CloseHandle(handle);
		}
	}
}

auto gse::ide::agent::attach_runtime(session& s) -> void {
	s.full.filter = row_filter::conversation;
	s.changes.filter = row_filter::changes;
	s.everything.filter = row_filter::all;
	s.followup.filter = row_filter::conversation;

	for (transcript_view* v : views_of(s)) {
		v->log_id = generate_temp_id(hash_of("agent_log", s.id, v->filter));
	}
	s.followup.log_id = generate_temp_id(hash_of("agent_followup", s.id));
}

auto gse::ide::agent::inherited_handoffs() -> std::vector<handoff> {
	int count = 0;
	wchar_t** arguments = win32::CommandLineToArgvW(win32::GetCommandLineW(), &count);
	if (arguments == nullptr) {
		return {};
	}

	std::vector<handoff> adopted;
	for (int index = 1; index < count; ++index) {
		const std::wstring_view argument(arguments[index]);
		if (!argument.starts_with(handoff_option)) {
			continue;
		}

		std::array<std::uintptr_t, 5> fields{};
		std::size_t field = 0;
		bool parsed = true;
		for (std::wstring_view rest = argument.substr(handoff_option.size()); parsed && field < fields.size(); ++field) {
			const std::size_t comma = rest.find(L',');
			const std::wstring_view token = rest.substr(0, comma);
			for (const wchar_t digit : token) {
				if (digit < L'0' || digit > L'9') {
					parsed = false;
					break;
				}
				fields[field] = fields[field] * 10u + static_cast<std::uintptr_t>(digit - L'0');
			}
			parsed = parsed && !token.empty();
			rest = comma == std::wstring_view::npos ? std::wstring_view{} : rest.substr(comma + 1);
		}

		if (!parsed || field != fields.size()) {
			continue;
		}

		adopted.push_back({
			.session = static_cast<std::uint32_t>(fields[0]),
			.process = reinterpret_cast<void*>(fields[1]),
			.job = reinterpret_cast<void*>(fields[2]),
			.output = reinterpret_cast<void*>(fields[3]),
			.input = reinterpret_cast<void*>(fields[4]),
		});
	}

	win32::LocalFree(arguments);
	return adopted;
}

auto gse::ide::agent::adopt_session(session& s, const handoff& adopted) -> bool {
	if (!win32::valid_handle(adopted.output) || !win32::valid_handle(adopted.input)) {
		return false;
	}

	s.process = adopted.process;
	s.job = adopted.job;
	s.output = adopted.output;
	s.input = adopted.input;
	s.running = true;
	s.launched_model_id = s.model_id;
	s.launched_effort = s.requested_effort;

	for (void* handle : { s.process, s.job, s.output, s.input }) {
		if (win32::valid_handle(handle)) {
			win32::SetHandleInformation(handle, win32::handle_flag_inherit, 0);
		}
	}

	append_row(s, {
		.kind = row_kind::note,
		.text = "reattached across the editor restart",
	});
	return true;
}

auto gse::ide::agent::hand_off_session(session& s) -> bool {
	if (!s.running || !win32::valid_handle(s.output) || !win32::valid_handle(s.input)) {
		return false;
	}

	std::vector<void*> handles;
	for (void* handle : { s.process, s.job, s.output, s.input }) {
		if (win32::valid_handle(handle)) {
			handles.push_back(handle);
		}
	}

	app::add_relaunch_handoff(handles, std::format(
		L"{}{},{},{},{},{}",
		handoff_option,
		s.id,
		reinterpret_cast<std::uintptr_t>(s.process),
		reinterpret_cast<std::uintptr_t>(s.job),
		reinterpret_cast<std::uintptr_t>(s.output),
		reinterpret_cast<std::uintptr_t>(s.input)
	));
	return true;
}

auto gse::ide::agent::create_session(data& d, const std::filesystem::path& cwd, const task_phase starting) -> session& {
	const std::uint32_t session_id = ++d.next_id;
	d.sessions.push_back({
		.id = session_id,
		.name = std::format("Agent {}", session_id),
		.cwd = cwd.empty() ? config::primary().project_root : cwd,
		.hydrated = true,
		.runs = { {
			.phase = starting,
			.started = unix_now(),
		} },
		.model_id = d.default_model.value,
		.requested_effort = d.default_effort,
	});
	attach_runtime(d.sessions.back());
	d.active = session_id;
	return d.sessions.back();
}

auto gse::ide::agent::create_sync_session(data& d, sync_request request) -> session& {
	const std::uint32_t session_id = ++d.next_id;
	d.sessions.push_back({
		.id = session_id,
		.name = std::format("Sync {}", request.root.filename().generic_display_string()),
		.cwd = request.root,
		.hydrated = true,
		.mode = transcript_mode::transcript,
		.sync = std::move(request),
		.runs = { {
			.phase = task_phase::settled,
			.started = unix_now(),
		} },
		.model_id = d.default_model.value,
		.requested_effort = d.default_effort,
	});
	attach_runtime(d.sessions.back());
	return d.sessions.back();
}

auto gse::ide::agent::is_chat(const session& s) -> bool {
	return !s.sync.has_value();
}

auto gse::ide::agent::sync_session(data& d) -> session* {
	const auto found = std::ranges::find_if_not(d.sessions, is_chat);
	return found != d.sessions.end() ? &*found : nullptr;
}

auto gse::ide::agent::session_command(const session& s, const std::filesystem::path& prompt) -> std::wstring {
	std::wstring command(agent_command.begin(), agent_command.end());

	if (s.sync) {
		command.append(sync_denied_tools.begin(), sync_denied_tools.end());
	}
	else if (policy_of(current_phase(s)).read_only) {
		command.append(read_only_denied_tools.begin(), read_only_denied_tools.end());
	}

	if (!prompt.empty()) {
		command += L" --append-system-prompt-file \"";
		command += prompt.wstring();
		command += L'"';
	}

	if (!s.model_id.empty()) {
		command += L" --model ";
		command.append(s.model_id.begin(), s.model_id.end());
	}
	if (s.requested_effort != agent_effort::inherit) {
		const std::string_view level = enum_to_string(s.requested_effort);
		command += L" --effort ";
		command.append(level.begin(), level.end());
	}

	const std::filesystem::path server = config::worktree_for(s.cwd).engine_root / "Tools" / "gse-mcp" / "server.mjs";
	const std::string mcp = std::format(mcp_config_format, server.generic_display_string());
	command.append(mcp.begin(), mcp.end());

	if (s.info.agent_id.empty()) {
		return command;
	}
	command += resume_option;
	command.append(s.info.agent_id.begin(), s.info.agent_id.end());
	return command;
}

auto gse::ide::agent::launch_session(session& s) -> bool {
	const credentials creds = agent_credentials();
	if (!creds.token) {
		request_token_prompt();
		s.info.failure = "Set a Claude token in the agent panel before starting a session.";
		return false;
	}

	const spawn::launched child = spawn::launch_streamed(session_command(s, write_phase_prompt(s)), s.cwd.wstring(), creds.environment);
	if (!win32::valid_handle(child.process)) {
		s.info.failure = "Could not launch Claude. Check that claude is on PATH.";
		return false;
	}

	s.process = child.process;
	s.job = child.job;
	s.output = child.output;
	s.input = child.input;
	s.pending.clear();
	s.running = true;
	s.launched_model_id = s.model_id;
	s.launched_effort = s.requested_effort;
	s.info.failure.clear();

	if (!s.info.agent_id.empty()) {
		append_row(s, {
			.kind = row_kind::note,
			.text = std::format("resuming {}", s.info.agent_id),
		});
	}
	return true;
}

auto gse::ide::agent::inquiry_command(const session& s) -> std::wstring {
	std::wstring command(inquiry_command_head.begin(), inquiry_command_head.end());
	if (!s.model_id.empty()) {
		command += L" --model ";
		command.append(s.model_id.begin(), s.model_id.end());
	}
	command += resume_option;
	command.append(s.info.agent_id.begin(), s.info.agent_id.end());
	return command;
}

auto gse::ide::agent::launch_inquiry(const session& s, const std::string_view prompt) -> inquiry_fork {
	const spawn::launched child = spawn::launch_streamed(inquiry_command(s), s.cwd.wstring(), agent_credentials().environment);
	inquiry_fork fork{
		.session = s.id,
		.process = child.process,
		.job = child.job,
		.output = child.output,
	};
	if (!win32::valid_handle(child.process)) {
		fork.verdict = inquiry_verdict::silent;
		return fork;
	}

	const std::string message = user_message(prompt, {});
	win32::DWORD written = 0;
	if (!win32::WriteFile(child.input, message.data(), static_cast<win32::DWORD>(message.size()), &written, nullptr)) {
		fork.verdict = inquiry_verdict::silent;
	}
	win32::CloseHandle(child.input);
	if (fork.verdict != inquiry_verdict::pending) {
		end_inquiry(fork);
	}
	return fork;
}

auto gse::ide::agent::pump_inquiry(inquiry_fork& fork) -> void {
	if (fork.verdict != inquiry_verdict::pending) {
		return;
	}

	std::vector<std::string> lines;
	const bool open = spawn::read_lines(fork.output, fork.pending, lines);
	for (const std::string& line : lines) {
		const std::optional<analysis::json::value> event = analysis::json::parse(line);
		if (!event || string_at(*event, "type") != "result") {
			continue;
		}
		fork.answer = string_at(*event, "result");

		std::string_view verdict = fork.answer;
		verdict = verdict.substr(0, verdict.find_last_not_of(" \t\r\n") + 1);
		if (const std::size_t newline = verdict.rfind('\n'); newline != std::string_view::npos) {
			verdict.remove_prefix(newline + 1);
		}
		fork.verdict = verdict.contains("NOT_MINE")
			? inquiry_verdict::denied
			: verdict.contains("CLAIM") ? inquiry_verdict::claimed : inquiry_verdict::silent;
	}

	if (!open && fork.verdict == inquiry_verdict::pending) {
		fork.verdict = inquiry_verdict::silent;
	}
	if (fork.verdict != inquiry_verdict::pending) {
		end_inquiry(fork);
	}
}

auto gse::ide::agent::end_inquiry(inquiry_fork& fork) -> void {
	spawn::terminate(fork.process, fork.job);
	for (void** handle : { &fork.output, &fork.job, &fork.process }) {
		if (win32::valid_handle(*handle)) {
			win32::CloseHandle(*handle);
			*handle = nullptr;
		}
	}
	if (fork.verdict == inquiry_verdict::pending) {
		fork.verdict = inquiry_verdict::silent;
	}
}

auto gse::ide::agent::append_row(session& s, transcript_row row) -> void {
	hydrate_session(s);

	if (row.kind == row_kind::retry && !s.rows.empty() && s.rows.back().kind == row_kind::retry) {
		return;
	}

	if (row.kind == row_kind::tool && !row.file.empty() && !(row.added.empty() && row.removed.empty())) {
		note_written_file(s, row.file, row.added);
		record_step_edit(s, row);
	}
	if (row.stamped == 0) {
		row.stamped = unix_now();
	}
	row.agent = s.info.agent_id;
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
		const std::string_view kind = string_at(*event, "type");
		if (const std::int64_t resets = limit_reset_at(*event); resets > s.limited_until) {
			s.limited_until = resets;
		}
		if (kind == "result") {
			s.think_clock.reset();
			s.recent_turn.emplace();
			s.wrote_this_turn = false;
			if (!turn_failed(*event)) {
				s.limited_until = 0;
			}
			else if (usage_limited(*event) && s.limited_until <= unix_now()) {
				s.limited_until = unix_now() + limit_backoff_seconds;
			}
		}
		else if (kind == "assistant" || kind == "user") {
			if (!s.think_clock) {
				s.think_clock.emplace();
			}
			if (kind == "user" || s.action.empty()) {
				s.action = thinking_phrase(s);
			}
		}

		const std::string_view uuid = anchorable(*event) ? string_at(*event, "uuid") : std::string_view{};
		std::vector<transcript_row> rows = summarize(*event, s.info);
		for (const transcript_row& row : rows) {
			if (row.kind == row_kind::tool) {
				s.action = action_phrase(row);
			}
			else if (row.kind == row_kind::text) {
				s.action = "writing back";
			}
		}
		for (transcript_row& row : rows) {
			row.uuid = uuid;
			append_row(s, std::move(row));
		}

		if (kind != "result") {
			continue;
		}
		s.turn_open = false;
		s.woke_by.clear();
		if (retryable_failure(*event)) {
			arm_retry(s);
		}
		else if (!turn_failed(*event)) {
			s.turn_ended = true;
			s.retry.waiting = false;
			s.retry.attempts = 0;
			s.resume_attempts = 0;
		}
	}

	track_run_id(s);

	if (!open) {
		append_row(s, {
			.kind = row_kind::note,
			.text = "agent exited",
		});
		s.running = false;
		s.think_clock.reset();
		s.wrote_this_turn = false;
	}
}

auto gse::ide::agent::stop_session(session& s) -> void {
	if (s.running) {
		spawn::terminate(s.process, s.job);
		s.running = false;
	}
	close_session(s);
	s.pending.clear();
	s.turn_open = false;
	s.think_clock.reset();
	s.wrote_this_turn = false;
	s.action.clear();
}

auto gse::ide::agent::restart_session(session& s) -> void {
	stop_session(s);

	if (!launch_session(s)) {
		append_row(s, {
			.kind = row_kind::failure,
			.text = "could not relaunch 'claude' with the new settings",
			.detail = "the conversation is intact - press + to start a new chat, or reopen the panel",
		});
	}
}

auto gse::ide::agent::close_session(session& s) -> void {
	if (s.fork.process) {
		end_inquiry(*s.fork.process);
	}
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

auto gse::ide::agent::erase_session(data& d, const std::uint32_t session_id) -> void {
	const auto found = std::ranges::find(d.sessions, session_id, &session::id);
	if (found == d.sessions.end()) {
		return;
	}

	if (found->running) {
		spawn::terminate(found->process, found->job);
		found->running = false;
	}
	close_session(*found);

	if (!found->info.agent_id.empty() && !found->name.empty()) {
		d.chat_names[found->info.agent_id] = found->name;
	}

	const auto index = static_cast<std::size_t>(std::distance(d.sessions.begin(), found));
	const bool was_active = d.active == session_id;
	d.sessions.erase(found);

	if (d.renaming == session_id) {
		d.renaming = 0;
	}
	if (d.pending_close == session_id) {
		d.pending_close = 0;
	}
	if (d.sessions.empty()) {
		d.active = 0;
		d.info_open = false;
	}
	else if (was_active) {
		d.active = d.sessions[std::min(index, d.sessions.size() - 1)].id;
	}
}

auto gse::ide::agent::request_close(data& d, const std::uint32_t session_id) -> void {
	const auto found = std::ranges::find(d.sessions, session_id, &session::id);
	if (found == d.sessions.end()) {
		return;
	}
	if (found->running) {
		d.pending_close = session_id;
		return;
	}
	erase_session(d, session_id);
}

auto gse::ide::agent::send_to_session(session& s, const std::string_view prompt, const std::span<const gui::image_attachment> attachments, const bool editor_driven, std::optional<transcript_row> shown) -> bool {
	const bool settings_stale = s.launched_model_id != s.model_id || s.launched_effort != s.requested_effort;
	if (s.running && !s.think_clock && settings_stale) {
		restart_session(s);
	}
	if (!s.running || !win32::valid_handle(s.input)) {
		s.info.failure = "Message not sent: Claude is not connected. Your draft has been kept.";
		append_row(s, { .kind = row_kind::failure, .text = s.info.failure });
		return false;
	}

	const std::string message = user_message(prompt, attachments);
	win32::DWORD written = 0;
	if (!win32::WriteFile(s.input, message.data(), static_cast<win32::DWORD>(message.size()), &written, nullptr)
		|| written != message.size()) {
		stop_session(s);
		s.info.failure = "Message not sent: the Claude connection failed. Your draft has been kept; try sending again.";
		append_row(s, { .kind = row_kind::failure, .text = s.info.failure });
		return false;
	}

	s.info.failure.clear();
	s.retry.waiting = false;
	s.retry.held = true;
	s.turn_open = true;
	s.editor_turn = editor_driven;
	s.turn_ended = false;
	s.hibernating = false;
	s.wake_prompt.clear();
	s.asleep_blocker.clear();
	s.woke_by.clear();

	append_row(s, shown ? std::move(*shown) : transcript_row{
		.kind = row_kind::user,
		.text = std::string(prompt),
		.detail = attachments.empty()
			? std::string{}
			: std::format("{} image(s) attached", attachments.size()),
	});


	s.action = thinking_phrase(s);
	s.think_clock.emplace();
	return true;
}

auto gse::ide::agent::interrupt_session(session& s) -> void {
	s.retry.waiting = false;
	s.turn_open = false;

	if (!s.running || !win32::valid_handle(s.input) || !s.think_clock) {
		return;
	}

	const std::string message = std::format(
		R"({{"type":"control_request","request_id":"stop_{}_{}","request":{{"subtype":"interrupt"}}}})" "\n",
		s.id,
		s.next_control++
	);

	win32::DWORD written = 0;
	if (!win32::WriteFile(s.input, message.data(), static_cast<win32::DWORD>(message.size()), &written, nullptr)) {
		append_row(s, {
			.kind = row_kind::failure,
			.text = "failed to interrupt the agent",
		});
		return;
	}

	s.action = "stopping";
}

auto gse::ide::agent::active_session(data& d) -> session* {
	session* last = nullptr;
	for (session& s : d.sessions) {
		if (!is_chat(s)) {
			continue;
		}
		if (s.id == d.active) {
			return &s;
		}
		last = &s;
	}
	return last;
}

auto gse::ide::agent::environment_value(const std::wstring_view name) -> std::wstring {
	const win32::DWORD needed = win32::GetEnvironmentVariableW(name.data(), nullptr, 0);
	if (needed == 0) {
		return {};
	}

	std::wstring value(needed, L'\0');
	const win32::DWORD written = win32::GetEnvironmentVariableW(name.data(), value.data(), needed);
	value.resize(written);
	return value;
}

auto gse::ide::agent::environment_path(const std::wstring_view name) -> std::filesystem::path {
	const std::wstring value = environment_value(name);
	if (value.empty()) {
		return {};
	}

	return value;
}

auto gse::ide::agent::refresh_usage(data& d) -> void {
	if (d.usage_client && d.usage_ticket.exists()) {
		for (http::completion& done : d.usage_client->poll()) {
			if (done.ticket == d.usage_ticket) {
				d.usage_ticket.reset();
				apply_usage(d, done.value);
			}
		}
		return;
	}

	if (!d.info_open || unix_now() - d.usage.fetched < usage_refresh_seconds) {
		return;
	}
	d.usage.fetched = unix_now();

	const credentials creds = agent_credentials();
	if (creds.token_value.empty()) {
		d.usage.windows.clear();
		d.usage.error = "no CLAUDE_CODE_OAUTH_TOKEN";
		return;
	}

	if (!d.usage_client) {
		d.usage_client = std::make_unique<http::client>();
	}

	d.usage_ticket = d.usage_client->send({
		.url = std::string(usage_url),
		.headers = {
			{ .name = "Authorization", .value = "Bearer " + creds.token_value },
			{ .name = "anthropic-beta", .value = std::string(oauth_beta) },
			{ .name = "Content-Type", .value = "application/json" },
		},
		.timeout = seconds(10.f),
	});
}

auto gse::ide::agent::apply_usage(data& d, const http::result& result) -> void {
	if (!result) {
		d.usage.error = http::message(result.error());
		return;
	}
	if (!result->ok()) {
		d.usage.error = std::format("usage request returned {}", result->status);
		return;
	}

	const std::optional<analysis::json::value> root = analysis::json::parse(result->body);
	if (!root || !root->is_object()) {
		d.usage.error = "usage response was not json";
		return;
	}

	constexpr std::pair<std::string_view, std::string_view> windows[] = {
		{ "five_hour", "session (5h)" },
		{ "seven_day", "week (all models)" },
		{ "seven_day_opus", "week (opus)" },
		{ "seven_day_sonnet", "week (sonnet)" },
	};

	d.usage.windows.clear();
	d.usage.error.clear();
	for (const auto& [key, label] : windows) {
		const analysis::json::value* window = root->find(key);
		if (!window || !window->is_object()) {
			continue;
		}
		const analysis::json::value* used = window->find("utilization");
		const analysis::json::value* resets = window->find("resets_at");
		if (!used || used->type != analysis::json::value::kind::number) {
			continue;
		}
		d.usage.windows.push_back({
			.label = std::string(label),
			.utilization = used->number,
			.resets_at = !resets
				? 0
				: resets->type == analysis::json::value::kind::number
				? resets->as_int()
				: parse_timestamp(resets->as_string()),
		});
	}

	if (d.usage.windows.empty()) {
		d.usage.error = "no usage windows reported";
	}
}

auto gse::ide::agent::load_model_options() -> std::vector<model_option> {
	std::vector<model_option> out;
	for (const std::string_view alias : builtin_model_aliases) {
		out.push_back({
			.value = std::string(alias),
			.label = std::string(alias),
		});
	}

	const std::filesystem::path file = claude_config_file();
	std::ifstream in(file, std::ios::binary);
	if (!in) {
		log::println(log::level::warning, log::category::task, "agent: could not read '{}' - the model list is empty, so chats use the claude default", file);
		return out;
	}

	const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	const std::optional<analysis::json::value> root = analysis::json::parse(text);
	if (!root) {
		log::println(log::level::warning, log::category::task, "agent: '{}' is not valid json - the model list is empty", file);
		return out;
	}

	const analysis::json::value* cached = root->find("additionalModelOptionsCache");
	if (!cached || !cached->is_array()) {
		log::println(log::level::info, log::category::task, "agent: '{}' has no cached model list yet - open the model picker in a terminal claude once to populate it", file);
		return out;
	}

	for (const analysis::json::value& entry : cached->children) {
		const analysis::json::value* id = entry.find("value");
		if (!id || id->as_string().empty()) {
			continue;
		}
		const analysis::json::value* disabled = entry.find("disabled");
		if (disabled && disabled->type == analysis::json::value::kind::boolean && disabled->boolean) {
			continue;
		}
		if (std::ranges::any_of(out, [id](const model_option& known) {
			return known.value == id->as_string();
		})) {
			continue;
		}

		const analysis::json::value* described = entry.find("description");
		const analysis::json::value* labelled = entry.find("label");
		std::string_view label = described ? described->as_string() : std::string_view{};
		label = label.substr(0, label.find(" \xC2\xB7 "));
		if (label.empty() && labelled) {
			label = labelled->as_string();
		}

		out.push_back({
			.value = std::string(id->as_string()),
			.label = label.empty() ? std::string(id->as_string()) : std::string(label),
		});
	}

	log::println(log::level::info, log::category::task, "agent: {} model(s) available from '{}'", out.size(), file);
	return out;
}

auto gse::ide::agent::available_models() -> std::span<const model_option> {
	static const std::vector<model_option> cached = load_model_options();
	return cached;
}

auto gse::ide::agent::unix_now() -> std::int64_t {
	return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

auto gse::ide::agent::claude_config_file() -> std::filesystem::path {
	const std::filesystem::path home = claude_home();
	return home.empty() ? std::filesystem::path{} : home.parent_path() / ".claude.json";
}

auto gse::ide::agent::claude_home() -> std::filesystem::path {
	if (std::filesystem::path configured = environment_path(config_dir_name); !configured.empty()) {
		return configured;
	}

	const std::filesystem::path profile = environment_path(user_profile_name);
	if (profile.empty()) {
		return {};
	}

	return profile / ".claude";
}

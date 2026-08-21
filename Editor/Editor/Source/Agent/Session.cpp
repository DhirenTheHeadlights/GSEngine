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
import :model;
import :session;
import :stream;

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
		writer & d.sessions & d.active & d.chat_names;
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
	reader & restored & active;
	if (version >= chat_names_sessions_version) {
		reader & names;
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
		const auto match = std::ranges::find(d.sessions, adopted.session, &session::id);
		if (match != d.sessions.end() && adopt_session(*match, adopted)) {
			continue;
		}
		discard_handoff(adopted);
	}
}

auto gse::ide::agent::discard_handoff(const handoff& adopted) -> void {
	spawn::terminate(adopted.process, adopted.job);
	for (void* handle : { adopted.process, adopted.job, adopted.output, adopted.input }) {
		if (win32::valid_handle(handle)) {
			win32::CloseHandle(handle);
		}
	}
}

auto gse::ide::agent::attach_runtime(session& s) -> void {
	s.log_id = generate_temp_id(hash_combine(stable_id("agent_log"), s.id));
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

auto gse::ide::agent::create_session(data& d, const std::filesystem::path& cwd) -> session& {
	const std::uint32_t session_id = ++d.next_id;
	d.sessions.push_back({
		.id = session_id,
		.name = std::format("Agent {}", session_id),
		.cwd = cwd.empty() ? config::primary().project_root : cwd,
		.hydrated = true,
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

	if (!s.info.agent_id.empty()) {
		append_row(s, {
			.kind = row_kind::note,
			.text = std::format("resuming {}", s.info.agent_id),
		});
	}

	if (!creds.token) {
		s.info.failure = "no CLAUDE_CODE_OAUTH_TOKEN";
		append_row(s, {
			.kind = row_kind::failure,
			.text = "no CLAUDE_CODE_OAUTH_TOKEN in this process or in the user environment",
			.detail = "run `claude setup-token` and start a new session - headless claude cannot use the interactive login",
		});
	}

	return true;
}

auto gse::ide::agent::append_row(session& s, transcript_row row) -> void {
	hydrate_session(s);

	if (row.kind == row_kind::tool && !row.file.empty() && !(row.added.empty() && row.removed.empty())) {
		note_written_file(s, row.file);
	}
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
		if (kind == "result") {
			s.think_clock.reset();
			s.recent_turn.emplace();
		}
		else if (kind == "user") {
			s.action = "thinking";
		}

		const std::string_view uuid = anchorable(*event) ? string_at(*event, "uuid") : std::string_view{};
		std::vector<transcript_row> rows = summarize(*event, s.info);
		for (const transcript_row& row : rows) {
			if (row.kind == row_kind::tool) {
				s.action = tool_action(row);
			}
			else if (row.kind == row_kind::text) {
				s.action = "responding";
			}
		}
		for (transcript_row& row : rows) {
			row.uuid = uuid;
			append_row(s, std::move(row));
		}

		if (kind != "result") {
			continue;
		}
		if (retryable_failure(*event)) {
			arm_retry(s);
		}
		else if (!turn_failed(*event)) {
			s.retry.waiting = false;
			s.retry.attempts = 0;
		}
	}

	if (!open) {
		append_row(s, {
			.kind = row_kind::note,
			.text = "agent exited",
		});
		s.running = false;
		s.think_clock.reset();
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

auto gse::ide::agent::send_to_session(session& s, const std::string_view prompt, const std::span<const attachment> attachments) -> void {
	if (!s.running || !win32::valid_handle(s.input)) {
		return;
	}

	s.info.failure.clear();
	s.retry.waiting = false;
	s.retry.prompt = std::string(prompt);
	s.retry.images.clear();
	for (const attachment& image : attachments) {
		s.retry.images.push_back(image.path);
	}

	append_row(s, {
		.kind = row_kind::user,
		.text = std::string(prompt),
		.detail = attachments.empty()
			? std::string{}
			: std::format("{} image(s) attached", attachments.size()),
	});

	const std::string message = user_message(prompt, attachments);
	win32::DWORD written = 0;
	if (!win32::WriteFile(s.input, message.data(), static_cast<win32::DWORD>(message.size()), &written, nullptr)) {
		append_row(s, {
			.kind = row_kind::failure,
			.text = "failed to send to the agent",
		});
		return;
	}

	s.action = "thinking";
	s.think_clock.emplace();
}

auto gse::ide::agent::interrupt_session(session& s) -> void {
	s.retry.waiting = false;

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
	if (d.sessions.empty()) {
		return nullptr;
	}
	const auto found = std::ranges::find(d.sessions, d.active, &session::id);
	return found != d.sessions.end() ? &*found : &d.sessions.back();
}

auto gse::ide::agent::environment_path(const std::wstring_view name) -> std::filesystem::path {
	const win32::DWORD needed = win32::GetEnvironmentVariableW(name.data(), nullptr, 0);
	if (needed == 0) {
		return {};
	}

	std::wstring value(needed, L'\0');
	const win32::DWORD written = win32::GetEnvironmentVariableW(name.data(), value.data(), needed);
	value.resize(written);
	if (value.empty()) {
		return {};
	}

	return value;
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

export module gse.ide.build:build_runner;

import std;
import gse;
import gse.config;
import gse.ide.config;
import gse.win32;

export namespace gse::ide::build_runner {
	struct output_stream {
		std::mutex mutex;
		std::vector<std::string> lines;
		std::atomic<bool> running = false;
		void* process = nullptr;
	};

	auto in_progress() -> bool;

	auto game_build_generation() -> std::uint32_t;

	auto game_graph_path() -> std::filesystem::path;

	auto take_imported_surface() -> std::optional<gse::attached_surface_message>;

	auto take_new_streams() -> std::vector<std::pair<std::string, std::shared_ptr<output_stream>>>;

	auto start_build_game() -> void;

	auto start_build_and_run_game() -> void;

	auto start_rebuild() -> void;

	auto cleanup_backup() -> void;

	auto attach_process(output_stream& stream, void* process) -> void;

	auto close_process(output_stream& stream) -> void;

	auto terminate_process(output_stream& stream) -> void;
}

namespace gse::ide::build_runner {
	struct runner_state {
		std::mutex mutex;
		std::atomic<std::uint32_t> generation = 0;
		std::atomic<bool> building = false;
		std::filesystem::path graph_path;
		std::optional<gse::attached_surface_message> pending_surface;
		std::vector<std::pair<std::string, std::shared_ptr<output_stream>>> new_streams;
	};

	struct source_fingerprint {
		std::uintmax_t size = 0;
		std::int64_t mtime = 0;

		auto operator==(const source_fingerprint& other) const -> bool = default;
	};

	auto state() -> runner_state&;

	auto emit(const std::shared_ptr<output_stream>& stream, std::string text) -> void;

	auto find_build_dir() -> std::filesystem::path;

	auto project_source_roots(std::string_view target) -> std::vector<std::filesystem::path>;

	auto is_build_source(const std::filesystem::path& path) -> bool;

	auto source_state_path(const std::filesystem::path& build_dir, std::string_view target) -> std::filesystem::path;

	auto load_source_state(const std::filesystem::path& path) -> std::unordered_map<std::string, source_fingerprint>;

	auto save_source_state(const std::filesystem::path& path, const std::unordered_map<std::string, source_fingerprint>& state) -> void;

	auto refresh_changed_sources(const std::shared_ptr<output_stream>& stream, const std::filesystem::path& build_dir, std::string_view target) -> void;

	auto compiler_bin_dir(const std::filesystem::path& build_dir) -> std::filesystem::path;

	auto build_command(const std::filesystem::path& build_dir, std::wstring_view target) -> std::wstring;

	auto backup_path(const std::filesystem::path& executable) -> std::filesystem::path;

	auto current_executable() -> std::filesystem::path;

	auto register_stream(std::string name) -> std::shared_ptr<output_stream>;

	auto run_capture(
		const std::shared_ptr<output_stream>& stream,
		const std::wstring& command_line,
		const std::wstring& working_dir,
		const std::filesystem::path& compiler_bin
	) -> int;

	auto launch_game_attached(const std::shared_ptr<output_stream>& stream, const std::filesystem::path& build_dir) -> bool;

	auto build_game(const std::shared_ptr<output_stream>& stream, bool run) -> bool;

	auto rebuild_editor(const std::shared_ptr<output_stream>& stream) -> void;
}

auto gse::ide::build_runner::state() -> runner_state& {
	static runner_state instance;
	return instance;
}

auto gse::ide::build_runner::emit(const std::shared_ptr<output_stream>& stream, std::string text) -> void {
	std::lock_guard lock(stream->mutex);
	stream->lines.push_back(std::move(text));
}

auto gse::ide::build_runner::find_build_dir() -> std::filesystem::path {
	const std::filesystem::path build_root = gse::config::root_dir / "out" / "build";
	std::error_code ec;
	if (!std::filesystem::is_directory(build_root, ec)) {
		return {};
	}

	std::filesystem::path best;
	std::filesystem::file_time_type best_time{};
	for (const auto& entry : std::filesystem::directory_iterator(build_root, ec)) {
		std::error_code inner;
		if (!entry.is_directory(inner)) {
			continue;
		}
		const std::filesystem::path ninja = entry.path() / "build.ninja";
		if (std::filesystem::exists(ninja, inner)) {
			const std::filesystem::file_time_type time = std::filesystem::last_write_time(ninja, inner);
			if (best.empty() || time > best_time) {
				best = entry.path();
				best_time = time;
			}
		}
	}
	return best;
}

auto gse::ide::build_runner::project_source_roots(const std::string_view target) -> std::vector<std::filesystem::path> {
	if (target == "Editor") {
		return {
			gse::config::source_dir,
			gse::ide::config::source_dir,
		};
	}
	return {
		gse::config::source_dir,
		gse::ide::config::game_source_dir,
	};
}

auto gse::ide::build_runner::is_build_source(const std::filesystem::path& path) -> bool {
	static const std::unordered_set<std::string> extensions{
		".cppm",
		".cpp",
		".ixx",
		".cc",
		".cxx",
		".c",
		".h",
		".hpp",
		".hxx",
		".inl",
	};
	std::string extension = path.extension().generic_native_encoded_string();
	std::ranges::transform(extension, extension.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return extensions.contains(extension);
}

auto gse::ide::build_runner::source_state_path(const std::filesystem::path& build_dir, const std::string_view target) -> std::filesystem::path {
	return build_dir / (".gse_source_state_" + std::string(target) + ".txt");
}

auto gse::ide::build_runner::load_source_state(const std::filesystem::path& path) -> std::unordered_map<std::string, source_fingerprint> {
	std::unordered_map<std::string, source_fingerprint> state;
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		return state;
	}
	std::string line;
	while (std::getline(in, line)) {
		const std::size_t tab = line.find('\t');
		if (tab == std::string::npos) {
			continue;
		}
		std::string key = line.substr(tab + 1);
		if (!key.empty() && key.back() == '\r') {
			key.pop_back();
		}
		source_fingerprint fingerprint;
		const std::from_chars_result size_result = std::from_chars(line.data(), line.data() + tab, fingerprint.size);
		if (size_result.ec != std::errc{}) {
			continue;
		}
		const char* cursor = size_result.ptr;
		if (cursor < line.data() + tab && *cursor == ' ') {
			++cursor;
		}
		const std::from_chars_result mtime_result = std::from_chars(cursor, line.data() + tab, fingerprint.mtime);
		if (mtime_result.ec != std::errc{}) {
			continue;
		}
		state[std::move(key)] = fingerprint;
	}
	return state;
}

auto gse::ide::build_runner::save_source_state(const std::filesystem::path& path, const std::unordered_map<std::string, source_fingerprint>& state) -> void {
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (!out) {
		return;
	}
	for (const auto& [key, fingerprint] : state) {
		std::array<char, 48> digits{};
		const std::to_chars_result size_result = std::to_chars(digits.data(), digits.data() + digits.size(), fingerprint.size);
		if (size_result.ec != std::errc{}) {
			continue;
		}
		*size_result.ptr = ' ';
		const std::to_chars_result mtime_result = std::to_chars(size_result.ptr + 1, digits.data() + digits.size(), fingerprint.mtime);
		if (mtime_result.ec != std::errc{}) {
			continue;
		}
		out.write(digits.data(), mtime_result.ptr - digits.data());
		out.put('\t');
		out.write(key.data(), static_cast<std::streamsize>(key.size()));
		out.put('\n');
	}
}

auto gse::ide::build_runner::refresh_changed_sources(const std::shared_ptr<output_stream>& stream, const std::filesystem::path& build_dir, const std::string_view target) -> void {
	const std::filesystem::path state_path = source_state_path(build_dir, target);
	std::error_code exists_ec;
	const bool seeding = !std::filesystem::exists(state_path, exists_ec);
	const std::unordered_map<std::string, source_fingerprint> previous = load_source_state(state_path);

	std::unordered_map<std::string, source_fingerprint> current;
	std::size_t refreshed = 0;

	for (const std::filesystem::path& source_root : project_source_roots(target)) {
		std::error_code walk_ec;
		if (!std::filesystem::is_directory(source_root, walk_ec)) {
			continue;
		}
		auto iterator = std::filesystem::recursive_directory_iterator(source_root, std::filesystem::directory_options::skip_permission_denied, walk_ec);
		const std::filesystem::recursive_directory_iterator end;
		for (; iterator != end; iterator.increment(walk_ec)) {
			if (walk_ec) {
				break;
			}
			const std::filesystem::directory_entry& entry = *iterator;
			std::error_code entry_ec;
			if (!entry.is_regular_file(entry_ec) || !is_build_source(entry.path())) {
				continue;
			}
			source_fingerprint fingerprint{
				.size = entry.file_size(entry_ec),
				.mtime = static_cast<std::int64_t>(entry.last_write_time(entry_ec).time_since_epoch().count()),
			};
			std::string key = entry.path().generic_native_encoded_string();
			const auto found = previous.find(key);
			if (!seeding && (found == previous.end() || found->second != fingerprint)) {
				std::error_code touch_ec;
				std::filesystem::last_write_time(entry.path(), std::filesystem::file_time_type::clock::now(), touch_ec);
				if (!touch_ec) {
					++refreshed;
					fingerprint.mtime = static_cast<std::int64_t>(std::filesystem::last_write_time(entry.path(), touch_ec).time_since_epoch().count());
				}
			}
			current[std::move(key)] = fingerprint;
		}
	}

	save_source_state(state_path, current);

	if (seeding) {
		emit(stream, "indexed " + std::to_string(current.size()) + " sources for change tracking");
	}
	else if (refreshed > 0) {
		emit(stream, "refreshed " + std::to_string(refreshed) + " changed source(s) before build");
	}
}

auto gse::ide::build_runner::compiler_bin_dir(const std::filesystem::path& build_dir) -> std::filesystem::path {
	std::ifstream cache(build_dir / "CMakeCache.txt");
	if (!cache) {
		return {};
	}

	constexpr std::string_view key = "CMAKE_CXX_COMPILER:";
	std::string line;
	while (std::getline(cache, line)) {
		if (!line.starts_with(key)) {
			continue;
		}
		const std::size_t equals = line.find('=');
		if (equals == std::string::npos) {
			break;
		}
		std::filesystem::path bin = std::filesystem::path(line.substr(equals + 1)).parent_path();
		bin.make_preferred();
		return bin;
	}
	return {};
}

auto gse::ide::build_runner::build_command(const std::filesystem::path& build_dir, const std::wstring_view target) -> std::wstring {
	std::wstring command = L"cmd.exe /c cmake --build \"" + build_dir.wstring() + L"\" --target ";
	command += target;
	return command;
}

auto gse::ide::build_runner::backup_path(const std::filesystem::path& executable) -> std::filesystem::path {
	std::filesystem::path result = executable;
	result += ".bak";
	return result;
}

auto gse::ide::build_runner::current_executable() -> std::filesystem::path {
	std::wstring buffer(gse::win32::max_path, L'\0');
	const gse::win32::DWORD length = gse::win32::GetModuleFileNameW(nullptr, buffer.data(), static_cast<gse::win32::DWORD>(buffer.size()));
	if (length == 0) {
		return {};
	}
	buffer.resize(length);
	return std::filesystem::path(buffer);
}

auto gse::ide::build_runner::register_stream(std::string name) -> std::shared_ptr<output_stream> {
	auto stream = std::make_shared<output_stream>();
	stream->running.store(true, std::memory_order_release);
	auto& s = state();
	std::lock_guard lock(s.mutex);
	s.new_streams.emplace_back(std::move(name), stream);
	return stream;
}

auto gse::ide::build_runner::run_capture(
	const std::shared_ptr<output_stream>& stream,
	const std::wstring& command_line,
	const std::wstring& working_dir,
	const std::filesystem::path& compiler_bin
) -> int {
	gse::win32::SECURITY_ATTRIBUTES attributes{
		.nLength = sizeof(gse::win32::SECURITY_ATTRIBUTES),
		.bInheritHandle = 1,
	};

	void* read_end = nullptr;
	void* write_end = nullptr;
	if (!gse::win32::CreatePipe(&read_end, &write_end, &attributes, 0)) {
		emit(stream, "failed to create output pipe");
		return -1;
	}
	gse::win32::SetHandleInformation(read_end, gse::win32::handle_flag_inherit, 0);

	std::vector<wchar_t> command_buffer(command_line.begin(), command_line.end());
	command_buffer.push_back(0);
	std::vector<wchar_t> environment = gse::win32::environment_with_path_prefix(compiler_bin.wstring());

	gse::win32::STARTUPINFOW startup{
		.cb = sizeof(gse::win32::STARTUPINFOW),
		.dwFlags = gse::win32::startf_use_std_handles,
		.hStdOutput = write_end,
		.hStdError = write_end,
	};

	gse::win32::PROCESS_INFORMATION process{};
	const int spawned = gse::win32::CreateProcessW(
		nullptr,
		command_buffer.data(),
		nullptr,
		nullptr,
		1,
		gse::win32::create_no_window | gse::win32::create_unicode_environment,
		environment.empty() ? nullptr : environment.data(),
		working_dir.empty() ? nullptr : working_dir.c_str(),
		&startup,
		&process
	);

	gse::win32::CloseHandle(write_end);

	if (!spawned) {
		gse::win32::CloseHandle(read_end);
		emit(stream, "failed to launch build process");
		return -1;
	}

	attach_process(*stream, process.hProcess);

	std::string pending;
	std::array<char, 4096> chunk{};
	gse::win32::DWORD received = 0;
	while (gse::win32::ReadFile(read_end, chunk.data(), static_cast<gse::win32::DWORD>(chunk.size()), &received, nullptr) && received > 0) {
		pending.append(chunk.data(), received);
		for (std::size_t newline = pending.find('\n'); newline != std::string::npos; newline = pending.find('\n')) {
			std::string text = pending.substr(0, newline);
			if (!text.empty() && text.back() == '\r') {
				text.pop_back();
			}
			emit(stream, std::move(text));
			pending.erase(0, newline + 1);
		}
	}
	if (!pending.empty()) {
		emit(stream, std::move(pending));
	}

	gse::win32::CloseHandle(read_end);
	gse::win32::WaitForSingleObject(process.hProcess, gse::win32::infinite);
	gse::win32::DWORD code = 0;
	gse::win32::GetExitCodeProcess(process.hProcess, &code);
	{
		std::lock_guard lock(stream->mutex);
		stream->process = nullptr;
	}
	gse::win32::CloseHandle(process.hProcess);
	gse::win32::CloseHandle(process.hThread);
	return static_cast<int>(code);
}

auto gse::ide::build_runner::launch_game_attached(const std::shared_ptr<output_stream>& stream, const std::filesystem::path& build_dir) -> bool {
	const std::filesystem::path game_exe = build_dir / "Game" / "GoonSquad.exe";
	if (!std::filesystem::exists(game_exe)) {
		emit(stream, "game executable not found: " + game_exe.display_string());
		return false;
	}

	const gse::win32::DWORD editor_pid = gse::win32::GetCurrentProcessId();
	const std::uint32_t generation = state().generation.load(std::memory_order_acquire);
	const std::string pipe_name = "\\\\.\\pipe\\gse_editor_" + std::to_string(editor_pid) + "_" + std::to_string(generation);
	const std::wstring wide_pipe(pipe_name.begin(), pipe_name.end());
	const std::filesystem::path graph_file = std::filesystem::temp_directory_path() / "gse_editor_game_graph.bin";
	{
		auto& s = state();
		std::lock_guard lock(s.mutex);
		s.graph_path = graph_file;
	}

	void* pipe = gse::win32::CreateNamedPipeW(wide_pipe.c_str(), gse::win32::pipe_access_inbound, gse::win32::pipe_type_byte | gse::win32::pipe_wait, 1, 0, sizeof(gse::attached_surface_message) * 2, 0, nullptr);
	if (!gse::win32::valid_handle(pipe)) {
		emit(stream, "failed to create editor pipe");
		return false;
	}

	std::wstring command = L"\"" + game_exe.wstring() + L"\"";
	command += L" --engine-attached";
	command += L" --engine-ipc-pipe-name " + wide_pipe;
	command += L" --engine-parent-pid " + std::to_wstring(editor_pid);
	command += L" --engine-dump-system-graph-path \"" + graph_file.wstring() + L"\"";

	std::vector<wchar_t> command_buffer(command.begin(), command.end());
	command_buffer.push_back(0);

	const std::wstring working_dir = gse::config::root_dir.wstring();
	gse::win32::STARTUPINFOW startup{
		.cb = sizeof(gse::win32::STARTUPINFOW),
	};
	gse::win32::PROCESS_INFORMATION process{};
	const int spawned = gse::win32::CreateProcessW(nullptr, command_buffer.data(), nullptr, nullptr, 0, 0, nullptr, working_dir.c_str(), &startup, &process);
	if (!spawned) {
		gse::win32::CloseHandle(pipe);
		emit(stream, "failed to launch game");
		return false;
	}

	emit(stream, "launched game (pid " + std::to_string(process.dwProcessId) + ")");
	attach_process(*stream, process.hProcess);
	gse::win32::CloseHandle(process.hThread);

	std::thread([stream, handle = process.hProcess] {
		gse::win32::WaitForSingleObject(handle, gse::win32::infinite);
		close_process(*stream);
		gse::win32::CloseHandle(handle);
	}).detach();

	std::thread([pipe] {
		const bool connected = gse::win32::ConnectNamedPipe(pipe, nullptr) != 0 || gse::win32::GetLastError() == gse::win32::error_pipe_connected;
		if (connected) {
			gse::attached_surface_message message{};
			gse::win32::DWORD read = 0;
			const bool received = gse::win32::ReadFile(pipe, &message, sizeof(message), &read, nullptr) != 0;
			if (received && read == sizeof(message) && message.magic == gse::attached_surface_magic) {
				void* game = gse::win32::OpenProcess(gse::win32::process_dup_handle, 0, message.pid);
				if (gse::win32::valid_handle(game)) {
					for (std::size_t i = 0; i < gse::attached_ring_size; ++i) {
						void* duplicate = nullptr;
						gse::win32::DuplicateHandle(game, message.surface_handles[i], gse::win32::GetCurrentProcess(), &duplicate, 0, 0, gse::win32::duplicate_same_access);
						message.surface_handles[i] = duplicate;
					}
					void* duplicate_semaphore = nullptr;
					gse::win32::DuplicateHandle(game, message.semaphore_handle, gse::win32::GetCurrentProcess(), &duplicate_semaphore, 0, 0, gse::win32::duplicate_same_access);
					message.semaphore_handle = duplicate_semaphore;
					gse::win32::CloseHandle(game);

					auto& s = state();
					std::lock_guard lock(s.mutex);
					s.pending_surface = message;
				}
			}
		}
		gse::win32::DisconnectNamedPipe(pipe);
		gse::win32::CloseHandle(pipe);
	}).detach();

	return true;
}

auto gse::ide::build_runner::build_game(const std::shared_ptr<output_stream>& stream, const bool run) -> bool {
	const std::filesystem::path build_dir = find_build_dir();
	if (build_dir.empty()) {
		emit(stream, "no configured build directory found under out/build");
		return false;
	}

	refresh_changed_sources(stream, build_dir, "GoonSquad");

	const std::filesystem::path game_exe = build_dir / "Game" / "GoonSquad.exe";
	const std::filesystem::path backup = backup_path(game_exe);
	std::error_code ec;
	if (std::filesystem::exists(game_exe, ec)) {
		std::filesystem::remove(backup, ec);
		std::filesystem::rename(game_exe, backup, ec);
	}

	emit(stream, "building GoonSquad...");
	const std::wstring command = build_command(build_dir, L"GoonSquad");
	const int code = run_capture(stream, command, gse::config::root_dir.wstring(), compiler_bin_dir(build_dir));
	if (code != 0) {
		emit(stream, "build failed (exit " + std::to_string(code) + ")");
		if (!std::filesystem::exists(game_exe, ec) && std::filesystem::exists(backup, ec)) {
			std::filesystem::rename(backup, game_exe, ec);
		}
		return false;
	}

	emit(stream, "build succeeded");
	std::filesystem::remove(backup, ec);
	state().generation.fetch_add(1, std::memory_order_acq_rel);

	if (run) {
		return launch_game_attached(stream, build_dir);
	}
	return false;
}

auto gse::ide::build_runner::rebuild_editor(const std::shared_ptr<output_stream>& stream) -> void {
	const std::filesystem::path build_dir = find_build_dir();
	if (build_dir.empty()) {
		emit(stream, "no configured build directory found under out/build");
		return;
	}

	refresh_changed_sources(stream, build_dir, "Editor");

	const std::filesystem::path editor_exe = current_executable();
	if (editor_exe.empty()) {
		emit(stream, "could not resolve editor executable path");
		return;
	}

	const std::filesystem::path backup = backup_path(editor_exe);
	std::error_code ec;
	std::filesystem::remove(backup, ec);
	std::filesystem::rename(editor_exe, backup, ec);
	if (ec) {
		emit(stream, "could not back up running editor; aborting rebuild");
		return;
	}

	emit(stream, "rebuilding editor...");
	const std::wstring command = build_command(build_dir, L"Editor");
	const int code = run_capture(stream, command, gse::config::root_dir.wstring(), compiler_bin_dir(build_dir));
	if (code != 0) {
		emit(stream, "rebuild failed (exit " + std::to_string(code) + "); restoring previous editor");
		std::filesystem::rename(backup, editor_exe, ec);
		return;
	}

	emit(stream, "rebuild succeeded; relaunching editor");
	std::wstring relaunch = L"\"" + editor_exe.wstring() + L"\"";
	std::vector<wchar_t> command_buffer(relaunch.begin(), relaunch.end());
	command_buffer.push_back(0);
	const std::wstring working_dir = gse::config::root_dir.wstring();
	gse::win32::STARTUPINFOW startup{
		.cb = sizeof(gse::win32::STARTUPINFOW),
	};
	gse::win32::PROCESS_INFORMATION process{};
	if (gse::win32::CreateProcessW(nullptr, command_buffer.data(), nullptr, nullptr, 0, 0, nullptr, working_dir.c_str(), &startup, &process)) {
		gse::win32::CloseHandle(process.hProcess);
		gse::win32::CloseHandle(process.hThread);
	}
	gse::shutdown();
}

auto gse::ide::build_runner::in_progress() -> bool {
	return state().building.load(std::memory_order_acquire);
}

auto gse::ide::build_runner::game_build_generation() -> std::uint32_t {
	return state().generation.load(std::memory_order_acquire);
}

auto gse::ide::build_runner::game_graph_path() -> std::filesystem::path {
	auto& s = state();
	std::lock_guard lock(s.mutex);
	return s.graph_path;
}

auto gse::ide::build_runner::take_imported_surface() -> std::optional<gse::attached_surface_message> {
	auto& s = state();
	std::lock_guard lock(s.mutex);
	if (!s.pending_surface) {
		return std::nullopt;
	}
	std::optional<gse::attached_surface_message> result = s.pending_surface;
	s.pending_surface.reset();
	return result;
}

auto gse::ide::build_runner::take_new_streams() -> std::vector<std::pair<std::string, std::shared_ptr<output_stream>>> {
	auto& s = state();
	std::lock_guard lock(s.mutex);
	return std::exchange(s.new_streams, {});
}

auto gse::ide::build_runner::start_build_game() -> void {
	auto& s = state();
	if (s.building.exchange(true, std::memory_order_acq_rel)) {
		return;
	}
	auto stream = register_stream("Build Game");
	std::thread([stream] {
		build_game(stream, false);
		close_process(*stream);
		state().building.store(false, std::memory_order_release);
	}).detach();
}

auto gse::ide::build_runner::start_build_and_run_game() -> void {
	auto& s = state();
	if (s.building.exchange(true, std::memory_order_acq_rel)) {
		return;
	}
	auto stream = register_stream("Build & Run");
	std::thread([stream] {
		const bool live = build_game(stream, true);
		if (!live) {
			close_process(*stream);
		}
		state().building.store(false, std::memory_order_release);
	}).detach();
}

auto gse::ide::build_runner::start_rebuild() -> void {
	auto& s = state();
	if (s.building.exchange(true, std::memory_order_acq_rel)) {
		return;
	}
	auto stream = register_stream("Rebuild Editor");
	std::thread([stream] {
		rebuild_editor(stream);
		close_process(*stream);
		state().building.store(false, std::memory_order_release);
	}).detach();
}

auto gse::ide::build_runner::cleanup_backup() -> void {
	std::error_code ec;
	const std::filesystem::path build_dir = find_build_dir();
	if (!build_dir.empty()) {
		std::filesystem::remove(backup_path(build_dir / "Editor" / "Editor.exe"), ec);
		std::filesystem::remove(backup_path(build_dir / "Game" / "GoonSquad.exe"), ec);
	}
	const std::filesystem::path editor_exe = current_executable();
	if (!editor_exe.empty()) {
		std::filesystem::remove(backup_path(editor_exe), ec);
	}
}

auto gse::ide::build_runner::attach_process(output_stream& stream, void* process) -> void {
	std::lock_guard lock(stream.mutex);
	stream.process = process;
}

auto gse::ide::build_runner::close_process(output_stream& stream) -> void {
	std::lock_guard lock(stream.mutex);
	stream.process = nullptr;
	stream.running.store(false, std::memory_order_release);
}

auto gse::ide::build_runner::terminate_process(output_stream& stream) -> void {
	std::lock_guard lock(stream.mutex);
	if (gse::win32::valid_handle(stream.process)) {
		gse::win32::TerminateProcess(stream.process, 1);
	}
}

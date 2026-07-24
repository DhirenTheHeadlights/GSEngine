export module gse.ide.build:build_runner;

import std;
import gse;
import gse.config;
import gse.ide.config;
import gse.win32;

import :spawn;

export namespace gse::ide::build_runner {
	enum class build_target : std::uint8_t {
		game,
		editor,
	};

	struct build_request {
		build_target target = build_target::game;
		bool run_after = false;
	};

	struct stream_opened {
		std::string name;
		std::shared_ptr<spawn::output_stream> stream;
	};

	struct attached_surface_ready {
		attached_surface_message message;
	};

	struct build_completion {
		std::mutex mutex;
		bool done = false;
		bool game_launched = false;
		std::uint32_t generation = 0;
		void* game_process = nullptr;
		void* surface_pipe = nullptr;
		std::filesystem::path graph_path;
	};

	struct attached_game {
		void* process = nullptr;
		std::shared_ptr<spawn::output_stream> stream;
		bool owns_pipe = false;
	};

	struct surface_pipe {
		void* handle = nullptr;
		bool connected = false;
		std::size_t received = 0;
		attached_surface_message message{};
	};

	struct [[= gse::system_state<"Build Runner">{}]] data {
		[[= gse::shared]] bool building = false;
		[[= gse::shared]] std::uint32_t game_generation = 0;
		[[= gse::shared]] std::filesystem::path game_graph_path;
		build_completion completion;
		std::shared_ptr<spawn::output_stream> active_stream;
		std::jthread worker;
		std::vector<attached_game> games;
		surface_pipe pipe;
	};

	[[= gse::system_init{}]]
	auto init(data& d) -> async::task<>;

	[[= gse::system_run<>{}]]
	auto run(context& ctx, data& d) -> async::task<>;

	[[= gse::system_shutdown{}]]
	auto shutdown(data& d) -> void;
}

namespace gse::ide::build_runner {
	constexpr std::uint32_t source_state_magic = 0x47534253;
	constexpr std::uint32_t source_state_version = 1;

	constexpr std::array<std::string_view, 10> build_source_extensions = {
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

	constexpr std::string_view module_write_signature = "failed to write compiled module";
	constexpr std::string_view module_read_signature = "failed to read compiled module";
	constexpr std::string_view file_exists_signature = "File exists";

	struct source_fingerprint {
		std::uintmax_t size = 0;
		std::int64_t mtime = 0;

		auto operator==(const source_fingerprint& other) const -> bool = default;
	};

	auto find_build_dir() -> std::filesystem::path;

	auto project_source_roots(std::string_view target) -> std::vector<std::filesystem::path>;

	auto is_build_source(const std::filesystem::path& path) -> bool;

	auto source_state_path(const std::filesystem::path& build_dir, std::string_view target) -> std::filesystem::path;

	auto load_source_state(const std::filesystem::path& path) -> std::unordered_map<std::string, source_fingerprint>;

	auto save_source_state(const std::filesystem::path& path, const std::unordered_map<std::string, source_fingerprint>& state) -> void;

	auto refresh_changed_sources(spawn::output_stream& stream, const std::filesystem::path& build_dir, std::string_view target) -> void;

	auto compiler_bin_dir(const std::filesystem::path& build_dir) -> std::filesystem::path;

	auto build_command(const std::filesystem::path& build_dir, std::string_view target) -> std::wstring;

	auto backup_path(const std::filesystem::path& executable) -> std::filesystem::path;

	auto current_executable() -> std::filesystem::path;

	auto collect_module_write_conflicts(
		spawn::output_stream& stream,
		std::size_t from_line,
		const std::filesystem::path& build_dir
	) -> std::vector<std::filesystem::path>;

	auto run_build_with_module_recovery(
		const std::stop_token& st,
		spawn::output_stream& stream,
		const std::wstring& command,
		const std::filesystem::path& build_dir,
		const std::filesystem::path& compiler_bin
	) -> int;

	auto launch_game_attached(build_completion& completion, spawn::output_stream& stream, std::uint32_t generation) -> void;

	auto build_game(
		const std::stop_token& st,
		build_completion& completion,
		spawn::output_stream& stream,
		bool run_after,
		std::uint32_t next_generation
	) -> void;

	auto rebuild_editor(const std::stop_token& st, spawn::output_stream& stream) -> void;

	auto build_worker(
		const std::stop_token& st,
		build_completion* completion,
		std::shared_ptr<spawn::output_stream> stream,
		build_request request,
		std::uint32_t next_generation
	) -> void;

	auto cleanup_backups() -> void;

	auto start_build(context& ctx, data& d, const build_request& request) -> void;

	auto drain_completion(data& d) -> void;

	auto poll_games(data& d) -> void;

	auto close_surface_pipe(data& d) -> void;

	auto import_surface_handles(attached_surface_message& message) -> bool;

	auto poll_surface_pipe(context& ctx, data& d) -> void;
}

auto gse::ide::build_runner::find_build_dir() -> std::filesystem::path {
	std::error_code ec;
	if (!std::filesystem::exists(config::build_dir / "build.ninja", ec)) {
		return {};
	}
	return config::build_dir;
}

auto gse::ide::build_runner::project_source_roots(const std::string_view target) -> std::vector<std::filesystem::path> {
	if (target == config::editor_target) {
		return {
			gse::config::source_dir,
			config::source_dir,
		};
	}
	return {
		gse::config::source_dir,
		config::game_source_dir,
	};
}

auto gse::ide::build_runner::is_build_source(const std::filesystem::path& path) -> bool {
	std::string extension = path.extension().generic_native_encoded_string();
	std::ranges::transform(extension, extension.begin(), [](const unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return std::ranges::contains(build_source_extensions, extension);
}

auto gse::ide::build_runner::source_state_path(const std::filesystem::path& build_dir, const std::string_view target) -> std::filesystem::path {
	return build_dir / (".gse_source_state_" + std::string(target) + ".bin");
}

auto gse::ide::build_runner::load_source_state(const std::filesystem::path& path) -> std::unordered_map<std::string, source_fingerprint> {
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		return {};
	}
	binary_reader reader(in);
	std::uint32_t magic = 0;
	std::uint32_t version = 0;
	reader & magic & version;
	if (!in || magic != source_state_magic || version != source_state_version) {
		return {};
	}
	std::unordered_map<std::string, source_fingerprint> state;
	reader & state;
	if (!in) {
		return {};
	}
	return state;
}

auto gse::ide::build_runner::save_source_state(const std::filesystem::path& path, const std::unordered_map<std::string, source_fingerprint>& state) -> void {
	std::filesystem::path temp = path;
	temp += ".tmp";
	{
		std::ofstream out(temp, std::ios::binary | std::ios::trunc);
		if (!out) {
			return;
		}
		binary_writer writer(out, source_state_magic, source_state_version);
		writer & state;
	}
	std::error_code ec;
	std::filesystem::rename(temp, path, ec);
}

auto gse::ide::build_runner::refresh_changed_sources(spawn::output_stream& stream, const std::filesystem::path& build_dir, const std::string_view target) -> void {
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
			if (!entry.is_regular_file(entry_ec) || entry_ec || !is_build_source(entry.path())) {
				continue;
			}
			std::error_code size_ec;
			std::error_code time_ec;
			const std::uintmax_t size = entry.file_size(size_ec);
			const std::filesystem::file_time_type mtime = entry.last_write_time(time_ec);
			if (size_ec || time_ec) {
				continue;
			}
			source_fingerprint fingerprint{
				.size = size,
				.mtime = static_cast<std::int64_t>(mtime.time_since_epoch().count()),
			};
			std::string key = entry.path().generic_native_encoded_string();
			const auto found = previous.find(key);
			if (!seeding && (found == previous.end() || found->second != fingerprint)) {
				std::error_code touch_ec;
				std::filesystem::last_write_time(entry.path(), std::filesystem::file_time_type::clock::now(), touch_ec);
				if (!touch_ec) {
					++refreshed;
					std::error_code read_ec;
					const std::filesystem::file_time_type touched = std::filesystem::last_write_time(entry.path(), read_ec);
					if (!read_ec) {
						fingerprint.mtime = static_cast<std::int64_t>(touched.time_since_epoch().count());
					}
				}
			}
			current[std::move(key)] = fingerprint;
		}
	}

	save_source_state(state_path, current);

	if (seeding) {
		spawn::emit(stream, "indexed " + std::to_string(current.size()) + " sources for change tracking");
	}
	else if (refreshed > 0) {
		spawn::emit(stream, "refreshed " + std::to_string(refreshed) + " changed source(s) before build");
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

auto gse::ide::build_runner::build_command(const std::filesystem::path& build_dir, const std::string_view target) -> std::wstring {
	std::wstring command = L"cmd.exe /c cmake --build \"" + build_dir.wstring() + L"\" --target ";
	command += std::wstring(target.begin(), target.end());
	return command;
}

auto gse::ide::build_runner::backup_path(const std::filesystem::path& executable) -> std::filesystem::path {
	std::filesystem::path result = executable;
	result += ".bak";
	return result;
}

auto gse::ide::build_runner::current_executable() -> std::filesystem::path {
	std::wstring buffer(win32::max_path, L'\0');
	for (;;) {
		const win32::DWORD length = win32::GetModuleFileNameW(nullptr, buffer.data(), static_cast<win32::DWORD>(buffer.size()));
		if (length == 0) {
			return {};
		}
		if (length < buffer.size()) {
			buffer.resize(length);
			return std::filesystem::path(buffer);
		}
		buffer.resize(buffer.size() * 2);
	}
}

auto gse::ide::build_runner::collect_module_write_conflicts(
	spawn::output_stream& stream,
	const std::size_t from_line,
	const std::filesystem::path& build_dir
) -> std::vector<std::filesystem::path> {
	std::vector<std::string> lines;
	{
		std::lock_guard lock(stream.mutex);
		if (from_line < stream.lines.size()) {
			lines.assign(stream.lines.begin() + static_cast<std::ptrdiff_t>(from_line), stream.lines.end());
		}
	}

	const bool write_failed = std::ranges::any_of(lines, [](const std::string& line) {
		return line.find(module_write_signature) != std::string::npos;
	});
	const bool file_exists = std::ranges::any_of(lines, [](const std::string& line) {
		return line.find(file_exists_signature) != std::string::npos;
	});
	const bool read_failed = std::ranges::any_of(lines, [](const std::string& line) {
		return line.find(module_read_signature) != std::string::npos;
	});
	if (!(write_failed && file_exists) && !read_failed) {
		return {};
	}

	constexpr std::string_view failed_marker = "FAILED:";
	constexpr std::string_view note_marker = "compiled module file is '";

	std::vector<std::filesystem::path> conflicts;
	std::unordered_set<std::string> seen;
	const std::filesystem::path normalized_build_dir = std::filesystem::absolute(build_dir).lexically_normal();
	for (const std::string& line : lines) {
		std::vector<std::string> candidates;
		if (line.starts_with(failed_marker)) {
			std::istringstream tokens(line.substr(failed_marker.size()));
			std::string token;
			while (tokens >> token) {
				if (!token.starts_with('[')) {
					candidates.push_back(token);
				}
			}
		}
		else {
			const std::size_t marker = line.find(note_marker);
			if (marker != std::string::npos) {
				const std::size_t begin = marker + note_marker.size();
				const std::size_t end = line.find('\'', begin);
				if (end != std::string::npos) {
					candidates.push_back(line.substr(begin, end - begin));
				}
			}
		}

		for (std::string& candidate : candidates) {
			std::filesystem::path gcm = std::move(candidate);
			if (gcm.extension() != ".gcm") {
				continue;
			}
			if (gcm.is_relative()) {
				gcm = normalized_build_dir / gcm;
			}
			gcm = std::filesystem::absolute(gcm).lexically_normal();
			const std::filesystem::path relative = gcm.lexically_relative(normalized_build_dir);
			if (relative.empty() || relative.is_absolute() || *relative.begin() == "..") {
				continue;
			}
			if (seen.insert(gcm.generic_native_encoded_string()).second) {
				conflicts.push_back(std::move(gcm));
			}
		}
	}
	return conflicts;
}

auto gse::ide::build_runner::run_build_with_module_recovery(
	const std::stop_token& st,
	spawn::output_stream& stream,
	const std::wstring& command,
	const std::filesystem::path& build_dir,
	const std::filesystem::path& compiler_bin
) -> int {
	constexpr int max_attempts = 16;
	std::unordered_set<std::string> recovered;
	for (int attempt = 1; ; ++attempt) {
		std::size_t start_line = 0;
		{
			std::lock_guard lock(stream.mutex);
			start_line = stream.lines.size();
		}

		const int code = spawn::run_capture(stream, command, gse::config::root_dir.wstring(), compiler_bin);
		if (code == 0 || attempt >= max_attempts || st.stop_requested() || stream.terminated.load(std::memory_order_acquire)) {
			return code;
		}

		const std::vector<std::filesystem::path> conflicts = collect_module_write_conflicts(stream, start_line, build_dir);
		if (conflicts.empty()) {
			return code;
		}

		std::size_t cleared = 0;
		for (const std::filesystem::path& gcm : conflicts) {
			if (!recovered.insert(gcm.generic_native_encoded_string()).second) {
				continue;
			}
			std::error_code ec;
			if (std::filesystem::remove(gcm, ec)) {
				++cleared;
			}
			else if (ec) {
				spawn::emit(stream, "could not clear stale module cache file " + gcm.generic_display_string() + ": " + ec.message());
			}
		}
		if (cleared == 0) {
			return code;
		}

		spawn::emit(stream, "cleared " + std::to_string(cleared) + " stale module cache file(s); retrying build");
	}
}

auto gse::ide::build_runner::launch_game_attached(build_completion& completion, spawn::output_stream& stream, const std::uint32_t generation) -> void {
	const std::filesystem::path& game_exe = config::game_executable;
	std::error_code ec;
	if (!std::filesystem::exists(game_exe, ec)) {
		spawn::emit(stream, "game executable not found: " + game_exe.display_string());
		return;
	}

	const win32::DWORD editor_pid = win32::GetCurrentProcessId();
	const std::string pipe_name = "\\\\.\\pipe\\gse_editor_" + std::to_string(editor_pid) + "_" + std::to_string(generation);
	const std::wstring wide_pipe(pipe_name.begin(), pipe_name.end());
	const std::filesystem::path graph_file = std::filesystem::temp_directory_path() / std::format("gse_editor_game_graph_{}_{}.bin", editor_pid, generation);

	void* pipe = win32::CreateNamedPipeW(wide_pipe.c_str(), win32::pipe_access_inbound, win32::pipe_type_byte | win32::pipe_nowait, 1, 0, sizeof(attached_surface_message) * 2, 0, nullptr);
	if (!win32::valid_handle(pipe)) {
		spawn::emit(stream, "failed to create editor pipe");
		return;
	}

	std::wstring command = L"\"" + game_exe.wstring() + L"\"";
	command += L" --engine-attached";
	command += L" --engine-ipc-pipe-name " + wide_pipe;
	command += L" --engine-parent-pid " + std::to_wstring(editor_pid);
	command += L" --engine-dump-system-graph-path \"" + graph_file.wstring() + L"\"";

	std::vector<wchar_t> command_buffer(command.begin(), command.end());
	command_buffer.push_back(0);

	const std::wstring working_dir = gse::config::root_dir.wstring();
	win32::STARTUPINFOW startup{
		.cb = sizeof(win32::STARTUPINFOW),
	};
	win32::PROCESS_INFORMATION process{};
	if (!win32::CreateProcessW(nullptr, command_buffer.data(), nullptr, nullptr, 0, 0, nullptr, working_dir.c_str(), &startup, &process)) {
		win32::CloseHandle(pipe);
		spawn::emit(stream, "failed to launch game");
		return;
	}

	spawn::emit(stream, "launched game (pid " + std::to_string(process.dwProcessId) + ")");
	spawn::attach_process(stream, process.hProcess, nullptr);
	win32::CloseHandle(process.hThread);

	std::lock_guard lock(completion.mutex);
	completion.game_launched = true;
	completion.game_process = process.hProcess;
	completion.surface_pipe = pipe;
	completion.graph_path = graph_file;
}

auto gse::ide::build_runner::build_game(
	const std::stop_token& st,
	build_completion& completion,
	spawn::output_stream& stream,
	const bool run_after,
	const std::uint32_t next_generation
) -> void {
	const std::filesystem::path build_dir = find_build_dir();
	if (build_dir.empty()) {
		spawn::emit(stream, "configured build directory is unavailable");
		return;
	}

	refresh_changed_sources(stream, build_dir, config::game_target);
	const std::filesystem::path compiler_bin = compiler_bin_dir(build_dir);

	const std::filesystem::path& game_exe = config::game_executable;
	const std::filesystem::path backup = backup_path(game_exe);
	std::error_code ec;
	if (std::filesystem::exists(game_exe, ec)) {
		std::filesystem::remove(backup, ec);
		std::filesystem::rename(game_exe, backup, ec);
		if (ec) {
			spawn::emit(stream, "could not move aside " + game_exe.filename().display_string() + "; aborting build");
			return;
		}
	}

	spawn::emit(stream, "building " + std::string(config::game_target) + "...");
	const int code = run_build_with_module_recovery(st, stream, build_command(build_dir, config::game_target), build_dir, compiler_bin);
	if (code != 0) {
		spawn::emit(stream, "build failed (exit " + std::to_string(code) + ")");
		if (std::filesystem::exists(backup, ec)) {
			std::filesystem::remove(game_exe, ec);
			std::filesystem::rename(backup, game_exe, ec);
		}
		return;
	}

	spawn::emit(stream, "build succeeded");
	std::filesystem::remove(backup, ec);
	{
		std::lock_guard lock(completion.mutex);
		completion.generation = next_generation;
	}

	if (run_after && !st.stop_requested()) {
		launch_game_attached(completion, stream, next_generation);
	}
}

auto gse::ide::build_runner::rebuild_editor(const std::stop_token& st, spawn::output_stream& stream) -> void {
	const std::filesystem::path build_dir = find_build_dir();
	if (build_dir.empty()) {
		spawn::emit(stream, "configured build directory is unavailable");
		return;
	}

	refresh_changed_sources(stream, build_dir, config::editor_target);
	const std::filesystem::path compiler_bin = compiler_bin_dir(build_dir);

	const std::filesystem::path editor_exe = current_executable();
	if (editor_exe.empty()) {
		spawn::emit(stream, "could not resolve editor executable path");
		return;
	}

	const std::filesystem::path backup = backup_path(editor_exe);
	std::error_code ec;
	std::filesystem::remove(backup, ec);
	std::filesystem::rename(editor_exe, backup, ec);
	if (ec) {
		spawn::emit(stream, "could not back up running editor; aborting rebuild");
		return;
	}

	spawn::emit(stream, "rebuilding editor...");
	const int code = run_build_with_module_recovery(st, stream, build_command(build_dir, config::editor_target), build_dir, compiler_bin);
	if (code != 0) {
		spawn::emit(stream, "rebuild failed (exit " + std::to_string(code) + "); restoring previous editor");
		std::filesystem::remove(editor_exe, ec);
		std::filesystem::rename(backup, editor_exe, ec);
		return;
	}

	if (st.stop_requested()) {
		return;
	}

	spawn::emit(stream, "rebuild succeeded; relaunching editor");
	gse::app::relaunch_on_exit(editor_exe, gse::config::root_dir);
	gse::shutdown();
}

auto gse::ide::build_runner::build_worker(
	const std::stop_token& st,
	build_completion* completion,
	const std::shared_ptr<spawn::output_stream> stream,
	const build_request request,
	const std::uint32_t next_generation
) -> void {
	if (request.target == build_target::editor) {
		rebuild_editor(st, *stream);
	}
	else {
		build_game(st, *completion, *stream, request.run_after, next_generation);
	}

	bool launched = false;
	{
		std::lock_guard lock(completion->mutex);
		launched = completion->game_launched;
	}
	if (!launched) {
		spawn::close_process(*stream);
	}

	std::lock_guard lock(completion->mutex);
	completion->done = true;
}

auto gse::ide::build_runner::cleanup_backups() -> void {
	std::error_code ec;
	std::filesystem::remove(backup_path(config::game_executable), ec);
	std::filesystem::remove(backup_path(config::editor_executable), ec);
	const std::filesystem::path editor_exe = current_executable();
	if (!editor_exe.empty()) {
		std::filesystem::remove(backup_path(editor_exe), ec);
	}
}

auto gse::ide::build_runner::start_build(context& ctx, data& d, const build_request& request) -> void {
	if (d.building) {
		return;
	}

	const std::string_view name = request.target == build_target::editor
		? "Rebuild Editor"
		: request.run_after ? "Build & Run" : "Build Game";
	auto stream = std::make_shared<spawn::output_stream>();
	stream->running.store(true, std::memory_order_release);
	ctx.channels.push<stream_opened>({
		.name = std::string(name),
		.stream = stream,
	});

	{
		std::lock_guard lock(d.completion.mutex);
		d.completion.done = false;
		d.completion.game_launched = false;
		d.completion.generation = 0;
		d.completion.game_process = nullptr;
		d.completion.surface_pipe = nullptr;
		d.completion.graph_path.clear();
	}

	d.building = true;
	d.active_stream = stream;
	d.worker = std::jthread(build_worker, &d.completion, std::move(stream), request, d.game_generation + 1);
}

auto gse::ide::build_runner::drain_completion(data& d) -> void {
	if (!d.building) {
		return;
	}
	{
		std::lock_guard lock(d.completion.mutex);
		if (!d.completion.done) {
			return;
		}
	}
	if (d.worker.joinable()) {
		d.worker.join();
	}
	d.building = false;

	if (d.completion.generation != 0) {
		d.game_generation = d.completion.generation;
	}
	if (d.completion.game_launched) {
		close_surface_pipe(d);
		d.pipe.handle = d.completion.surface_pipe;
		d.game_graph_path = std::move(d.completion.graph_path);
		for (attached_game& game : d.games) {
			game.owns_pipe = false;
		}
		d.games.push_back({
			.process = d.completion.game_process,
			.stream = d.active_stream,
			.owns_pipe = true,
		});
	}
	d.active_stream.reset();
}

auto gse::ide::build_runner::poll_games(data& d) -> void {
	for (std::size_t i = 0; i < d.games.size();) {
		attached_game& game = d.games[i];
		if (win32::WaitForSingleObject(game.process, 0) != win32::wait_object_0) {
			++i;
			continue;
		}
		spawn::close_process(*game.stream);
		win32::CloseHandle(game.process);
		if (game.owns_pipe && !d.pipe.connected) {
			close_surface_pipe(d);
		}
		d.games.erase(d.games.begin() + static_cast<std::ptrdiff_t>(i));
	}
}

auto gse::ide::build_runner::close_surface_pipe(data& d) -> void {
	if (!d.pipe.handle) {
		return;
	}
	win32::DisconnectNamedPipe(d.pipe.handle);
	win32::CloseHandle(d.pipe.handle);
	d.pipe.handle = nullptr;
	d.pipe.connected = false;
	d.pipe.received = 0;
}

auto gse::ide::build_runner::import_surface_handles(attached_surface_message& message) -> bool {
	void* game = win32::OpenProcess(win32::process_dup_handle, 0, message.pid);
	if (!win32::valid_handle(game)) {
		return false;
	}

	std::array<void*, attached_ring_size> surfaces{};
	void* semaphore = nullptr;
	bool ok = true;
	for (std::size_t i = 0; i < attached_ring_size; ++i) {
		if (!win32::DuplicateHandle(game, message.surface_handles[i], win32::GetCurrentProcess(), &surfaces[i], 0, 0, win32::duplicate_same_access)) {
			ok = false;
		}
	}
	if (ok && !win32::DuplicateHandle(game, message.semaphore_handle, win32::GetCurrentProcess(), &semaphore, 0, 0, win32::duplicate_same_access)) {
		ok = false;
	}
	win32::CloseHandle(game);

	if (!ok) {
		for (void* surface : surfaces) {
			if (win32::valid_handle(surface)) {
				win32::CloseHandle(surface);
			}
		}
		if (win32::valid_handle(semaphore)) {
			win32::CloseHandle(semaphore);
		}
		return false;
	}

	for (std::size_t i = 0; i < attached_ring_size; ++i) {
		message.surface_handles[i] = surfaces[i];
	}
	message.semaphore_handle = semaphore;
	return true;
}

auto gse::ide::build_runner::poll_surface_pipe(context& ctx, data& d) -> void {
	if (!d.pipe.handle) {
		return;
	}

	if (!d.pipe.connected) {
		if (win32::ConnectNamedPipe(d.pipe.handle, nullptr) != 0) {
			d.pipe.connected = true;
		}
		else {
			const win32::DWORD error = win32::GetLastError();
			if (error == win32::error_pipe_connected) {
				d.pipe.connected = true;
			}
			else if (error != win32::error_pipe_listening) {
				close_surface_pipe(d);
				return;
			}
		}
		if (!d.pipe.connected) {
			return;
		}
	}

	win32::DWORD available = 0;
	if (!win32::PeekNamedPipe(d.pipe.handle, nullptr, 0, nullptr, &available, nullptr)) {
		close_surface_pipe(d);
		return;
	}
	if (available == 0) {
		return;
	}

	auto* bytes = reinterpret_cast<char*>(&d.pipe.message);
	win32::DWORD read = 0;
	if (!win32::ReadFile(d.pipe.handle, bytes + d.pipe.received, static_cast<win32::DWORD>(sizeof(attached_surface_message) - d.pipe.received), &read, nullptr)) {
		if (win32::GetLastError() != win32::error_no_data) {
			close_surface_pipe(d);
		}
		return;
	}
	d.pipe.received += read;
	if (d.pipe.received < sizeof(attached_surface_message)) {
		return;
	}

	if (d.pipe.message.magic == attached_surface_magic && import_surface_handles(d.pipe.message)) {
		ctx.channels.push<attached_surface_ready>({
			.message = d.pipe.message,
		});
	}
	close_surface_pipe(d);
}

auto gse::ide::build_runner::init(data&) -> async::task<> {
	cleanup_backups();
	return {};
}

auto gse::ide::build_runner::run(context& ctx, data& d) -> async::task<> {
	for (const build_request& request : ctx.read_channel<build_request>()) {
		start_build(ctx, d, request);
	}
	drain_completion(d);
	poll_games(d);
	poll_surface_pipe(ctx, d);
	return {};
}

auto gse::ide::build_runner::shutdown(data& d) -> void {
	d.worker.request_stop();
	if (d.active_stream) {
		spawn::terminate_process(*d.active_stream);
	}
	if (d.worker.joinable()) {
		d.worker.join();
	}
	close_surface_pipe(d);
	for (const attached_game& game : d.games) {
		spawn::close_process(*game.stream);
		win32::CloseHandle(game.process);
	}
	d.games.clear();
}

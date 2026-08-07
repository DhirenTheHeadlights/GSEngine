export module gse.ide.build:build_runner;

import std;
import gse;
import gse.config;
import gse.gpu;
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

	struct build_finished {};

	struct stream_opened {
		std::string name;
		std::shared_ptr<spawn::output_stream> stream;
	};

	struct attached_surface_ready {
		std::uint32_t generation = 0;
		std::shared_ptr<const attached_surface_message> message;
	};

	struct attached_surface_imported {
		std::uint32_t generation = 0;
	};

	struct attached_surface_rejected {
		std::uint32_t generation = 0;
	};

	struct attached_session_ended {
		std::uint32_t generation = 0;
	};

	struct attached_input {
		input::event event;
	};

	enum class attached_session_status : std::uint8_t {
		awaiting_surface,
		active,
	};

	struct attached_session {
		std::uint32_t generation = 0;
		attached_session_status status = attached_session_status::awaiting_surface;
	};

	struct build_completion {
		std::mutex mutex;
		bool done = false;
		bool game_launched = false;
		std::uint32_t generation = 0;
		void* game_process = nullptr;
		void* game_job = nullptr;
		void* game_output = nullptr;
		std::uint32_t game_pid = 0;
		void* surface_pipe = nullptr;
		std::filesystem::path graph_path;
	};

	struct attached_game {
		std::uint32_t generation = 0;
		void* process = nullptr;
		std::shared_ptr<spawn::output_stream> stream;
		void* output = nullptr;
		bool owns_pipe = false;
		std::string pending;
	};

	struct surface_pipe {
		std::uint32_t generation = 0;
		void* handle = nullptr;
		bool connected = false;
		bool handshake_done = false;
		std::size_t received = 0;
		attached_surface_message message{};
		time next_pacing_send{};
		std::vector<char> pending_tail;
	};

	struct [[= system_state<"Build Runner">{}]] data {
		[[= shared]] bool building = false;
		[[= shared]] std::uint32_t game_generation = 0;
		[[= shared]] std::filesystem::path game_graph_path;
		[[= shared]] std::optional<attached_session> session;
		build_completion completion;
		std::shared_ptr<spawn::output_stream> active_stream;
		std::jthread worker;
		std::vector<attached_game> games;
		surface_pipe pipe;
	};

	[[= system_init{}]]
	auto init(
		data& d
	) -> async::task<>;

	[[= system_run<>{}]]
	auto run(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> async::task<>;

	[[= system_shutdown{}]]
	auto shutdown(
		data& d
	) -> void;
}

namespace gse::ide::build_runner {
	constexpr std::uint32_t source_state_magic = 0x47534253;
	constexpr std::uint32_t source_state_version = 2;

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
	constexpr std::string_view copy_error_signature = "Error copying file";
	constexpr std::string_view permission_denied_signature = "Permission denied";

	struct source_fingerprint {
		std::uintmax_t size = 0;
		std::int64_t mtime = 0;

		auto operator==(
			const source_fingerprint& other
		) const -> bool = default;
	};

	auto find_build_dir(
		const std::filesystem::path& candidate
	) -> std::filesystem::path;

	auto project_source_roots(
		std::string_view target
	) -> std::vector<std::filesystem::path>;

	auto is_build_source(
		const std::filesystem::path& path
	) -> bool;

	auto source_state_path() -> std::filesystem::path;

	auto load_source_state(
		const std::filesystem::path& path
	) -> std::unordered_map<std::string, source_fingerprint>;

	auto save_source_state(
		const std::filesystem::path& path,
		const std::unordered_map<std::string, source_fingerprint>& state
	) -> void;

	auto refresh_changed_sources(
		spawn::output_stream& stream,
		std::string_view target
	) -> void;

	auto cache_value(
		const std::filesystem::path& build_dir,
		std::string_view key
	) -> std::string;

	auto compiler_bin_dir(
		const std::filesystem::path& build_dir
	) -> std::filesystem::path;

	auto configure_command(
		const std::filesystem::path& project_dir,
		const std::filesystem::path& build_dir
	) -> std::wstring;

	auto ensure_configured(
		spawn::output_stream& stream
	) -> std::filesystem::path;

	auto build_command(
		const std::filesystem::path& build_dir,
		std::string_view target
	) -> std::wstring;

	auto backup_path(
		const std::filesystem::path& executable
	) -> std::filesystem::path;

	auto current_executable() -> std::filesystem::path;

	auto collect_module_write_conflicts(
		std::span<const std::string> lines,
		const std::filesystem::path& build_dir
	) -> std::vector<std::filesystem::path>;

	auto collect_locked_output_copies(
		std::span<const std::string> lines,
		const std::filesystem::path& build_dir
	) -> std::vector<std::filesystem::path>;

	auto clear_stale_module_file(
		spawn::output_stream& stream,
		const std::filesystem::path& file
	) -> bool;

	auto run_build_with_module_recovery(
		const std::stop_token& st,
		spawn::output_stream& stream,
		const std::wstring& command,
		const std::filesystem::path& source_dir,
		const std::filesystem::path& build_dir,
		const std::filesystem::path& compiler_bin
	) -> int;

	auto launch_game_attached(
		build_completion& completion,
		spawn::output_stream& stream,
		std::uint32_t generation
	) -> void;

	auto build_game(
		const std::stop_token& st,
		build_completion& completion,
		spawn::output_stream& stream,
		bool run_after,
		std::uint32_t next_generation
	) -> void;

	auto rebuild_editor(
		const std::stop_token& st,
		spawn::output_stream& stream
	) -> void;

	auto build_worker(
		const std::stop_token& st,
		build_completion* completion,
		std::shared_ptr<spawn::output_stream> stream,
		build_request request,
		std::uint32_t next_generation
	) -> void;

	auto cleanup_backups() -> void;

	auto start_build(
		context& ctx,
		data& d,
		const build_request& request
	) -> void;

	auto drain_completion(
		context& ctx,
		data& d
	) -> void;

	auto poll_games(
		context& ctx,
		data& d
	) -> void;

	auto stop_games(
		data& d
	) -> std::optional<std::uint32_t>;

	auto close_surface_pipe(
		data& d
	) -> void;

	auto import_surface_handles(
		attached_surface_message& message
	) -> bool;

	auto own_surface_message(
		attached_surface_message message
	) -> std::shared_ptr<const attached_surface_message>;

	auto poll_surface_pipe(
		context& ctx,
		data& d
	) -> void;

	auto flush_pipe_tail(
		data& d
	) -> void;

	auto write_pipe_message(
		data& d,
		const void* bytes,
		std::size_t size
	) -> void;

	auto send_attached_input(
		data& d,
		const input::event& event
	) -> void;

	auto send_attached_pacing(
		data& d,
		time_t<std::uint64_t> refresh
	) -> void;
}

auto gse::ide::build_runner::find_build_dir(const std::filesystem::path& candidate) -> std::filesystem::path {
	std::error_code ec;
	if (!std::filesystem::exists(candidate / "build.ninja", ec)) {
		return {};
	}
	return candidate;
}

auto gse::ide::build_runner::project_source_roots(const std::string_view target) -> std::vector<std::filesystem::path> {
	if (target == config::editor_target) {
		return {
			gse::config::source_dir(),
			config::source_dir(),
		};
	}
	return {
		config::engine_source_dir(),
		config::project_source_dir(),
	};
}

auto gse::ide::build_runner::is_build_source(const std::filesystem::path& path) -> bool {
	std::string extension = path.extension().generic_native_encoded_string();
	std::ranges::transform(extension, extension.begin(), [](const unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return std::ranges::contains(build_source_extensions, extension);
}

auto gse::ide::build_runner::source_state_path() -> std::filesystem::path {
	return gse::config::cache_dir() / "source_state.bin";
}

auto gse::ide::build_runner::load_source_state(const std::filesystem::path& path) -> std::unordered_map<std::string, source_fingerprint> {
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		return {};
	}
	binary_reader reader(in);
	std::uint32_t magic = 0;
	std::uint32_t version = 0;
	std::uint32_t epoch = 0;
	reader & magic & version & epoch;
	if (!in || magic != source_state_magic || version != source_state_version || epoch != archive_format_epoch) {
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

auto gse::ide::build_runner::refresh_changed_sources(spawn::output_stream& stream, const std::string_view target) -> void {
	const std::filesystem::path state_path = source_state_path();
	std::error_code exists_ec;
	const bool seeding = !std::filesystem::exists(state_path, exists_ec);
	const std::unordered_map<std::string, source_fingerprint> previous = load_source_state(state_path);

	std::unordered_map<std::string, source_fingerprint> current = previous;
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

auto gse::ide::build_runner::cache_value(const std::filesystem::path& build_dir, const std::string_view key) -> std::string {
	std::ifstream cache(build_dir / "CMakeCache.txt");
	if (!cache) {
		return {};
	}

	const std::string prefix = std::string(key) + ":";
	std::string line;
	while (std::getline(cache, line)) {
		if (!line.starts_with(prefix)) {
			continue;
		}
		const std::size_t equals = line.find('=');
		if (equals == std::string::npos) {
			break;
		}
		return line.substr(equals + 1);
	}
	return {};
}

auto gse::ide::build_runner::compiler_bin_dir(const std::filesystem::path& build_dir) -> std::filesystem::path {
	const std::string compiler = cache_value(build_dir, "CMAKE_CXX_COMPILER");
	if (compiler.empty()) {
		return {};
	}
	std::filesystem::path bin = std::filesystem::path(compiler).parent_path();
	bin.make_preferred();
	return bin;
}

auto gse::ide::build_runner::configure_command(const std::filesystem::path& project_dir, const std::filesystem::path& build_dir) -> std::wstring {
	const std::filesystem::path& editor_build = gse::ide::config::build_dir();

	std::wstring command = L"cmd.exe /c cmake -G Ninja -S \"" + project_dir.wstring() + L"\" -B \"" + build_dir.wstring() + L"\"";
	command += L" -DGSE_ENGINE_DIR=\"" + gse::ide::config::engine_root().wstring() + L"\"";
	command += L" -DVCPKG_MANIFEST_MODE=OFF";
	command += L" -DVCPKG_INSTALLED_DIR=\"" + (editor_build / "vcpkg_installed").wstring() + L"\"";

	constexpr std::array<std::string_view, 7> inherited = {
		"CMAKE_TOOLCHAIN_FILE",
		"VCPKG_TARGET_TRIPLET",
		"VCPKG_OVERLAY_PORTS",
		"VCPKG_OVERLAY_TRIPLETS",
		"CMAKE_C_COMPILER",
		"CMAKE_CXX_COMPILER",
		"CMAKE_BUILD_TYPE",
	};

	for (const std::string_view key : inherited) {
		const std::string value = cache_value(editor_build, key);
		if (value.empty()) {
			continue;
		}
		const std::wstring wide_key(key.begin(), key.end());
		command += L" -D" + wide_key + L"=\"" + std::filesystem::path(value).wstring() + L"\"";
	}

	return command;
}

auto gse::ide::build_runner::ensure_configured(spawn::output_stream& stream) -> std::filesystem::path {
	const std::filesystem::path& build_dir = config::project_build_dir();
	std::error_code ec;

	if (!find_build_dir(build_dir).empty()) {
		const std::string bound = cache_value(build_dir, "GSE_ENGINE_DIR");
		const std::string expected = config::engine_root().generic_native_encoded_string();
		if (bound.empty() || bound == expected) {
			return build_dir;
		}

		spawn::emit(stream, "build tree was configured against " + bound);
		spawn::emit(stream, "project now binds " + expected + "; removing the stale build tree...");
		std::filesystem::remove_all(build_dir, ec);
		if (ec) {
			spawn::emit(stream, "could not remove " + build_dir.generic_display_string() + "; delete it and build again");
			return {};
		}
		ec.clear();
	}

	const std::filesystem::path& project_dir = config::project_root();
	if (!std::filesystem::exists(project_dir / "CMakeLists.txt", ec)) {
		return {};
	}

	spawn::emit(stream, "configuring " + project_dir.generic_display_string() + "...");
	std::filesystem::create_directories(build_dir, ec);

	const std::filesystem::path compiler_bin = compiler_bin_dir(config::build_dir());
	if (spawn::run_capture(stream, configure_command(project_dir, build_dir), project_dir.wstring(), compiler_bin) != 0) {
		spawn::emit(stream, "configure failed");
		return {};
	}

	return find_build_dir(build_dir);
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
	const std::span<const std::string> lines,
	const std::filesystem::path& build_dir
) -> std::vector<std::filesystem::path> {
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

auto gse::ide::build_runner::collect_locked_output_copies(const std::span<const std::string> lines, const std::filesystem::path& build_dir) -> std::vector<std::filesystem::path> {
	std::vector<std::filesystem::path> locked;
	std::unordered_set<std::string> seen;
	const std::filesystem::path normalized_build_dir = std::filesystem::absolute(build_dir).lexically_normal();

	for (const std::string& line : lines) {
		if (line.find(copy_error_signature) == std::string::npos || line.find(permission_denied_signature) == std::string::npos) {
			continue;
		}

		std::vector<std::string> quoted;
		for (std::size_t pos = 0;;) {
			const std::size_t open = line.find('"', pos);
			if (open == std::string::npos) {
				break;
			}
			const std::size_t close = line.find('"', open + 1);
			if (close == std::string::npos) {
				break;
			}
			quoted.push_back(line.substr(open + 1, close - open - 1));
			pos = close + 1;
		}
		if (quoted.size() < 2) {
			continue;
		}

		const std::filesystem::path source(quoted[0]);
		std::filesystem::path target(quoted[1]);

		std::error_code ec;
		if (std::filesystem::is_directory(target, ec)) {
			target /= source.filename();
		}
		target = std::filesystem::absolute(target).lexically_normal();

		const std::filesystem::path relative = target.lexically_relative(normalized_build_dir);
		if (relative.empty() || relative.is_absolute() || *relative.begin() == "..") {
			continue;
		}
		if (seen.insert(target.generic_native_encoded_string()).second) {
			locked.push_back(std::move(target));
		}
	}
	return locked;
}

auto gse::ide::build_runner::clear_stale_module_file(spawn::output_stream& stream, const std::filesystem::path& file) -> bool {
	constexpr int clear_attempts = 12;
	constexpr std::chrono::milliseconds clear_retry_delay(50);

	std::error_code ec;
	for (int attempt = 0; attempt < clear_attempts; ++attempt) {
		ec.clear();
		if (std::filesystem::remove(file, ec)) {
			return true;
		}
		if (!ec) {
			return false;
		}
		std::this_thread::sleep_for(clear_retry_delay);
	}

	std::filesystem::path aside = file;
	aside += ".stale";

	std::error_code drop;
	std::filesystem::remove(aside, drop);

	std::error_code moved;
	std::filesystem::rename(file, aside, moved);
	if (!moved) {
		return true;
	}

	spawn::emit(stream, "could not clear stale module cache file " + file.generic_display_string() + " after " + std::to_string(clear_attempts) + " attempts: " + ec.message());
	return false;
}

auto gse::ide::build_runner::run_build_with_module_recovery(
	const std::stop_token& st,
	spawn::output_stream& stream,
	const std::wstring& command,
	const std::filesystem::path& source_dir,
	const std::filesystem::path& build_dir,
	const std::filesystem::path& compiler_bin
) -> int {
	constexpr int max_attempts = 16;
	std::unordered_set<std::string> recovered;
	for (int attempt = 1; ; ++attempt) {
		spawn::begin_transcript(stream);
		const int code = spawn::run_capture(stream, command, source_dir.wstring(), compiler_bin);
		const std::vector<std::string> transcript = spawn::take_transcript(stream);
		if (code == 0 || attempt >= max_attempts || st.stop_requested() || stream.terminated.load(std::memory_order_acquire)) {
			return code;
		}

		const std::vector<std::filesystem::path> conflicts = collect_module_write_conflicts(transcript, build_dir);
		const std::vector<std::filesystem::path> locked = collect_locked_output_copies(transcript, build_dir);
		if (conflicts.empty() && locked.empty()) {
			return code;
		}

		std::size_t cleared = 0;
		for (const std::filesystem::path& gcm : conflicts) {
			const std::string key = gcm.generic_native_encoded_string();
			if (recovered.contains(key)) {
				continue;
			}
			if (clear_stale_module_file(stream, gcm)) {
				recovered.insert(key);
				++cleared;
			}
		}

		// A runtime dependency the running editor has loaded cannot be overwritten, but Windows
		// does allow renaming it, which is the same trick the self-rebuild uses on Editor.exe.
		// Moving it aside frees the path so the copy lands; the .bak is reclaimed on a later build.
		std::size_t displaced = 0;
		for (const std::filesystem::path& file : locked) {
			const std::string key = file.generic_native_encoded_string();
			if (recovered.contains(key)) {
				continue;
			}
			std::filesystem::path backup = file;
			backup += ".bak";

			std::error_code ec;
			std::filesystem::remove(backup, ec);
			ec.clear();
			std::filesystem::rename(file, backup, ec);
			if (!ec) {
				recovered.insert(key);
				++displaced;
			}
			else {
				spawn::emit(stream, "could not displace locked file " + file.generic_display_string() + ": " + ec.message());
			}
		}

		if (cleared == 0 && displaced == 0) {
			return code;
		}

		if (cleared > 0) {
			spawn::emit(stream, "cleared " + std::to_string(cleared) + " stale module cache file(s); retrying build");
		}
		if (displaced > 0) {
			spawn::emit(stream, "displaced " + std::to_string(displaced) + " locked runtime file(s); retrying build");
		}
	}
}

auto gse::ide::build_runner::launch_game_attached(build_completion& completion, spawn::output_stream& stream, const std::uint32_t generation) -> void {
	const std::filesystem::path& game_exe = config::game_executable();
	std::error_code ec;
	if (!std::filesystem::exists(game_exe, ec)) {
		spawn::emit(stream, "game executable not found: " + game_exe.generic_display_string());
		return;
	}

	const win32::DWORD editor_pid = win32::GetCurrentProcessId();
	const std::string pipe_name = "\\\\.\\pipe\\gse_editor_" + std::to_string(editor_pid) + "_" + std::to_string(generation);
	const std::wstring wide_pipe(pipe_name.begin(), pipe_name.end());
	const std::filesystem::path graph_file = std::filesystem::temp_directory_path() / std::format("gse_editor_game_graph_{}_{}.bin", editor_pid, generation);

	void* pipe = win32::CreateNamedPipeW(wide_pipe.c_str(), win32::pipe_access_duplex, win32::pipe_type_byte | win32::pipe_nowait, 1, sizeof(attached_input_message) * 512, sizeof(attached_surface_message) * 2, 0, nullptr);
	if (!win32::valid_handle(pipe)) {
		spawn::emit(stream, "failed to create editor pipe");
		return;
	}

	std::wstring command = L"\"" + game_exe.wstring() + L"\"";
	command += L" --engine-attached";
	command += L" --engine-ipc-pipe-name " + wide_pipe;
	command += L" --engine-parent-pid " + std::to_wstring(editor_pid);
	command += L" --engine-dump-system-graph-path \"" + graph_file.wstring() + L"\"";

	const spawn::launched game = spawn::launch_streamed(command, config::project_root().wstring());
	if (!win32::valid_handle(game.process)) {
		win32::CloseHandle(pipe);
		spawn::emit(stream, "failed to launch game");
		return;
	}

	spawn::emit(stream, "launched game (pid " + std::to_string(game.pid) + ")");

	std::lock_guard lock(completion.mutex);
	completion.game_launched = true;
	completion.game_process = game.process;
	completion.game_job = game.job;
	completion.game_output = game.output;
	completion.game_pid = game.pid;
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
	const std::filesystem::path build_dir = ensure_configured(stream);
	if (build_dir.empty()) {
		spawn::emit(stream, "configured build directory is unavailable");
		return;
	}

	refresh_changed_sources(stream, config::game_target());
	const std::filesystem::path compiler_bin = compiler_bin_dir(build_dir);

	const std::filesystem::path& game_exe = config::game_executable();
	const std::filesystem::path backup = backup_path(game_exe);
	std::error_code ec;
	if (std::filesystem::exists(game_exe, ec)) {
		std::filesystem::remove(backup, ec);
		std::filesystem::rename(game_exe, backup, ec);
		if (ec) {
			spawn::emit(stream, "could not move aside " + game_exe.filename().generic_display_string() + "; aborting build");
			return;
		}
	}

	spawn::emit(stream, "building " + std::string(config::game_target()) + "...");
	const int code = run_build_with_module_recovery(st, stream, build_command(build_dir, config::game_target()), config::project_root(), build_dir, compiler_bin);
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
	const std::filesystem::path build_dir = find_build_dir(config::build_dir());
	if (build_dir.empty()) {
		spawn::emit(stream, "configured build directory is unavailable");
		return;
	}

	refresh_changed_sources(stream, config::editor_target);
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
	const int code = run_build_with_module_recovery(st, stream, build_command(build_dir, config::editor_target), gse::config::root_dir(), build_dir, compiler_bin);
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
	app::relaunch_on_exit(editor_exe, gse::config::root_dir());
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

	spawn::close_process(*stream);

	std::lock_guard lock(completion->mutex);
	completion->done = true;
}

auto gse::ide::build_runner::cleanup_backups() -> void {
	std::error_code ec;
	std::filesystem::remove(backup_path(config::game_executable()), ec);
	std::filesystem::remove(backup_path(config::editor_executable()), ec);
	const std::filesystem::path editor_exe = current_executable();
	if (!editor_exe.empty()) {
		std::filesystem::remove(backup_path(editor_exe), ec);
	}
}

auto gse::ide::build_runner::start_build(context& ctx, data& d, const build_request& request) -> void {
	if (d.building) {
		return;
	}

	// Windows keeps the image mapped while the process lives, so the linker cannot
	// overwrite the game exe until every running instance has actually exited.
	if (request.target == build_target::game) {
		if (const std::optional<std::uint32_t> ended = stop_games(d)) {
			ctx.channels.push<attached_session_ended>({
				.generation = *ended,
			});
		}
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
		d.completion.game_job = nullptr;
		d.completion.game_output = nullptr;
		d.completion.game_pid = 0;
		d.completion.surface_pipe = nullptr;
		d.completion.graph_path.clear();
	}

	d.building = true;
	d.active_stream = stream;
	d.worker = std::jthread(build_worker, &d.completion, std::move(stream), request, d.game_generation + 1);
}

auto gse::ide::build_runner::drain_completion(context& ctx, data& d) -> void {
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
		d.pipe.generation = d.completion.generation;
		d.pipe.handle = d.completion.surface_pipe;
		d.game_graph_path = std::move(d.completion.graph_path);
		d.session.emplace(attached_session{
			.generation = d.completion.generation,
			.status = attached_session_status::awaiting_surface,
		});
		for (attached_game& game : d.games) {
			game.owns_pipe = false;
		}

		auto stream = std::make_shared<spawn::output_stream>();
		stream->running.store(true, std::memory_order_release);
		spawn::attach_process(*stream, d.completion.game_process, d.completion.game_job);
		ctx.channels.push<stream_opened>({
			.name = std::format("Game {}", d.completion.game_pid),
			.stream = stream,
		});

		d.games.push_back({
			.generation = d.completion.generation,
			.process = d.completion.game_process,
			.stream = std::move(stream),
			.output = d.completion.game_output,
			.owns_pipe = true,
		});
	}
	d.active_stream.reset();
	ctx.channels.push<build_finished>({});
}

auto gse::ide::build_runner::poll_games(context& ctx, data& d) -> void {
	for (std::size_t i = 0; i < d.games.size();) {
		attached_game& game = d.games[i];
		if (game.output && !spawn::pump_output(*game.stream, game.output, game.pending)) {
			win32::CloseHandle(game.output);
			game.output = nullptr;
		}
		if (win32::WaitForSingleObject(game.process, 0) != win32::wait_object_0) {
			++i;
			continue;
		}
		if (game.output) {
			spawn::pump_output(*game.stream, game.output, game.pending);
			win32::CloseHandle(game.output);
			game.output = nullptr;
		}
		spawn::close_process(*game.stream);
		win32::CloseHandle(game.process);
		if (game.owns_pipe) {
			close_surface_pipe(d);
			if (d.session && d.session->generation == game.generation) {
				d.session.reset();
			}
			ctx.channels.push<attached_session_ended>({
				.generation = game.generation,
			});
		}
		d.games.erase(d.games.begin() + static_cast<std::ptrdiff_t>(i));
	}
}

auto gse::ide::build_runner::stop_games(data& d) -> std::optional<std::uint32_t> {
	const std::optional<std::uint32_t> ended = d.session
		? std::optional<std::uint32_t>{ d.session->generation }
		: std::nullopt;
	for (attached_game& game : d.games) {
		spawn::terminate_process(*game.stream);
		win32::WaitForSingleObject(game.process, 5000);
		if (game.output) {
			spawn::pump_output(*game.stream, game.output, game.pending);
			win32::CloseHandle(game.output);
			game.output = nullptr;
		}
		spawn::close_process(*game.stream);
		win32::CloseHandle(game.process);
	}
	d.games.clear();
	close_surface_pipe(d);
	d.session.reset();
	return ended;
}

auto gse::ide::build_runner::close_surface_pipe(data& d) -> void {
	if (d.pipe.handle) {
		win32::DisconnectNamedPipe(d.pipe.handle);
		win32::CloseHandle(d.pipe.handle);
	}
	d.pipe.handle = nullptr;
	d.pipe.connected = false;
	d.pipe.handshake_done = false;
	d.pipe.received = 0;
	d.pipe.generation = 0;
	d.pipe.message = {};
	d.pipe.pending_tail.clear();
}

auto gse::ide::build_runner::import_surface_handles(attached_surface_message& message) -> bool {
	void* game = win32::OpenProcess(win32::process_dup_handle, 0, message.pid);
	if (!win32::valid_handle(game)) {
		return false;
	}

	std::array<void*, attached_ring_size> surfaces{};
	void* produced_semaphore = nullptr;
	void* consumed_semaphore = nullptr;
	bool ok = true;
	for (std::size_t i = 0; i < attached_ring_size; ++i) {
		if (!win32::DuplicateHandle(game, message.surface_handles[i], win32::GetCurrentProcess(), &surfaces[i], 0, 0, win32::duplicate_same_access)) {
			ok = false;
		}
	}
	if (ok && !win32::DuplicateHandle(game, message.produced_semaphore_handle, win32::GetCurrentProcess(), &produced_semaphore, 0, 0, win32::duplicate_same_access)) {
		ok = false;
	}
	if (ok && !win32::DuplicateHandle(game, message.consumed_semaphore_handle, win32::GetCurrentProcess(), &consumed_semaphore, 0, 0, win32::duplicate_same_access)) {
		ok = false;
	}
	win32::CloseHandle(game);

	if (!ok) {
		for (void* surface : surfaces) {
			if (win32::valid_handle(surface)) {
				win32::CloseHandle(surface);
			}
		}
		if (win32::valid_handle(produced_semaphore)) {
			win32::CloseHandle(produced_semaphore);
		}
		if (win32::valid_handle(consumed_semaphore)) {
			win32::CloseHandle(consumed_semaphore);
		}
		return false;
	}

	for (std::size_t i = 0; i < attached_ring_size; ++i) {
		message.surface_handles[i] = surfaces[i];
	}
	message.produced_semaphore_handle = produced_semaphore;
	message.consumed_semaphore_handle = consumed_semaphore;
	return true;
}

auto gse::ide::build_runner::own_surface_message(attached_surface_message message) -> std::shared_ptr<const attached_surface_message> {
	return {
		new attached_surface_message(std::move(message)),
		[](const attached_surface_message* owned) {
			for (void* surface : owned->surface_handles) {
				if (win32::valid_handle(surface)) {
					win32::CloseHandle(surface);
				}
			}
			if (win32::valid_handle(owned->produced_semaphore_handle)) {
				win32::CloseHandle(owned->produced_semaphore_handle);
			}
			if (win32::valid_handle(owned->consumed_semaphore_handle)) {
				win32::CloseHandle(owned->consumed_semaphore_handle);
			}
			delete owned;
		},
	};
}

auto gse::ide::build_runner::poll_surface_pipe(context& ctx, data& d) -> void {
	if (!d.pipe.handle || d.pipe.handshake_done) {
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

	d.pipe.received = 0;
	if (d.pipe.message.magic == attached_surface_magic && import_surface_handles(d.pipe.message)) {
		ctx.channels.push<attached_surface_ready>({
			.generation = d.pipe.generation,
			.message = own_surface_message(std::move(d.pipe.message)),
		});
		d.pipe.message = {};
		d.pipe.handshake_done = true;
		return;
	}
	close_surface_pipe(d);
}

auto gse::ide::build_runner::flush_pipe_tail(data& d) -> void {
	if (!d.pipe.handle || d.pipe.pending_tail.empty()) {
		return;
	}
	win32::DWORD written = 0;
	if (!win32::WriteFile(d.pipe.handle, d.pipe.pending_tail.data(), static_cast<win32::DWORD>(d.pipe.pending_tail.size()), &written, nullptr)) {
		log::println(log::level::warning, log::category::general, "attached pipe: write failed (error {}); closing pipe", win32::GetLastError());
		close_surface_pipe(d);
		return;
	}
	d.pipe.pending_tail.erase(d.pipe.pending_tail.begin(), d.pipe.pending_tail.begin() + written);
}

auto gse::ide::build_runner::write_pipe_message(data& d, const void* bytes, const std::size_t size) -> void {
	if (!d.pipe.handle || !d.pipe.handshake_done) {
		return;
	}
	flush_pipe_tail(d);
	if (!d.pipe.handle || !d.pipe.pending_tail.empty()) {
		return;
	}
	win32::DWORD written = 0;
	if (!win32::WriteFile(d.pipe.handle, bytes, static_cast<win32::DWORD>(size), &written, nullptr)) {
		log::println(log::level::warning, log::category::general, "attached pipe: write failed (error {}); closing pipe", win32::GetLastError());
		close_surface_pipe(d);
		return;
	}
	if (written < size) {
		const auto* tail = static_cast<const char*>(bytes) + written;
		d.pipe.pending_tail.assign(tail, tail + (size - written));
	}
}

auto gse::ide::build_runner::send_attached_input(data& d, const input::event& event) -> void {
	const attached_input_message message{
		.magic = attached_input_magic,
		.event = event,
	};
	write_pipe_message(d, &message, sizeof(message));
}

auto gse::ide::build_runner::send_attached_pacing(data& d, const time_t<std::uint64_t> refresh) -> void {
	if (refresh == time_t<std::uint64_t>{}) {
		return;
	}
	const attached_pacing_message message{
		.magic = attached_pacing_magic,
		.refresh = refresh,
	};
	write_pipe_message(d, &message, sizeof(message));
}

auto gse::ide::build_runner::init(data&) -> async::task<> {
	cleanup_backups();
	return {};
}

auto gse::ide::build_runner::run(context& ctx, shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	for (const attached_surface_imported& imported : ctx.read_channel<attached_surface_imported>()) {
		if (d.session && d.session->generation == imported.generation) {
			d.session->status = attached_session_status::active;
		}
	}
	for (const attached_surface_rejected& rejected : ctx.read_channel<attached_surface_rejected>()) {
		if (d.session && d.session->generation == rejected.generation) {
			if (const std::optional<std::uint32_t> ended = stop_games(d)) {
				ctx.channels.push<attached_session_ended>({
					.generation = *ended,
				});
			}
		}
	}
	for (const build_request& request : ctx.read_channel<build_request>()) {
		start_build(ctx, d, request);
	}
	drain_completion(ctx, d);
	poll_games(ctx, d);
	poll_surface_pipe(ctx, d);
	flush_pipe_tail(d);
	for (const attached_input& forwarded : ctx.read_channel<attached_input>()) {
		send_attached_input(d, forwarded.event);
	}
	if (d.pipe.handshake_done && gpu_s.swapchain) {
		if (const auto now = system_clock::now<time>(); now >= d.pipe.next_pacing_send) {
			send_attached_pacing(d, gpu_s.swapchain->refresh_interval());
			d.pipe.next_pacing_send = now + seconds(1.f);
		}
	}
	if (d.session && !d.pipe.handle) {
		if (const std::optional<std::uint32_t> ended = stop_games(d)) {
			ctx.channels.push<attached_session_ended>({
				.generation = *ended,
			});
		}
	}
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
	(void)stop_games(d);
}

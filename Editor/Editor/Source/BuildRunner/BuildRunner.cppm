export module gse.ide.build:build_runner;

import gse;
import gse.config;
import gse.gpu;
import gse.ide.config;
import gse.win32;
import std;

import :configs;
import :profiles;
import :spawn;

export namespace gse::ide::build_runner {
	struct stop_session_request {};

	struct build_request {
		build_target target = build_target::game;
		bool run_after = false;
		play_session session;
		std::string config;
		std::string profile;
		const config::worktree* tree = nullptr;
		std::string inbox_id;
	};

	struct select_profile_request {
		std::string name;
	};

	struct edit_profiles_request {
		std::vector<build_profile> profiles;
		std::string active;
	};

	enum class stream_kind : std::uint8_t {
		none,
		build_game,
		build_editor,
		game,
	};

	struct build_error {
		std::filesystem::path file;
		std::uint32_t line = 0;
		std::string message;
		std::vector<std::string> notes;
		std::vector<std::filesystem::path> related;
	};

	struct build_finished {
		id key;
		stream_kind kind = stream_kind::none;
		std::vector<build_error> errors;
		std::string inbox_id;
		bool succeeded = false;
	};

	struct source_changed {
		std::filesystem::path path;
		std::int64_t mtime = 0;
	};

	struct stream_opened {
		std::string name;
		stream_kind kind = stream_kind::none;
		std::shared_ptr<spawn::output_stream> stream;
		std::optional<std::uint32_t> attached_instance;
	};

	constexpr std::uint32_t max_attached_instances = 4;

	template <typename Data>
	[[nodiscard]] auto session_for(
		const Data& d,
		std::uint32_t generation,
		std::uint32_t instance
	) -> bool;

	struct attached_surface_ready {
		std::uint32_t generation = 0;
		std::uint32_t instance = 0;
		std::uint32_t revision = 0;
		std::shared_ptr<const attached_surface_message> message;
	};

	struct attached_surface_imported {
		std::uint32_t generation = 0;
		std::uint32_t instance = 0;
	};

	struct attached_surface_rejected {
		std::uint32_t generation = 0;
		std::uint32_t instance = 0;
		std::string reason;
	};

	struct attached_session_ended {
		std::uint32_t generation = 0;
		std::uint32_t instance = 0;
	};

	struct attached_fatal_reported {
		std::uint32_t generation = 0;
		std::uint32_t instance = 0;
		std::string file;
		std::uint32_t line = 0;
		std::string function;
		std::string comment;
	};

	struct attached_input {
		std::uint32_t instance = 0;
		input::event event;
	};

	struct attached_resize {
		std::uint32_t instance = 0;
		vec2u extent{ 0, 0 };
	};

	enum class attached_session_status : std::uint8_t {
		awaiting_surface,
		active,
	};

	struct attached_session {
		std::uint32_t generation = 0;
		std::uint32_t instance = 0;
		attached_session_status status = attached_session_status::awaiting_surface;
	};

	struct launched_child {
		std::uint32_t instance = 0;
		void* process = nullptr;
		void* job = nullptr;
		void* output = nullptr;
		std::uint32_t pid = 0;
		void* surface_pipe = nullptr;
		bool attached = false;
		std::string label;
	};

	struct build_completion {
		std::mutex mutex;
		id key;
		stream_kind kind = stream_kind::none;
		std::vector<build_error> errors;
		bool done = false;
		bool succeeded = false;
		bool game_launched = false;
		std::uint32_t generation = 0;
		std::filesystem::path graph_path;
		std::vector<launched_child> children;
	};

	struct server_status {
		bool running = false;
		std::uint16_t port = 0;
	};

	struct attached_game {
		std::uint32_t generation = 0;
		std::uint32_t instance = 0;
		void* process = nullptr;
		std::shared_ptr<spawn::output_stream> stream;
		void* output = nullptr;
		bool owns_pipe = false;
		std::string pending;
	};

	struct surface_pipe {
		std::uint32_t generation = 0;
		std::uint32_t instance = 0;
		void* handle = nullptr;
		bool connected = false;
		bool handshake_done = false;
		std::size_t received = 0;
		std::size_t expected = sizeof(std::uint32_t);
		bool have_magic = false;
		std::array<char, std::max(sizeof(attached_surface_message), sizeof(attached_fatal_message))> bytes{};
		std::vector<char> pending_tail;
	};

	struct [[= system_state<"Build Runner">{}]] data {
		[[= shared]] bool building = false;
		[[= shared]] bool building_session = false;
		[[= shared]] std::uint32_t game_generation = 0;
		[[= shared]] std::filesystem::path game_graph_path;
		[[= shared]] std::array<attached_session, max_attached_instances> sessions{};
		[[= shared]] std::string session_error;
		[[= shared]] play_session session;
		[[= shared]] server_status server;
		[[= shared]] std::vector<build_profile> profiles;
		[[= shared]] std::string active_profile;
		build_completion completion;
		std::shared_ptr<spawn::output_stream> active_stream;
		task::thread worker;
		std::vector<attached_game> games;
		std::vector<attached_game> stopping;
		time stopping_deadline;
		std::array<surface_pipe, max_attached_instances> pipes{};
		std::optional<std::filesystem::file_time_type> editor_image_time;
		std::int64_t editor_image_reported = 0;
		bool editor_image_missing = false;
		bool editor_image_waiting = false;
		time next_image_poll{};
		std::string inbox_id;
	};

	[[= system_init{}]]
	auto init(
		data& d
	) -> async::task<>;

	[[= system_run<>{}]]
	auto run(
		context& ctx,
		data& d,
		channel_read<attached_surface_imported, attached_surface_rejected, build_request, stop_session_request, attached_input, attached_resize, select_profile_request, edit_profiles_request> requests_in,
		channel_write<attached_session_ended, stream_opened, build_finished, attached_surface_ready, attached_fatal_reported> events_out
	) -> async::task<>;

	[[= system_shutdown{}]]
	auto shutdown(
		data& d
	) -> void;

	auto request_for_profile(
		const build_profile& profile,
		bool run_after
	) -> build_request;

	auto request_analysis_pause(
		bool paused
	) -> void;

	auto analysis_pause_requested() -> bool;

	auto report_analysis_busy(
		bool busy
	) -> void;

	auto is_build_source(
		const std::filesystem::path& path
	) -> bool;

	auto is_build_touch(
		const std::filesystem::path& file,
		std::int64_t mtime
	) -> bool;

	auto build_key(
		const config::worktree& tree,
		std::string_view target
	) -> id;

	auto build_keys_for(
		const std::filesystem::path& file
	) -> std::vector<id>;

	auto build_times() -> std::unordered_map<id, std::int64_t>;
}

namespace gse::ide::build_runner {
	auto analysis_pause_state() -> std::atomic<bool>&;

	auto analysis_busy_state() -> std::atomic<bool>&;

	constexpr std::string_view backup_suffix = ".bak";
	constexpr std::uint32_t max_backups = 64;
	constexpr std::uint32_t source_state_magic = 0x47534253;
	constexpr std::uint32_t source_state_version = 2;
	constexpr std::uint32_t build_state_magic = 0x47534254;
	constexpr std::uint32_t build_state_version = 1;

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
	constexpr std::string_view dependency_cycle_signature = "build stopped: dependency cycle:";
	constexpr std::string_view ninja_deps_name = ".ninja_deps";

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
		const config::worktree& tree,
		std::string_view target
	) -> std::vector<std::filesystem::path>;

	auto is_inside(
		const std::filesystem::path& file,
		const std::filesystem::path& root
	) -> bool;

	struct build_touch_log {
		std::mutex mutex;
		std::unordered_map<id, std::int64_t> stamps;
	};

	auto build_touches() -> build_touch_log&;

	auto begin_build_touches() -> void;

	auto note_build_touch(
		const std::filesystem::path& file,
		std::int64_t mtime
	) -> void;

	auto source_state_path() -> std::filesystem::path;

	auto build_state_path() -> std::filesystem::path;

	auto record_build_time(
		id key,
		std::int64_t built_at
	) -> void;

	auto source_snapshot_time() -> std::int64_t;

	auto load_source_state(
		const std::filesystem::path& path
	) -> std::unordered_map<std::string, source_fingerprint>;

	auto save_source_state(
		const std::filesystem::path& path,
		const std::unordered_map<std::string, source_fingerprint>& state
	) -> void;

	auto refresh_changed_sources(
		spawn::output_stream& stream,
		const config::worktree& tree,
		std::string_view target
	) -> void;

	auto cache_value(
		const std::filesystem::path& build_dir,
		std::string_view key
	) -> std::string;

	auto compiler_bin_dir(
		const std::filesystem::path& build_dir
	) -> std::filesystem::path;

	auto retarget_build_path(
		const std::filesystem::path& path,
		const std::filesystem::path& from,
		const std::filesystem::path& to
	) -> std::filesystem::path;

	auto worktree_for_config(
		const config::worktree& tree,
		std::string_view name
	) -> config::worktree;

	auto engine_build_dir_for_config(
		const config::worktree& tree,
		std::string_view name
	) -> std::filesystem::path;

	auto preset_configure_command(
		const std::filesystem::path& engine_dir,
		std::string_view name
	) -> std::wstring;

	auto configure_command(
		const config::worktree& tree,
		const std::filesystem::path& project_dir,
		const std::filesystem::path& build_dir,
		const std::filesystem::path& inherit_from
	) -> std::wstring;

	auto ensure_engine_configured(
		spawn::output_stream& stream,
		const config::worktree& tree,
		const std::filesystem::path& build_dir,
		std::string_view name
	) -> bool;

	auto ensure_configured(
		spawn::output_stream& stream,
		const config::worktree& tree,
		std::string_view name
	) -> std::filesystem::path;

	auto build_command(
		const std::filesystem::path& build_dir,
		std::string_view target
	) -> std::wstring;

	auto sweep_backups(
		const std::filesystem::path& executable
	) -> std::size_t;

	auto next_backup_path(
		const std::filesystem::path& executable
	) -> std::filesystem::path;

	auto current_executable() -> std::filesystem::path;

	auto image_readable(
		const std::filesystem::path& file
	) -> bool;

	auto watch_editor_image(
		data& d
	) -> void;

	auto collect_module_write_conflicts(
		std::span<const std::string> lines,
		const std::filesystem::path& build_dir
	) -> std::vector<std::filesystem::path>;

	auto collect_locked_output_copies(
		std::span<const std::string> lines,
		const std::filesystem::path& build_dir
	) -> std::vector<std::filesystem::path>;

	struct diagnostic_site {
		std::filesystem::path file;
		std::uint32_t line = 0;
	};

	auto parse_diagnostic_site(
		std::string_view text,
		const std::filesystem::path& build_dir
	) -> std::optional<diagnostic_site>;

	auto collect_error_sites(
		std::span<const std::string> lines,
		const std::filesystem::path& build_dir
	) -> std::vector<build_error>;

	auto clear_stale_module_file(
		spawn::output_stream& stream,
		const std::filesystem::path& file
	) -> bool;

	auto collect_dependency_cycle(
		std::span<const std::string> lines
	) -> std::string;

	auto clear_dependency_log(
		spawn::output_stream& stream,
		const std::filesystem::path& build_dir
	) -> bool;

	struct build_outcome {
		int code = 0;
		std::vector<build_error> errors;
	};

	auto run_build_with_module_recovery(
		const std::stop_token& st,
		spawn::output_stream& stream,
		const std::wstring& command,
		const std::filesystem::path& source_dir,
		const std::filesystem::path& build_dir,
		const std::filesystem::path& compiler_bin
	) -> build_outcome;

	auto launch_game_attached(
		build_completion& completion,
		spawn::output_stream& stream,
		const config::worktree& tree,
		std::uint32_t generation,
		std::uint32_t instance,
		std::string label,
		const std::wstring& extra_args
	) -> void;

	auto launch_child(
		build_completion& completion,
		spawn::output_stream& stream,
		const config::worktree& tree,
		std::string label,
		const std::wstring& extra_args
	) -> void;

	auto launch_play_session(
		build_completion& completion,
		spawn::output_stream& stream,
		const config::worktree& tree,
		const play_session& session,
		std::uint32_t generation
	) -> void;

	auto build_game(
		const std::stop_token& st,
		build_completion& completion,
		spawn::output_stream& stream,
		const config::worktree& tree,
		const build_request& request,
		std::uint32_t next_generation
	) -> void;

	auto rebuild_editor(
		const std::stop_token& st,
		build_completion& completion,
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
		channel_write<attached_session_ended, stream_opened, build_finished, attached_surface_ready, attached_fatal_reported> events_out,
		data& d,
		const build_request& request
	) -> void;

	auto drain_completion(
		channel_write<attached_session_ended, stream_opened, build_finished, attached_surface_ready, attached_fatal_reported> events_out,
		data& d
	) -> void;

	auto poll_games(
		channel_write<attached_session_ended, stream_opened, build_finished, attached_surface_ready, attached_fatal_reported> events_out,
		data& d
	) -> void;

	auto stop_games(
		data& d
	) -> std::optional<std::uint32_t>;

	auto poll_stopping_games(
		data& d,
		bool block_until_exit
	) -> void;

	auto close_surface_pipe(
		data& d,
		std::uint32_t instance
	) -> void;

	auto import_surface_handles(
		attached_surface_message& message
	) -> bool;

	auto own_surface_message(
		attached_surface_message message
	) -> std::shared_ptr<const attached_surface_message>;

	auto poll_surface_pipe(
		channel_write<attached_session_ended, stream_opened, build_finished, attached_surface_ready, attached_fatal_reported> events_out,
		data& d,
		std::uint32_t instance
	) -> void;

	auto flush_pipe_tail(
		data& d,
		std::uint32_t instance
	) -> void;

	auto write_pipe_message(
		data& d,
		std::uint32_t instance,
		const void* bytes,
		std::size_t size
	) -> void;

	auto send_attached_input(
		data& d,
		std::uint32_t instance,
		const input::event& event
	) -> void;

	auto send_attached_resize(
		data& d,
		std::uint32_t instance,
		vec2u extent
	) -> void;
}

auto gse::ide::build_runner::find_build_dir(const std::filesystem::path& candidate) -> std::filesystem::path {
	std::error_code ec;
	if (!std::filesystem::exists(candidate / "build.ninja", ec)) {
		return {};
	}
	return candidate;
}

auto gse::ide::build_runner::project_source_roots(const config::worktree& tree, const std::string_view target) -> std::vector<std::filesystem::path> {
	if (target == config::editor_target) {
		return {
			gse::config::source_dir(),
			config::source_dir(),
		};
	}
	return {
		tree.engine_source,
		tree.project_source,
	};
}

auto gse::ide::build_runner::is_build_source(const std::filesystem::path& path) -> bool {
	std::string extension = path.extension().generic_native_encoded_string();
	std::ranges::transform(extension, extension.begin(), [](const unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return std::ranges::contains(build_source_extensions, extension);
}

auto gse::ide::build_runner::build_touches() -> build_touch_log& {
	static build_touch_log log;
	return log;
}

auto gse::ide::build_runner::begin_build_touches() -> void {
	build_touch_log& log = build_touches();
	std::lock_guard _(log.mutex);
	log.stamps.clear();
}

auto gse::ide::build_runner::note_build_touch(const std::filesystem::path& file, const std::int64_t mtime) -> void {
	build_touch_log& log = build_touches();
	std::lock_guard _(log.mutex);
	log.stamps[config::path_id(file)] = mtime;
}

auto gse::ide::build_runner::is_build_touch(const std::filesystem::path& file, const std::int64_t mtime) -> bool {
	build_touch_log& log = build_touches();
	std::lock_guard _(log.mutex);
	const auto found = log.stamps.find(config::path_id(file));
	return found != log.stamps.end() && found->second == mtime;
}

auto gse::ide::build_runner::is_inside(const std::filesystem::path& file, const std::filesystem::path& root) -> bool {
	if (root.empty()) {
		return false;
	}
	const std::filesystem::path relative = file.lexically_relative(root);
	return !relative.empty() && *relative.begin() != "..";
}

auto gse::ide::build_runner::build_key(const config::worktree& tree, const std::string_view target) -> id {
	return generate_temp_id(stable_id(std::format("{}/{}", tree.name, target)));
}

auto gse::ide::build_runner::build_keys_for(const std::filesystem::path& file) -> std::vector<id> {
	if (!is_build_source(file)) {
		return {};
	}

	std::vector<id> keys;
	if (is_inside(file, config::source_dir())) {
		keys.push_back(build_key(config::primary(), config::editor_target));
		return keys;
	}
	for (const config::worktree& tree : config::worktrees()) {
		if (is_inside(file, tree.engine_source) || is_inside(file, tree.project_source)) {
			keys.push_back(build_key(tree, tree.game_target));
		}
	}
	return keys;
}

auto gse::ide::build_runner::source_state_path() -> std::filesystem::path {
	return gse::config::cache_dir() / "source_state.bin";
}

auto gse::ide::build_runner::build_state_path() -> std::filesystem::path {
	return gse::config::cache_dir() / "build_state.bin";
}

auto gse::ide::build_runner::build_times() -> std::unordered_map<id, std::int64_t> {
	std::ifstream in(build_state_path(), std::ios::binary);
	if (!in) {
		return {};
	}
	binary_reader reader(in);
	std::uint32_t magic = 0;
	std::uint32_t version = 0;
	std::uint32_t epoch = 0;
	reader & magic & version & epoch;
	if (!in || magic != build_state_magic || version != build_state_version || epoch != archive_format_epoch) {
		return {};
	}
	std::unordered_map<id, std::int64_t> times;
	reader & times;
	if (!in) {
		return {};
	}
	return times;
}

auto gse::ide::build_runner::record_build_time(const id key, const std::int64_t built_at) -> void {
	std::unordered_map<id, std::int64_t> times = build_times();
	times[key] = built_at;

	const std::filesystem::path path = build_state_path();
	std::filesystem::path temp = path;
	temp += ".tmp";
	{
		std::ofstream out(temp, std::ios::binary | std::ios::trunc);
		if (!out) {
			return;
		}
		binary_writer writer(out, build_state_magic, build_state_version);
		writer & times;
	}
	std::error_code ec;
	std::filesystem::rename(temp, path, ec);
}

auto gse::ide::build_runner::source_snapshot_time() -> std::int64_t {
	return static_cast<std::int64_t>(std::filesystem::file_time_type::clock::now().time_since_epoch().count());
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

auto gse::ide::build_runner::refresh_changed_sources(spawn::output_stream& stream, const config::worktree& tree, const std::string_view target) -> void {
	const std::filesystem::path state_path = source_state_path();
	std::error_code exists_ec;
	const bool seeding = !std::filesystem::exists(state_path, exists_ec);
	const std::unordered_map<std::string, source_fingerprint> previous = load_source_state(state_path);
	begin_build_touches();

	std::unordered_map<std::string, source_fingerprint> current = previous;
	std::size_t refreshed = 0;

	for (const std::filesystem::path& source_root : project_source_roots(tree, target)) {
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
					note_build_touch(entry.path(), fingerprint.mtime);
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

auto gse::ide::build_runner::retarget_build_path(const std::filesystem::path& path, const std::filesystem::path& from, const std::filesystem::path& to) -> std::filesystem::path {
	if (path == from) {
		return to;
	}
	if (path.empty() || !is_inside(path, from)) {
		return path;
	}
	return gse::config::generic(to / path.lexically_relative(from));
}

auto gse::ide::build_runner::worktree_for_config(const config::worktree& tree, const std::string_view name) -> config::worktree {
	if (name.empty() || name == active_build_config()) {
		return tree;
	}

	const build_config* selected = build_config_for(name);
	if (selected == nullptr) {
		return tree;
	}

	const std::filesystem::path& from = tree.project_build;
	const std::filesystem::path to = gse::config::generic(from.parent_path() / selected->directory);
	if (from.empty() || from == to) {
		return tree;
	}

	config::worktree out = tree;
	out.project_build = to;
	out.project_output = retarget_build_path(tree.project_output, from, to);
	out.project_compile_commands = retarget_build_path(tree.project_compile_commands, from, to);
	out.game_executable = retarget_build_path(tree.game_executable, from, to);
	return out;
}

auto gse::ide::build_runner::engine_build_dir_for_config(const config::worktree& tree, const std::string_view name) -> std::filesystem::path {
	const std::filesystem::path& editor_build = config::build_dir();
	if (name.empty() || name == active_build_config()) {
		return editor_build;
	}

	const build_config* selected = build_config_for(name);
	if (selected == nullptr) {
		return editor_build;
	}
	if (selected->source_relative.empty()) {
		return gse::config::generic(editor_build.parent_path() / selected->directory);
	}
	return gse::config::generic(tree.engine_root / selected->source_relative);
}

auto gse::ide::build_runner::preset_configure_command(const std::filesystem::path& engine_dir, const std::string_view name) -> std::wstring {
	return L"cmd.exe /c cmake -S \"" + engine_dir.wstring() + L"\" --preset " + std::wstring(name.begin(), name.end());
}

auto gse::ide::build_runner::ensure_engine_configured(spawn::output_stream& stream, const config::worktree& tree, const std::filesystem::path& build_dir, const std::string_view name) -> bool {
	if (!find_build_dir(build_dir).empty()) {
		return true;
	}

	if (name.empty() || name == active_build_config()) {
		spawn::emit(stream, "the editor's own build tree at " + build_dir.generic_display_string() + " is not configured");
		return false;
	}

	if (build_config_for(name) == nullptr) {
		spawn::emit(stream, "no build configuration named '" + std::string(name) + "' is defined in CMakePresets.json");
		return false;
	}

	spawn::emit(stream, "configuring the " + std::string(name) + " build tree; the first build of a configuration compiles everything");
	const std::filesystem::path compiler_bin = compiler_bin_dir(config::build_dir());
	if (spawn::run_capture(stream, preset_configure_command(tree.engine_root, name), tree.engine_root.wstring(), compiler_bin) != 0) {
		spawn::emit(stream, "configure failed");
		return false;
	}
	return !find_build_dir(build_dir).empty();
}

auto gse::ide::build_runner::configure_command(const config::worktree& tree, const std::filesystem::path& project_dir, const std::filesystem::path& build_dir, const std::filesystem::path& inherit_from) -> std::wstring {
	std::wstring command = L"cmd.exe /c cmake -G Ninja -S \"" + project_dir.wstring() + L"\" -B \"" + build_dir.wstring() + L"\"";
	command += L" -DGSE_ENGINE_DIR=\"" + tree.engine_root.wstring() + L"\"";
	command += L" -DVCPKG_MANIFEST_MODE=OFF";
	command += L" -DVCPKG_INSTALLED_DIR=\"" + (inherit_from / "vcpkg_installed").wstring() + L"\"";

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
		const std::string value = cache_value(inherit_from, key);
		if (value.empty()) {
			continue;
		}
		const std::wstring wide_key(key.begin(), key.end());
		command += L" -D" + wide_key + L"=\"" + std::filesystem::path(value).wstring() + L"\"";
	}

	return command;
}

auto gse::ide::build_runner::ensure_configured(spawn::output_stream& stream, const config::worktree& tree, const std::string_view name) -> std::filesystem::path {
	const std::filesystem::path& build_dir = tree.project_build;
	std::error_code ec;

	if (is_inside(tree.project_root, tree.engine_root)) {
		return ensure_engine_configured(stream, tree, build_dir, name) ? find_build_dir(build_dir) : std::filesystem::path{};
	}

	if (!find_build_dir(build_dir).empty()) {
		const std::string bound = cache_value(build_dir, "GSE_ENGINE_DIR");
		const std::string expected = tree.engine_root.generic_native_encoded_string();
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

	const std::filesystem::path& project_dir = tree.project_root;
	if (!std::filesystem::exists(project_dir / "CMakeLists.txt", ec)) {
		return {};
	}

	const std::filesystem::path engine_build = engine_build_dir_for_config(tree, name);
	if (!ensure_engine_configured(stream, tree, engine_build, name)) {
		return {};
	}

	spawn::emit(stream, "configuring " + project_dir.generic_display_string() + "...");
	std::filesystem::create_directories(build_dir, ec);

	const std::filesystem::path compiler_bin = compiler_bin_dir(engine_build);
	if (spawn::run_capture(stream, configure_command(tree, project_dir, build_dir, engine_build), project_dir.wstring(), compiler_bin) != 0) {
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

auto gse::ide::build_runner::sweep_backups(const std::filesystem::path& executable) -> std::size_t {
	const std::filesystem::path dir = executable.parent_path();
	std::error_code ec;
	if (dir.empty() || !std::filesystem::exists(dir, ec)) {
		return 0;
	}

	const std::string prefix = executable.filename().generic_display_string() + ".";
	std::size_t held = 0;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec)) {
		const std::string name = entry.path().filename().generic_display_string();
		if (!name.starts_with(prefix) || !name.ends_with(backup_suffix)) {
			continue;
		}
		std::error_code remove_ec;
		if (!std::filesystem::remove(entry.path(), remove_ec)) {
			++held;
		}
	}
	return held;
}

auto gse::ide::build_runner::next_backup_path(const std::filesystem::path& executable) -> std::filesystem::path {
	std::error_code ec;
	for (std::uint32_t index = 1; index <= max_backups; ++index) {
		std::filesystem::path candidate = executable;
		candidate += std::format(".{}{}", index, backup_suffix);
		if (!std::filesystem::exists(candidate, ec)) {
			return candidate;
		}
	}

	std::filesystem::path exhausted = executable;
	exhausted += std::format(".{}{}", max_backups, backup_suffix);
	return exhausted;
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

auto gse::ide::build_runner::watch_editor_image(data& d) -> void {
	const time now = system_clock::now<time>();
	if (now < d.next_image_poll) {
		return;
	}
	d.next_image_poll = now + seconds(1.f);

	const std::filesystem::path editor_exe = current_executable();
	if (editor_exe.empty()) {
		log::println(log::level::warning, log::category::general, "editor watch: could not resolve this instance's executable path");
		return;
	}

	std::error_code ec;
	const std::filesystem::file_time_type stamp = std::filesystem::last_write_time(editor_exe, ec);
	if (ec) {
		if (!d.editor_image_missing) {
			d.editor_image_missing = true;
			log::println(log::level::info, log::category::general, "editor watch: '{}' is unavailable ({}); a rebuild is probably in flight", editor_exe, ec.message());
		}
		return;
	}
	if (d.editor_image_missing) {
		d.editor_image_missing = false;
		log::println(log::level::info, log::category::general, "editor watch: '{}' is back", editor_exe);
	}
	const auto stamp_ticks = static_cast<std::int64_t>(stamp.time_since_epoch().count());

	if (!d.editor_image_time) {
		d.editor_image_time = stamp;
		d.editor_image_reported = stamp_ticks;
		log::println(log::level::info, log::category::general, "editor watch: watching '{}' (image {})", editor_exe, stamp_ticks);
		return;
	}

	if (stamp <= *d.editor_image_time) {
		if (d.editor_image_reported != stamp_ticks) {
			d.editor_image_reported = stamp_ticks;
			log::println(log::level::info, log::category::general, "editor watch: '{}' went back to image {}; the rebuild must have failed", editor_exe, stamp_ticks);
		}
		return;
	}

	if (d.editor_image_reported != stamp_ticks) {
		d.editor_image_reported = stamp_ticks;
		d.editor_image_waiting = false;
		log::println(
			log::level::info,
			log::category::general,
			"editor watch: '{}' image {} -> {}, building {}, session {}, relaunching {}",
			editor_exe,
			static_cast<std::int64_t>(d.editor_image_time->time_since_epoch().count()),
			stamp_ticks,
			d.building,
			d.sessions[0].generation != 0,
			app::relaunch_pending()
		);
		return;
	}

	const bool readable = image_readable(editor_exe);
	if (!readable || d.building || d.sessions[0].generation != 0 || app::relaunch_pending()) {
		if (!d.editor_image_waiting) {
			d.editor_image_waiting = true;
			log::println(
				log::level::info,
				log::category::general,
				"editor watch: holding the restart of '{}' (readable {}, building {}, session {}, relaunching {})",
				editor_exe,
				readable,
				d.building,
				d.sessions[0].generation != 0,
				app::relaunch_pending()
			);
		}
		return;
	}

	log::println(log::level::info, log::category::general, "editor watch: '{}' was rebuilt by another instance; restarting", editor_exe);
	app::relaunch_self_on_exit();
	gse::shutdown();
}

auto gse::ide::build_runner::image_readable(const std::filesystem::path& file) -> bool {
	void* handle = win32::CreateFileW(file.wstring().c_str(), win32::generic_read, win32::file_share_read, nullptr, win32::open_existing, win32::file_attribute_normal, nullptr);
	if (!win32::valid_handle(handle)) {
		return false;
	}
	win32::CloseHandle(handle);
	return true;
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

auto gse::ide::build_runner::parse_diagnostic_site(std::string_view text, const std::filesystem::path& build_dir) -> std::optional<diagnostic_site> {
	while (!text.empty() && (text.back() == ':' || text.back() == ',' || text.back() == ' ')) {
		text.remove_suffix(1);
	}
	while (!text.empty() && text.front() == ' ') {
		text.remove_prefix(1);
	}

	const auto trailing_number = [](std::string_view& head) -> std::optional<std::uint32_t> {
		const std::size_t colon = head.rfind(':');
		if (colon == std::string_view::npos || colon + 1 == head.size()) {
			return std::nullopt;
		}
		const std::string_view digits = head.substr(colon + 1);
		if (!std::ranges::all_of(digits, [](const unsigned char ch) { return std::isdigit(ch) != 0; })) {
			return std::nullopt;
		}
		std::uint32_t value = 0;
		if (std::from_chars(digits.data(), digits.data() + digits.size(), value).ec != std::errc{}) {
			return std::nullopt;
		}
		head = head.substr(0, colon);
		return value;
	};

	std::optional<std::uint32_t> last = trailing_number(text);
	if (!last) {
		return std::nullopt;
	}
	if (const std::optional<std::uint32_t> earlier = trailing_number(text)) {
		last = earlier;
	}

	std::filesystem::path file(text);
	if (file.empty() || !is_build_source(file)) {
		return std::nullopt;
	}
	if (file.is_relative()) {
		file = build_dir / file;
	}
	return diagnostic_site{
		.file = std::filesystem::absolute(file).lexically_normal(),
		.line = *last,
	};
}

auto gse::ide::build_runner::collect_error_sites(const std::span<const std::string> lines, const std::filesystem::path& build_dir) -> std::vector<build_error> {
	constexpr std::size_t max_errors = 32;
	constexpr std::size_t max_notes = 4;
	constexpr std::string_view error_marker = ": error: ";
	constexpr std::string_view fatal_marker = ": fatal error: ";
	constexpr std::string_view note_marker = ": note: ";
	constexpr std::string_view included_marker = "In file included from ";
	constexpr std::string_view imported_marker = "In module imported at ";
	constexpr std::string_view chain_marker = "from ";

	std::vector<build_error> errors;
	std::vector<std::filesystem::path> context;

	for (const std::string& line : lines) {
		std::string_view text = line;
		while (!text.empty() && text.front() == ' ') {
			text.remove_prefix(1);
		}

		if (text.starts_with(included_marker) || text.starts_with(imported_marker)) {
			context.clear();
			const std::size_t marker = text.starts_with(included_marker) ? included_marker.size() : imported_marker.size();
			if (const std::optional<diagnostic_site> site = parse_diagnostic_site(text.substr(marker), build_dir)) {
				context.push_back(site->file);
			}
			continue;
		}
		if (text.starts_with(chain_marker)) {
			if (const std::optional<diagnostic_site> site = parse_diagnostic_site(text.substr(chain_marker.size()), build_dir)) {
				context.push_back(site->file);
			}
			continue;
		}

		const std::size_t error_at = text.find(error_marker);
		const std::size_t fatal_at = text.find(fatal_marker);
		const std::size_t note_at = text.find(note_marker);
		const std::size_t head_end = std::min({ error_at, fatal_at, note_at });
		if (head_end == std::string_view::npos) {
			continue;
		}

		const std::size_t marker_size = head_end == fatal_at
			? fatal_marker.size()
			: head_end == error_at ? error_marker.size() : note_marker.size();

		const std::optional<diagnostic_site> site = parse_diagnostic_site(text.substr(0, head_end), build_dir);
		if (!site) {
			continue;
		}

		if (head_end == note_at) {
			if (errors.empty()) {
				continue;
			}
			build_error& owner = errors.back();
			if (owner.file != site->file && !std::ranges::contains(owner.related, site->file)) {
				owner.related.push_back(site->file);
			}
			if (owner.notes.size() < max_notes) {
				owner.notes.push_back(std::format("{}:{}: note: {}", site->file.generic_display_string(), site->line, text.substr(head_end + marker_size)));
			}
			continue;
		}

		if (errors.size() >= max_errors) {
			continue;
		}

		std::vector<std::filesystem::path> related = context;
		std::erase(related, site->file);
		errors.push_back({
			.file = site->file,
			.line = site->line,
			.message = std::string(text.substr(head_end + marker_size)),
			.related = std::move(related),
		});
	}

	return errors;
}

auto gse::ide::build_runner::analysis_pause_state() -> std::atomic<bool>& {
	static std::atomic<bool> paused = false;
	return paused;
}

auto gse::ide::build_runner::analysis_busy_state() -> std::atomic<bool>& {
	static std::atomic<bool> busy = false;
	return busy;
}

auto gse::ide::build_runner::request_analysis_pause(const bool paused) -> void {
	analysis_pause_state().store(paused, std::memory_order_release);
}

auto gse::ide::build_runner::analysis_pause_requested() -> bool {
	return analysis_pause_state().load(std::memory_order_acquire);
}

auto gse::ide::build_runner::report_analysis_busy(const bool busy) -> void {
	analysis_busy_state().store(busy, std::memory_order_release);
}

auto gse::ide::build_runner::clear_stale_module_file(spawn::output_stream& stream, const std::filesystem::path& file) -> bool {
	constexpr int clear_attempts = 40;
	constexpr std::chrono::milliseconds clear_retry_delay(250);

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

	spawn::emit(stream, "could not clear stale module cache file " + file.generic_display_string() + " after " + std::to_string(clear_attempts) + " attempts: " + ec.message());
	return false;
}

auto gse::ide::build_runner::collect_dependency_cycle(const std::span<const std::string> lines) -> std::string {
	for (const std::string& line : lines) {
		const std::size_t marker = line.find(dependency_cycle_signature);
		if (marker == std::string::npos) {
			continue;
		}

		const std::string tail = line.substr(marker + dependency_cycle_signature.size());
		const std::size_t begin = tail.find_first_not_of(" \t");
		const std::size_t end = tail.find_last_not_of(" \t\r.");
		if (begin == std::string::npos || end == std::string::npos || end < begin) {
			return {};
		}
		return tail.substr(begin, end - begin + 1);
	}
	return {};
}

auto gse::ide::build_runner::clear_dependency_log(spawn::output_stream& stream, const std::filesystem::path& build_dir) -> bool {
	constexpr int clear_attempts = 20;
	constexpr std::chrono::milliseconds clear_retry_delay(100);

	const std::filesystem::path deps = build_dir / ninja_deps_name;

	std::error_code ec;
	if (!std::filesystem::exists(deps, ec)) {
		spawn::emit(stream, "there is no ninja dependency log to clear, so the cycle is in the module sources themselves");
		return false;
	}

	for (int attempt = 0; attempt < clear_attempts; ++attempt) {
		ec.clear();
		if (std::filesystem::remove(deps, ec)) {
			spawn::emit(stream, "cleared the stale ninja dependency log " + deps.generic_display_string());
			spawn::emit(stream, "the retry rescans every module, so this build runs long");
			return true;
		}
		if (!ec) {
			return false;
		}
		std::this_thread::sleep_for(clear_retry_delay);
	}

	spawn::emit(stream, "could not clear " + deps.generic_display_string() + " after " + std::to_string(clear_attempts) + " attempts: " + ec.message());
	return false;
}

auto gse::ide::build_runner::run_build_with_module_recovery(
	const std::stop_token& st,
	spawn::output_stream& stream,
	const std::wstring& command,
	const std::filesystem::path& source_dir,
	const std::filesystem::path& build_dir,
	const std::filesystem::path& compiler_bin
) -> build_outcome {
	constexpr int max_attempts = 16;
	constexpr int analysis_wait_polls = 400;
	constexpr std::chrono::milliseconds analysis_poll_delay(50);

	for (int waited = 0; waited < analysis_wait_polls; ++waited) {
		if (!analysis_busy_state().load(std::memory_order_acquire) || st.stop_requested() || stream.terminated.load(std::memory_order_acquire)) {
			break;
		}
		if (waited == 0) {
			spawn::emit(stream, "waiting for semantic analysis to release compiled modules...");
		}
		std::this_thread::sleep_for(analysis_poll_delay);
	}

	std::unordered_set<std::string> recovered;
	for (int attempt = 1; ; ++attempt) {
		spawn::begin_transcript(stream);
		const int code = spawn::run_capture(stream, command, source_dir.wstring(), compiler_bin);
		const std::vector<std::string> transcript = spawn::take_transcript(stream);
		const auto finish = [&]() -> build_outcome {
			return {
				.code = code,
				.errors = code == 0 ? std::vector<build_error>{} : collect_error_sites(transcript, build_dir),
			};
		};

		if (code == 0 || attempt >= max_attempts || st.stop_requested() || stream.terminated.load(std::memory_order_acquire)) {
			return finish();
		}

		if (const std::string cycle = collect_dependency_cycle(transcript); !cycle.empty()) {
			const std::string key = (build_dir / ninja_deps_name).generic_native_encoded_string();
			if (recovered.contains(key)) {
				spawn::emit(stream, "the dependency cycle survived clearing the ninja dependency log, so it is a real module cycle: " + cycle);
				return finish();
			}

			spawn::emit(stream, "ninja reported a dependency cycle: " + cycle);
			if (!clear_dependency_log(stream, build_dir)) {
				return finish();
			}

			recovered.insert(key);
			continue;
		}

		const std::vector<std::filesystem::path> conflicts = collect_module_write_conflicts(transcript, build_dir);
		const std::vector<std::filesystem::path> locked = collect_locked_output_copies(transcript, build_dir);
		if (conflicts.empty() && locked.empty()) {
			return finish();
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
			return finish();
		}

		if (cleared > 0) {
			spawn::emit(stream, "cleared " + std::to_string(cleared) + " stale module cache file(s); retrying build");
		}
		if (displaced > 0) {
			spawn::emit(stream, "displaced " + std::to_string(displaced) + " locked runtime file(s); retrying build");
		}
	}
}

auto gse::ide::build_runner::launch_game_attached(build_completion& completion, spawn::output_stream& stream, const config::worktree& tree, const std::uint32_t generation, const std::uint32_t instance, std::string label, const std::wstring& extra_args) -> void {
	const std::filesystem::path& game_exe = tree.game_executable;
	std::error_code ec;
	if (!std::filesystem::exists(game_exe, ec)) {
		spawn::emit(stream, "game executable not found: " + game_exe.generic_display_string());
		return;
	}

	const win32::DWORD editor_pid = win32::GetCurrentProcessId();
	const std::string pipe_name = "\\\\.\\pipe\\gse_editor_" + std::to_string(editor_pid) + "_" + std::to_string(generation) + "_" + std::to_string(instance);
	const std::wstring wide_pipe(pipe_name.begin(), pipe_name.end());
	const std::filesystem::path graph_file = std::filesystem::temp_directory_path() / std::format("gse_editor_game_graph_{}_{}_{}.bin", editor_pid, generation, instance);

	void* pipe = win32::CreateNamedPipeW(wide_pipe.c_str(), win32::pipe_access_duplex, win32::pipe_type_byte | win32::pipe_nowait, 1, sizeof(attached_input_message) * 512, sizeof(attached_surface_message) * 2, 0, nullptr);
	if (!win32::valid_handle(pipe)) {
		spawn::emit(stream, "failed to create editor pipe");
		return;
	}

	const std::string_view backend_name = enum_to_string(gpu::active_backend);
	const std::wstring wide_backend(backend_name.begin(), backend_name.end());

	std::wstring command = L"\"" + game_exe.wstring() + L"\"";
	command += L" --engine-attached";
	command += L" --engine-ipc-pipe-name " + wide_pipe;
	command += L" --engine-parent-pid " + std::to_wstring(editor_pid);
	command += L" --engine-setting Graphics.backend=" + wide_backend;
	if (instance == 0) {
		command += L" --engine-dump-system-graph-path \"" + graph_file.wstring() + L"\"";
	}
	command += extra_args;

	const spawn::launched game = spawn::launch_streamed(command, tree.project_root.wstring());
	if (!win32::valid_handle(game.process)) {
		win32::CloseHandle(pipe);
		spawn::emit(stream, "failed to launch " + label);
		return;
	}

	if (win32::valid_handle(game.input)) {
		win32::CloseHandle(game.input);
	}

	spawn::emit(stream, "launched " + label + " (pid " + std::to_string(game.pid) + ")");

	std::lock_guard _(completion.mutex);
	completion.game_launched = true;
	if (instance == 0) {
		completion.graph_path = graph_file;
	}
	completion.children.push_back({
		.instance = instance,
		.process = game.process,
		.job = game.job,
		.output = game.output,
		.pid = game.pid,
		.surface_pipe = pipe,
		.attached = true,
		.label = std::move(label),
	});
}

auto gse::ide::build_runner::launch_child(build_completion& completion, spawn::output_stream& stream, const config::worktree& tree, std::string label, const std::wstring& extra_args) -> void {
	const std::filesystem::path& game_exe = tree.game_executable;
	std::error_code ec;
	if (!std::filesystem::exists(game_exe, ec)) {
		spawn::emit(stream, "game executable not found: " + game_exe.generic_display_string());
		return;
	}

	const std::wstring command = L"\"" + game_exe.wstring() + L"\"" + extra_args;

	const spawn::launched child = spawn::launch_streamed(command, tree.project_root.wstring());
	if (!win32::valid_handle(child.process)) {
		spawn::emit(stream, "failed to launch " + label);
		return;
	}

	if (win32::valid_handle(child.input)) {
		win32::CloseHandle(child.input);
	}

	spawn::emit(stream, "launched " + label + " (pid " + std::to_string(child.pid) + ")");

	std::lock_guard _(completion.mutex);
	completion.children.push_back({
		.process = child.process,
		.job = child.job,
		.output = child.output,
		.pid = child.pid,
		.label = std::move(label),
	});
}

auto gse::ide::build_runner::launch_play_session(build_completion& completion, spawn::output_stream& stream, const config::worktree& tree, const play_session& session, const std::uint32_t generation) -> void {
	const std::uint32_t clients = std::min<std::uint32_t>(std::max<std::uint32_t>(session.clients, 1), max_attached_instances);
	const std::uint32_t processes = clients + (session.dedicated_server ? 1u : 0u) + 1u;
	const std::uint32_t hardware = std::max<std::uint32_t>(2, std::thread::hardware_concurrency());
	const std::wstring pool_args = L" --engine-worker-threads " + std::to_wstring(std::max<std::uint32_t>(2, hardware / processes));

	std::wstring connect_args = pool_args;

	if (session.dedicated_server) {
		launch_child(
			completion,
			stream,
			tree,
			"server",
			L" --engine-net-role dedicated --engine-net-listen-port " + std::to_wstring(session.base_port) + pool_args
		);
		connect_args += L" --engine-net-connect 127.0.0.1:" + std::to_wstring(session.base_port);
	}

	for (std::uint32_t i = 0; i < clients; ++i) {
		auto label = std::format("client {}", i + 1);
		if (session.attached) {
			launch_game_attached(completion, stream, tree, generation, i, std::move(label), connect_args);
		}
		else {
			launch_child(completion, stream, tree, std::move(label), connect_args);
		}
	}
}

auto gse::ide::build_runner::build_game(
	const std::stop_token& st,
	build_completion& completion,
	spawn::output_stream& stream,
	const config::worktree& tree,
	const build_request& request,
	const std::uint32_t next_generation
) -> void {
	const std::filesystem::path build_dir = ensure_configured(stream, tree, request.config);
	if (build_dir.empty()) {
		spawn::emit(stream, "configured build directory is unavailable");
		return;
	}

	refresh_changed_sources(stream, tree, tree.game_target);
	const std::int64_t snapshot = source_snapshot_time();
	const std::filesystem::path compiler_bin = compiler_bin_dir(build_dir);

	const std::filesystem::path& game_exe = tree.game_executable;
	const std::size_t still_running = sweep_backups(game_exe);
	const std::filesystem::path backup = next_backup_path(game_exe);
	std::error_code ec;
	if (std::filesystem::exists(game_exe, ec)) {
		if (still_running > 0) {
			spawn::emit(stream, std::format("{} earlier build(s) are still running; they keep their own copy and are cleaned up once they exit", still_running));
		}

		ec.clear();
		std::filesystem::rename(game_exe, backup, ec);
		if (ec) {
			spawn::emit(stream, "could not rename " + game_exe.generic_display_string() + " to " + backup.filename().generic_display_string() + ": " + ec.message());
			spawn::emit(stream, "something has the game executable locked for writing, usually an attached debugger or antivirus scanning it, then build again");
			return;
		}
	}

	spawn::emit(stream, "building " + tree.game_target + "...");
	build_outcome outcome = run_build_with_module_recovery(st, stream, build_command(build_dir, tree.game_target), tree.project_root, build_dir, compiler_bin);
	if (outcome.code != 0) {
		spawn::emit(stream, "build failed (exit " + std::to_string(outcome.code) + ")");
		{
			std::lock_guard _(completion.mutex);
			completion.errors = std::move(outcome.errors);
		}
		if (std::filesystem::exists(backup, ec)) {
			std::filesystem::remove(game_exe, ec);
			std::filesystem::rename(backup, game_exe, ec);
		}
		return;
	}

	spawn::emit(stream, "build succeeded");
	record_build_time(build_key(tree, tree.game_target), snapshot);
	std::filesystem::remove(backup, ec);
	{
		std::lock_guard _(completion.mutex);
		completion.generation = next_generation;
		completion.succeeded = true;
	}

	if (request.run_after && !st.stop_requested()) {
		launch_play_session(completion, stream, tree, request.session, next_generation);
	}
}

auto gse::ide::build_runner::rebuild_editor(const std::stop_token& st, build_completion& completion, spawn::output_stream& stream) -> void {
	const std::filesystem::path build_dir = find_build_dir(config::build_dir());
	if (build_dir.empty()) {
		spawn::emit(stream, "configured build directory is unavailable");
		return;
	}

	refresh_changed_sources(stream, config::primary(), config::editor_target);
	const std::int64_t snapshot = source_snapshot_time();
	const std::filesystem::path compiler_bin = compiler_bin_dir(build_dir);

	const std::filesystem::path editor_exe = current_executable();
	if (editor_exe.empty()) {
		spawn::emit(stream, "could not resolve editor executable path");
		return;
	}

	const std::size_t still_running = sweep_backups(editor_exe);
	const std::filesystem::path backup = next_backup_path(editor_exe);
	std::error_code ec;
	if (still_running > 0) {
		spawn::emit(stream, std::format("{} earlier editor image(s) are still mapped; they are cleaned up once those instances exit", still_running));
	}

	std::filesystem::rename(editor_exe, backup, ec);
	if (ec) {
		spawn::emit(stream, "could not rename " + editor_exe.generic_display_string() + " to " + backup.filename().generic_display_string() + ": " + ec.message());
		spawn::emit(stream, "something has the editor executable locked for writing, usually an attached debugger or antivirus scanning it, then rebuild");
		return;
	}

	spawn::emit(stream, "rebuilding editor...");
	build_outcome outcome = run_build_with_module_recovery(st, stream, build_command(build_dir, config::editor_target), gse::config::root_dir(), build_dir, compiler_bin);
	if (outcome.code != 0) {
		spawn::emit(stream, "rebuild failed (exit " + std::to_string(outcome.code) + "); restoring previous editor");
		{
			std::lock_guard _(completion.mutex);
			completion.errors = std::move(outcome.errors);
		}
		std::filesystem::remove(editor_exe, ec);
		std::filesystem::rename(backup, editor_exe, ec);
		return;
	}

	record_build_time(build_key(config::primary(), config::editor_target), snapshot);

	if (st.stop_requested()) {
		return;
	}

	{
		std::lock_guard _(completion.mutex);
		completion.succeeded = true;
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
	const config::worktree& base = request.tree ? *request.tree : config::primary();
	const bool editor = request.target == build_target::editor;
	{
		std::lock_guard _(completion->mutex);
		completion->key = editor
			? build_key(config::primary(), config::editor_target)
			: build_key(base, base.game_target);
		completion->kind = editor ? stream_kind::build_editor : stream_kind::build_game;
	}

	if (editor) {
		if (!request.config.empty() && request.config != active_build_config()) {
			spawn::emit(*stream, "the editor always rebuilds in the configuration it is running, so '" + request.config + "' was ignored");
		}
		rebuild_editor(st, *completion, *stream);
	}
	else {
		const config::worktree tree = worktree_for_config(base, request.config);
		if (!request.config.empty() && request.config != active_build_config()) {
			spawn::emit(*stream, "building in " + std::string(build_config_label(request.config)) + " (" + tree.project_build.generic_display_string() + ")");
		}
		build_game(st, *completion, *stream, tree, request, next_generation);
	}

	spawn::close_process(*stream);

	std::lock_guard _(completion->mutex);
	completion->done = true;
}

auto gse::ide::build_runner::cleanup_backups() -> void {
	for (const config::worktree& tree : config::worktrees()) {
		(void)sweep_backups(tree.game_executable);
	}
	(void)sweep_backups(config::editor_executable());
	const std::filesystem::path editor_exe = current_executable();
	if (!editor_exe.empty()) {
		(void)sweep_backups(editor_exe);
	}
}

auto gse::ide::build_runner::start_build(const channel_write<attached_session_ended, stream_opened, build_finished, attached_surface_ready, attached_fatal_reported> events_out, data& d, const build_request& request) -> void {
	if (d.building) {
		return;
	}

	d.session_error.clear();

	build_request resolved = request;
	std::string profile_note;
	if (!resolved.profile.empty()) {
		if (const build_profile* named = profile_for(d.profiles, resolved.profile)) {
			resolved.config = named->config;
			resolved.session = named->session;
		}
		else {
			profile_note = "no build profile named '" + resolved.profile + "'; building with the editor's own configuration and one attached client";
			log::println(log::level::warning, log::category::task, "{}", profile_note);
		}
	}

	// Windows keeps the image mapped while the process lives, so the linker cannot
	// overwrite the game exe until every running instance has actually exited.
	if (resolved.target == build_target::game) {
		if (const std::optional<std::uint32_t> ended = stop_games(d)) {
			events_out.push<attached_session_ended>({
				.generation = *ended,
			});
		}
		if (resolved.run_after) {
			d.session = resolved.session;
		}
	}

	std::string name = resolved.target == build_target::editor
		? "Rebuild Editor"
		: resolved.run_after ? "Build & Run" : "Build Game";
	if (resolved.target == build_target::game && !resolved.config.empty() && resolved.config != active_build_config()) {
		name += " [" + std::string(build_config_label(resolved.config)) + "]";
	}
	if (resolved.tree && resolved.tree != &config::primary()) {
		name += " (" + resolved.tree->name + ")";
	}

	auto stream = std::make_shared<spawn::output_stream>();
	stream->running.store(true, std::memory_order_release);
	if (!profile_note.empty()) {
		spawn::emit(*stream, profile_note);
	}
	events_out.push<stream_opened>({
		.name = name,
		.kind = resolved.target == build_target::editor ? stream_kind::build_editor : stream_kind::build_game,
		.stream = stream,
	});

	{
		std::lock_guard _(d.completion.mutex);
		d.completion.done = false;
		d.completion.succeeded = false;
		d.completion.game_launched = false;
		d.completion.generation = 0;
		d.completion.graph_path.clear();
		d.completion.key = {};
		d.completion.kind = stream_kind::none;
		d.completion.errors.clear();
		d.completion.children.clear();
	}

	request_analysis_pause(true);

	d.building = true;
	d.building_session = resolved.target == build_target::game && resolved.run_after && resolved.session.attached;
	d.inbox_id = resolved.inbox_id;
	d.active_stream = stream;
	d.worker = task::spawn(
		log::thread_role::build,
		[completion = &d.completion, stream = std::move(stream), resolved = std::move(resolved), generation = d.game_generation + 1](const std::stop_token& st) mutable {
			build_worker(st, completion, std::move(stream), std::move(resolved), generation);
		}
	);
}

auto gse::ide::build_runner::drain_completion(const channel_write<attached_session_ended, stream_opened, build_finished, attached_surface_ready, attached_fatal_reported> events_out, data& d) -> void {
	if (!d.building) {
		return;
	}
	{
		std::lock_guard _(d.completion.mutex);
		if (!d.completion.done) {
			return;
		}
	}
	if (d.worker.joinable()) {
		d.worker.join();
	}
	d.building = false;
	d.building_session = false;
	request_analysis_pause(false);

	if (d.completion.generation != 0) {
		d.game_generation = d.completion.generation;
	}
	if (d.completion.game_launched) {
		d.game_graph_path = std::move(d.completion.graph_path);
		for (attached_game& game : d.games) {
			game.owns_pipe = false;
		}
	}

	for (launched_child& child : d.completion.children) {
		if (child.attached && child.instance < max_attached_instances) {
			close_surface_pipe(d, child.instance);
			d.pipes[child.instance].generation = d.completion.generation;
			d.pipes[child.instance].instance = child.instance;
			d.pipes[child.instance].handle = child.surface_pipe;
			d.sessions[child.instance] = attached_session{
				.generation = d.completion.generation,
				.instance = child.instance,
				.status = attached_session_status::awaiting_surface,
			};
		}

		auto child_stream = std::make_shared<spawn::output_stream>();
		child_stream->running.store(true, std::memory_order_release);
		spawn::attach_process(*child_stream, child.process, child.job);
		events_out.push<stream_opened>({
			.name = std::format("{} {}", child.label, child.pid),
			.kind = stream_kind::game,
			.stream = child_stream,
			.attached_instance = child.instance,
		});

		d.games.push_back({
			.generation = d.completion.generation,
			.instance = child.instance,
			.process = child.process,
			.stream = std::move(child_stream),
			.output = child.output,
			.owns_pipe = child.attached,
		});
	}
	d.completion.children.clear();
	d.active_stream.reset();

	events_out.push<build_finished>({
		.key = d.completion.key,
		.kind = d.completion.kind,
		.errors = std::move(d.completion.errors),
		.inbox_id = std::exchange(d.inbox_id, {}),
		.succeeded = d.completion.succeeded,
	});
	d.completion.errors.clear();
}

auto gse::ide::build_runner::poll_games(const channel_write<attached_session_ended, stream_opened, build_finished, attached_surface_ready, attached_fatal_reported> events_out, data& d) -> void {
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

		const bool owned_pipe = game.owns_pipe;
		const std::uint32_t generation = game.generation;
		const std::uint32_t instance = game.instance;

		if (owned_pipe) {
			poll_surface_pipe(events_out, d, instance);
			close_surface_pipe(d, instance);
			if (session_for(d, generation, instance)) {
				d.sessions[instance] = {};
			}
			events_out.push<attached_session_ended>({
				.generation = generation,
				.instance = instance,
			});
		}
		d.games.erase(d.games.begin() + static_cast<std::ptrdiff_t>(i));

		const bool clients_remain = std::ranges::any_of(d.games, [generation](const attached_game& sibling) {
			return sibling.generation == generation && sibling.owns_pipe;
		});

		if (owned_pipe && !clients_remain) {
			for (attached_game& sibling : d.games) {
				if (sibling.generation == generation) {
					spawn::terminate_process(*sibling.stream);
				}
			}
		}
	}
}

auto gse::ide::build_runner::poll_stopping_games(data& d, const bool block_until_exit) -> void {
	if (d.stopping.empty()) {
		return;
	}

	const bool expired = system_clock::now<time>() >= d.stopping_deadline;

	std::erase_if(
		d.stopping,
		[&](attached_game& game) {
			const win32::DWORD timeout = block_until_exit ? 5000 : 0;
			if (!expired && win32::WaitForSingleObject(game.process, timeout) != win32::wait_object_0) {
				return false;
			}
			if (game.output) {
				spawn::pump_output(*game.stream, game.output, game.pending);
				win32::CloseHandle(game.output);
				game.output = nullptr;
			}
			spawn::close_process(*game.stream);
			win32::CloseHandle(game.process);
			return true;
		}
	);
}

auto gse::ide::build_runner::stop_games(data& d) -> std::optional<std::uint32_t> {
	std::optional<std::uint32_t> ended;
	for (const attached_session& session : d.sessions) {
		if (session.generation != 0) {
			ended = session.generation;
			break;
		}
	}
	std::ranges::stable_sort(d.games, [](const attached_game& a, const attached_game& b) {
		return a.owns_pipe && !b.owns_pipe;
	});
	for (attached_game& game : d.games) {
		spawn::terminate_process(*game.stream);
		d.stopping.push_back(std::move(game));
	}
	d.games.clear();
	d.stopping_deadline = system_clock::now<time>() + seconds(5.f);
	for (std::uint32_t instance = 0; instance < max_attached_instances; ++instance) {
		close_surface_pipe(d, instance);
	}
	d.sessions = {};
	return ended;
}

auto gse::ide::build_runner::close_surface_pipe(data& d, const std::uint32_t instance) -> void {
	if (instance >= max_attached_instances) {
		return;
	}
	surface_pipe& pipe = d.pipes[instance];
	if (pipe.handle) {
		win32::DisconnectNamedPipe(pipe.handle);
		win32::CloseHandle(pipe.handle);
	}
	pipe.handle = nullptr;
	pipe.connected = false;
	pipe.handshake_done = false;
	pipe.received = 0;
	pipe.expected = sizeof(std::uint32_t);
	pipe.have_magic = false;
	pipe.generation = 0;
	pipe.pending_tail.clear();
}

template <typename Data>
auto gse::ide::build_runner::session_for(const Data& d, const std::uint32_t generation, const std::uint32_t instance) -> bool {
	return instance < max_attached_instances
		&& d.sessions[instance].generation != 0
		&& d.sessions[instance].generation == generation;
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

auto gse::ide::build_runner::poll_surface_pipe(const channel_write<attached_session_ended, stream_opened, build_finished, attached_surface_ready, attached_fatal_reported> events_out, data& d, const std::uint32_t instance) -> void {
	surface_pipe& pipe = d.pipes[instance];
	if (!pipe.handle) {
		return;
	}

	if (!pipe.connected) {
		if (win32::ConnectNamedPipe(pipe.handle, nullptr) != 0) {
			pipe.connected = true;
		}
		else {
			const win32::DWORD error = win32::GetLastError();
			if (error == win32::error_pipe_connected) {
				pipe.connected = true;
			}
			else if (error != win32::error_pipe_listening) {
				close_surface_pipe(d, instance);
				return;
			}
		}
		if (!pipe.connected) {
			return;
		}
	}

	win32::DWORD available = 0;
	while (win32::PeekNamedPipe(pipe.handle, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
		win32::DWORD read = 0;
		const auto remaining = static_cast<win32::DWORD>(pipe.expected - pipe.received);
		if (!win32::ReadFile(pipe.handle, pipe.bytes.data() + pipe.received, remaining, &read, nullptr)) {
			if (win32::GetLastError() != win32::error_no_data) {
				close_surface_pipe(d, instance);
			}
			return;
		}
		if (read == 0) {
			return;
		}
		pipe.received += read;
		if (pipe.received < pipe.expected) {
			continue;
		}

		std::uint32_t magic = 0;
		std::memcpy(&magic, pipe.bytes.data(), sizeof(magic));

		if (!pipe.have_magic) {
			std::size_t total = 0;
			if (magic == attached_surface_magic) {
				total = sizeof(attached_surface_message);
			}
			else if (magic == attached_fatal_magic) {
				total = sizeof(attached_fatal_message);
			}
			if (total == 0) {
				log::println(log::level::warning, log::category::general, "attached pipe: unknown message magic {:#x} from instance {}; closing pipe", magic, instance);
				close_surface_pipe(d, instance);
				return;
			}
			pipe.have_magic = true;
			pipe.expected = total;
			continue;
		}

		if (magic == attached_surface_magic) {
			attached_surface_message surface{};
			std::memcpy(&surface, pipe.bytes.data(), sizeof(surface));
			if (!import_surface_handles(surface)) {
				close_surface_pipe(d, instance);
				return;
			}
			events_out.push<attached_surface_ready>({
				.generation = pipe.generation,
				.instance = instance,
				.revision = surface.revision,
				.message = own_surface_message(std::move(surface)),
			});
			pipe.handshake_done = true;
		}
		else {
			attached_fatal_message fatal{};
			std::memcpy(&fatal, pipe.bytes.data(), sizeof(fatal));
			events_out.push<attached_fatal_reported>({
				.generation = pipe.generation,
				.instance = instance,
				.file = fatal.file,
				.line = fatal.line,
				.function = fatal.function,
				.comment = fatal.comment,
			});
		}

		pipe.received = 0;
		pipe.expected = sizeof(std::uint32_t);
		pipe.have_magic = false;
	}
}

auto gse::ide::build_runner::flush_pipe_tail(data& d, const std::uint32_t instance) -> void {
	surface_pipe& pipe = d.pipes[instance];
	if (!pipe.handle || pipe.pending_tail.empty()) {
		return;
	}
	win32::DWORD written = 0;
	if (!win32::WriteFile(pipe.handle, pipe.pending_tail.data(), static_cast<win32::DWORD>(pipe.pending_tail.size()), &written, nullptr)) {
		log::println(log::level::warning, log::category::general, "attached pipe: write failed (error {}); closing pipe", win32::GetLastError());
		close_surface_pipe(d, instance);
		return;
	}
	pipe.pending_tail.erase(pipe.pending_tail.begin(), pipe.pending_tail.begin() + written);
}

auto gse::ide::build_runner::write_pipe_message(data& d, const std::uint32_t instance, const void* bytes, const std::size_t size) -> void {
	surface_pipe& pipe = d.pipes[instance];
	if (!pipe.handle || !pipe.handshake_done) {
		return;
	}
	flush_pipe_tail(d, instance);
	if (!pipe.handle || !pipe.pending_tail.empty()) {
		return;
	}
	win32::DWORD written = 0;
	if (!win32::WriteFile(pipe.handle, bytes, static_cast<win32::DWORD>(size), &written, nullptr)) {
		log::println(log::level::warning, log::category::general, "attached pipe: write failed (error {}); closing pipe", win32::GetLastError());
		close_surface_pipe(d, instance);
		return;
	}
	if (written < size) {
		const auto* tail = static_cast<const char*>(bytes) + written;
		pipe.pending_tail.assign(tail, tail + (size - written));
	}
}

auto gse::ide::build_runner::send_attached_input(data& d, const std::uint32_t instance, const input::event& event) -> void {
	const attached_input_message message{
		.magic = attached_input_magic,
		.event = event,
	};
	write_pipe_message(d, instance, &message, sizeof(message));
}

auto gse::ide::build_runner::send_attached_resize(data& d, const std::uint32_t instance, const vec2u extent) -> void {
	const attached_resize_message message{
		.magic = attached_resize_magic,
		.extent = extent,
	};
	write_pipe_message(d, instance, &message, sizeof(message));
}

auto gse::ide::build_runner::init(data& d) -> async::task<> {
	cleanup_backups();
	load_profiles(d.profiles, d.active_profile);
	return {};
}

auto gse::ide::build_runner::request_for_profile(const build_profile& profile, const bool run_after) -> build_request {
	return {
		.target = build_target::game,
		.run_after = run_after,
		.session = profile.session,
		.config = profile.config,
	};
}

auto gse::ide::build_runner::run(context& ctx, data& d, const channel_read<attached_surface_imported, attached_surface_rejected, build_request, stop_session_request, attached_input, attached_resize, select_profile_request, edit_profiles_request> requests_in, const channel_write<attached_session_ended, stream_opened, build_finished, attached_surface_ready, attached_fatal_reported> events_out) -> async::task<> {
	for (const edit_profiles_request& edited : requests_in.of<edit_profiles_request>()) {
		if (edited.profiles.empty()) {
			continue;
		}
		d.profiles = edited.profiles;
		d.active_profile = profile_for(d.profiles, edited.active) != nullptr ? edited.active : d.profiles.front().name;
	}
	for (const select_profile_request& selected : requests_in.of<select_profile_request>()) {
		if (profile_for(d.profiles, selected.name) != nullptr) {
			d.active_profile = selected.name;
		}
	}
	for (const attached_surface_imported& imported : requests_in.of<attached_surface_imported>()) {
		if (session_for(d, imported.generation, imported.instance)) {
			d.sessions[imported.instance].status = attached_session_status::active;
		}
	}
	for (const attached_surface_rejected& rejected : requests_in.of<attached_surface_rejected>()) {
		if (session_for(d, rejected.generation, rejected.instance)) {
			d.session_error = rejected.reason;
			if (const std::optional<std::uint32_t> ended = stop_games(d)) {
				events_out.push<attached_session_ended>({
					.generation = *ended,
				});
			}
		}
	}
	for (const auto& _ : requests_in.of<stop_session_request>()) {
		d.session_error.clear();
		if (const std::optional<std::uint32_t> ended = stop_games(d)) {
			events_out.push<attached_session_ended>({
				.generation = *ended,
			});
		}
	}

	for (const build_request& request : requests_in.of<build_request>()) {
		start_build(events_out, d, request);
	}
	drain_completion(events_out, d);
	poll_games(events_out, d);
	poll_stopping_games(d, false);
	d.server = {
		.running = d.session.dedicated_server && std::ranges::any_of(d.games, [](const attached_game& game) {
			return !game.owns_pipe;
		}),
		.port = d.session.base_port,
	};
	for (std::uint32_t instance = 0; instance < max_attached_instances; ++instance) {
		poll_surface_pipe(events_out, d, instance);
		flush_pipe_tail(d, instance);
	}
	for (const attached_input& forwarded : requests_in.of<attached_input>()) {
		send_attached_input(d, forwarded.instance, forwarded.event);
	}

	for (const attached_resize& resized : requests_in.of<attached_resize>()) {
		send_attached_resize(d, resized.instance, resized.extent);
	}
	if (d.sessions[0].generation != 0 && !d.pipes[0].handle) {
		if (const std::optional<std::uint32_t> ended = stop_games(d)) {
			events_out.push<attached_session_ended>({
				.generation = *ended,
			});
		}
	}
	watch_editor_image(d);
	return {};
}

auto gse::ide::build_runner::shutdown(data& d) -> void {
	save_profiles(d.profiles, d.active_profile);
	d.worker.request_stop();
	if (d.active_stream) {
		spawn::terminate_process(*d.active_stream);
	}
	if (d.worker.joinable()) {
		d.worker.join();
	}
	(void)stop_games(d);
	poll_stopping_games(d, true);
}
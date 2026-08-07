export module gse.diag:profile_aggregator;

import std;

import gse.config;
import gse.math;
import gse.meta;
import gse.core;
import gse.containers;
import gse.time;

import :trace;

export namespace gse::profile {
	using sample_time = time_t<double>;

	enum struct domain : std::uint8_t {
		cpu,
		gpu
	};

	struct row {
		id id;
		sample_time ema;
		sample_time last;
		sample_time peak;
		std::uint64_t sample_count = 0;
	};

	struct report_entry {
		id id;
		sample_time per_frame;
		sample_time ema;
		sample_time last;
		sample_time peak;
		double calls_per_frame = 0.0;
		std::uint64_t sample_count = 0;
		std::uint32_t dominant_tid = 0;
	};

	auto build_report(
		domain domain,
		std::vector<report_entry>& out
	) -> void;

	auto lookup_report(
		id id,
		domain domain
	) -> std::optional<report_entry>;

	auto on_main_thread(
		std::uint32_t dominant_tid,
		std::uint32_t main_tid
	) -> bool;

	auto frames_profiled() -> std::uint64_t;

	auto ingest_frame() -> void;

	auto ingest_gpu_sample(
		id pass_id,
		sample_time duration
	) -> void;

	auto lookup(
		id id,
		domain domain
	) -> std::optional<row>;

	auto top_n(
		std::size_t n,
		domain domain,
		std::vector<row>& out
	) -> void;

	auto set_alpha(
		double alpha
	) -> void;

	auto alpha() -> double;

	auto set_enabled(
		bool enabled
	) -> void;

	auto enabled() -> bool;

	auto reset() -> void;

	auto dump(
		const std::filesystem::path& path = config::profile_dir() / std::format("{}.txt", config::executable_stem())
	) -> void;

	auto dump_chrome_trace(
		const std::filesystem::path& path = config::profile_dir() / std::format("{}.json", config::executable_stem())
	) -> void;

	constexpr std::uint32_t report_magic = 0x47535250;
	constexpr std::uint32_t report_version = 1;

	struct report_record {
		std::string tag;
		sample_time per_frame;
		sample_time ema;
		sample_time last;
		sample_time peak;
		double calls_per_frame = 0.0;
		std::uint64_t sample_count = 0;
		std::uint32_t dominant_tid = 0;
	};

	struct report_node {
		std::uint32_t tag = 0;
		std::uint32_t trace_id = 0;
		time_t<std::uint64_t> start;
		time_t<std::uint64_t> stop;
		time_t<std::uint64_t> self;
		std::uint32_t children_first = 0;
		std::uint32_t children_count = 0;
		bool open = false;
	};

	struct report_frame {
		std::vector<report_node> nodes;
		std::vector<std::uint32_t> children;
		std::vector<std::uint32_t> roots;
		std::uint64_t generation = 0;
		time_t<std::uint64_t> origin;
		sample_time span;
	};

	struct report_file {
		std::vector<report_record> cpu;
		std::vector<report_record> gpu;
		std::vector<std::string> tags;
		std::vector<report_frame> recorded;
		sample_time frame_time;
		std::uint64_t frames = 0;
		std::uint32_t main_tid = 0;
	};

	auto set_frame_recording(
		bool enabled
	) -> void;

	auto frame_recording() -> bool;

	auto set_recording_capacity(
		std::size_t frames
	) -> void;

	auto recording_capacity() -> std::size_t;

	auto build_report_file() -> report_file;

	auto dump_report(
		const std::filesystem::path& path = config::profile_dir() / std::format("{}.gsprof", config::executable_stem())
	) -> void;

	auto load_report(
		const std::filesystem::path& path
	) -> std::expected<report_file, archive_mismatch>;
}

namespace gse::profile {
	constexpr std::size_t default_recorded_frames = 600;
	constexpr std::size_t max_recorded_nodes = 4000000;
	constexpr std::size_t chrome_trace_frames = 8;

	inline std::atomic recording_frames{ false };
	inline std::atomic<std::size_t> max_recorded_frames{ default_recorded_frames };
	inline std::mutex recorded_mutex;
	inline std::deque<report_frame> recorded_frames;
	inline std::size_t recorded_node_total = 0;
	inline std::vector<std::string> recorded_tags;
	inline std::unordered_map<std::uint64_t, std::uint32_t> recorded_tag_indices;

	auto record_frame(
		const trace::frame_view& view
	) -> void;

	auto worst_recorded_frames(
		std::size_t count
	) -> std::vector<report_frame>;

	auto fill_records(
		domain domain,
		std::vector<report_record>& out
	) -> void;

	struct entry {
		id id;
		sample_time ema;
		sample_time last;
		sample_time peak;
		std::unordered_map<std::uint32_t, std::uint64_t> samples_by_tid;
	};

	struct dag_visit {
		std::uint32_t index = 0;
		int depth = 0;
	};

	constexpr int max_dag_depth = 12;
	constexpr std::size_t dag_bar_width = 80;
	constexpr std::size_t breakdown_row_limit = 20;
	constexpr std::size_t breakdown_tid_limit = 6;

	inline std::shared_mutex state_mutex;
	inline std::array<std::flat_map<id, entry>, 2> entries;
	inline std::atomic ema_alpha{ 0.1 };
	inline std::atomic is_enabled{ true };
	inline std::atomic<std::uint64_t> frame_count{ 0 };
	inline std::atomic<std::uint64_t> last_generation{ 0 };

	auto storage_for(
		domain domain
	) -> std::flat_map<id, entry>&;

	auto sample_count_of(
		const entry& e
	) -> std::uint64_t;

	auto dominant_tid_of(
		const entry& e
	) -> std::uint32_t;

	auto to_row(
		const entry& e
	) -> row;

	auto to_report(
		const entry& e,
		std::uint64_t frames
	) -> report_entry;

	auto update_entry(
		std::flat_map<id, entry>& map,
		id id,
		sample_time duration,
		std::uint32_t thread_id
	) -> void;

	auto snapshot_entries(
		domain domain,
		std::vector<entry>& out
	) -> void;

	auto write_section(
		std::ofstream& out,
		std::string_view title,
		std::span<const report_entry> rows,
		sample_time frame_time
	) -> void;

	auto write_thread_breakdown(
		std::ofstream& out,
		std::span<const entry> worker_src
	) -> void;

	auto write_dag(
		std::ofstream& out
	) -> void;

	auto flatten_dag(
		const trace::frame_view& fv,
		std::vector<dag_visit>& out
	) -> void;

	auto escape_json(
		std::string_view s
	) -> std::string;
}

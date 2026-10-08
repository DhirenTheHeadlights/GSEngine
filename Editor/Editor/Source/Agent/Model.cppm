export module gse.ide.agent:model;

import std;
import gse;

import gse.ide.build;
import gse.ide.config;
import gse.ide.net;

export namespace gse::ide::agent {
	constexpr std::string_view panel_name = "Agent";

	struct row_style {
		char prefix[8] = "  ";
		vec4f gui::style::* color = &gui::style::color_text_secondary;
	};

	enum class row_kind : std::uint8_t {
		note,
		user [[= row_style{
			.prefix = "",
			.color = &gui::style::color_text,
		}]],
		text [[= row_style{
			.prefix = "",
			.color = &gui::style::color_accent,
		}]],
		tool [[= row_style{
			.prefix = "- ",
			.color = &gui::style::color_accent_dim,
		}]],
		phase [[= row_style{
			.prefix = "# ",
			.color = &gui::style::color_folder,
		}]],
		denial [[= row_style{
			.prefix = "x ",
			.color = &gui::style::color_warning,
		}]],
		failure [[= row_style{
			.prefix = "! ",
			.color = &gui::style::color_error,
		}]],
		retry [[= row_style{
			.prefix = "~ ",
			.color = &gui::style::color_warning,
		}]],
	};

	struct phase_policy {
		char label[24] = "";
		char enter_label[24] = "";
		char opening[96] = "";
		vec4f gui::style::* color = &gui::style::color_text_secondary;
		bool read_only = false;
		bool fresh_context = false;
		bool side_thread = false;
		bool gated = false;
	};

	enum class task_phase : std::uint8_t {
		scope [[= phase_policy{
			.label = "scoping",
			.color = &gui::style::color_folder,
			.read_only = true,
			.fresh_context = true,
			.gated = true,
		}]],
		apply [[= phase_policy{
			.label = "applying",
			.enter_label = "Start applying",
			.opening = "The scope is approved. Implement it now, following your phase instructions.",
			.color = &gui::style::color_added,
		}]],
		review [[= phase_policy{
			.label = "reviewing",
			.opening = "Review the change now, following your phase instructions.",
			.color = &gui::style::color_accent,
			.read_only = true,
			.fresh_context = true,
			.side_thread = true,
		}]],
		revise [[= phase_policy{
			.label = "revising",
			.opening = "Address the review findings now, following your phase instructions.",
			.color = &gui::style::color_warning,
		}]],
		summarize [[= phase_policy{
			.label = "summarizing",
			.opening = "Write the owner's summary of this task now, following your phase instructions.",
			.color = &gui::style::color_file,
			.read_only = true,
			.fresh_context = true,
			.side_thread = true,
		}]],
		settled [[= phase_policy{
			.label = "settled",
			.color = &gui::style::color_text_disabled,
		}]],
	};

	struct transcript_row {
		row_kind kind = row_kind::note;
		std::string text;
		std::string detail;
		std::string uuid;
		std::string agent;
		task_phase phase = task_phase::settled;
		std::int64_t stamped = 0;
		std::filesystem::path file;
		std::vector<std::string> removed;
		std::vector<std::string> added;
		[[= archive_skip{}]] std::optional<std::uint32_t> start_line;
	};

	struct start_request {
		std::string prompt;
		std::filesystem::path cwd;
	};

	struct draft_file {
		std::filesystem::path relative;
		char code = '\0';
		int added = 0;
		int deleted = 0;
	};

	struct draft_request {
		std::filesystem::path root;
		std::string branch;
		std::vector<draft_file> files;
	};

	struct draft_ready {
		std::filesystem::path root;
		std::string message;
		std::string failure;
	};

	struct blame_offer {
		std::uint32_t session = 0;
		std::string session_name;
		std::filesystem::path file;
		std::uint32_t line = 0;
		std::uint32_t extra = 0;
		build_runner::stream_kind kind = build_runner::stream_kind::none;
	};

	struct dispatch_request {
		std::uint32_t session = 0;
	};

	struct session_info {
		std::string agent_id;
		std::string model;
		std::int64_t turns = 0;
		time api_time{};
		double cost = 0.0;
		std::int64_t context_used = 0;
		std::int64_t context_base = 0;
		std::string failure;
		[[= archive_skip{}]] byte_count tool_bytes;
		[[= archive_skip{}]] byte_count tool_peak;
		[[= archive_skip{}]] std::string tool_peak_name;
		[[= archive_skip{}]] std::unordered_map<std::string, std::string> tool_names;
	};

	constexpr std::uint32_t unplaced_line = ~0u;

	struct group_marker {
		std::uint32_t row = 0;
		std::uint32_t line = 0;
		std::uint32_t rows = 0;
		std::uint32_t toggle_line = unplaced_line;
	};

	struct link_marker {
		std::uint32_t first_line = 0;
		std::uint32_t last_line = 0;
		std::uint32_t row = 0;
	};

	struct diff_view {
		std::uint32_t row = 0;
		std::uint32_t line = unplaced_line;
		std::uint32_t first_line = unplaced_line;
		std::uint32_t width = 0;
		std::size_t columns = 0;
		std::size_t overflow = 0;
		gui::scroll_state scroll;
	};

	enum class row_filter : std::uint8_t {
		all,
		digest,
		changes,
	};

	struct transcript_view {
		row_filter filter = row_filter::all;
		gui::text_buffer buffer;
		gui::text_area_state state;
		std::vector<gui::text_span> spans;
		std::vector<gui::text_stop> stops;
		std::vector<gui::text_rule> rules;
		std::vector<gui::text_block> blocks;
		std::vector<link_marker> links;
		std::vector<std::uint32_t> line_rows;
		std::vector<group_marker> groups;
		std::vector<group_marker> previews;
		std::vector<diff_view> diffs;
		std::size_t flushed_rows = 0;
		float wrap_width = 0.f;
		std::uint64_t style_key = 0;
		gse::id log_id;
	};

	enum class transcript_mode : std::uint8_t {
		digest,
		transcript,
	};

	struct touched_source {
		id build_key;
		std::int64_t mtime = 0;
		bool wrote = false;
		std::vector<std::vector<std::string>> hunks;
	};

	struct blamed_error {
		std::filesystem::path file;
		std::uint32_t line = 0;
		std::string message;
		std::vector<std::string> notes;
	};

	enum class build_wait : std::uint8_t {
		none,
		queued,
		building,
	};

	struct hold_style {
		char label[64] = "";
	};

	enum class build_hold : std::uint8_t {
		none,
		building [[= hold_style{
			.label = "the editor is already running a build",
		}]],
		editor_restart [[= hold_style{
			.label = "the editor is rebuilding itself and will restart",
		}]],
		tree_busy [[= hold_style{
			.label = "another chat is still working in this tree",
		}]],
	};

	struct build_hold_state {
		build_hold reason = build_hold::none;
		std::string blocker;
	};

	struct model_option {
		std::string value;
		std::string label;
	};

	enum class agent_effort : std::uint8_t {
		inherit [[= settings::option_label{ .text = "Default" }]],
		low,
		medium,
		high,
		xhigh [[= settings::option_label{ .text = "X-High" }]],
		max,
	};

	struct state_style {
		char label[24] = "idle";
		vec4f gui::style::* color = &gui::style::color_text_secondary;
	};

	enum class agent_state : std::uint8_t {
		exited [[= state_style{
			.label = "exited",
			.color = &gui::style::color_text_disabled,
		}]],
		idle [[= state_style{
			.label = "idle",
			.color = &gui::style::color_text_secondary,
		}]],
		working [[= state_style{
			.label = "working",
			.color = &gui::style::color_accent,
		}]],
		editing [[= state_style{
			.label = "editing files",
			.color = &gui::style::color_added,
		}]],
		build_queued [[= state_style{
			.label = "build queued",
			.color = &gui::style::color_warning,
		}]],
		compiling [[= state_style{
			.label = "compiling",
			.color = &gui::style::color_folder,
		}]],
		hibernating [[= state_style{
			.label = "hibernating",
			.color = &gui::style::color_file,
		}]],
		awaiting_approval [[= state_style{
			.label = "waiting for approval",
			.color = &gui::style::color_folder,
		}]],
		rate_limited [[= state_style{
			.label = "usage limit reached",
			.color = &gui::style::color_warning,
		}]],
	};

	struct tool_flavor {
		char match[40] = "";
		char verb[32] = "";
	};

	enum class tool_kind : std::uint8_t {
		other,
		read [[= tool_flavor{ .match = "Read", .verb = "reading" }]],
		glob [[= tool_flavor{ .match = "Glob", .verb = "sifting for" }]],
		grep [[= tool_flavor{ .match = "Grep", .verb = "rummaging for" }]],
		edit [[= tool_flavor{ .match = "Edit", .verb = "rewriting" }]],
		write [[= tool_flavor{ .match = "Write", .verb = "drafting" }]],
		notebook [[= tool_flavor{ .match = "NotebookEdit", .verb = "rewriting" }]],
		shell [[= tool_flavor{ .match = "Bash", .verb = "at the shell:" }]],
		shell_ps [[= tool_flavor{ .match = "PowerShell", .verb = "at the shell:" }]],
		build [[= tool_flavor{ .match = "mcp__gse__gse_build", .verb = "feeding the compiler" }]],
		build_status [[= tool_flavor{ .match = "mcp__gse__gse_build_status", .verb = "checking on the build" }]],
		run [[= tool_flavor{ .match = "mcp__gse__gse_run", .verb = "taking it for a spin" }]],
		log [[= tool_flavor{ .match = "mcp__gse__gse_log_query", .verb = "reading the logs" }]],
		symbol [[= tool_flavor{ .match = "mcp__gse__gse_symbol_query", .verb = "chasing down" }]],
		trace [[= tool_flavor{ .match = "mcp__gse__gse_trace_query", .verb = "squinting at a trace" }]],
		hibernate [[= tool_flavor{ .match = "mcp__gse__gse_hibernate", .verb = "dozing off" }]],
		phase_done [[= tool_flavor{ .match = "mcp__gse__gse_phase_done", .verb = "handing in the work" }]],
		report_gap [[= tool_flavor{ .match = "mcp__gse__gse_report_gap", .verb = "filing a complaint" }]],
		remove [[= tool_flavor{ .match = "mcp__gse__gse_delete_file", .verb = "taking out the trash" }]],
		task [[= tool_flavor{ .match = "Task", .verb = "delegating" }]],
		agent [[= tool_flavor{ .match = "Agent", .verb = "delegating" }]],
		web_search [[= tool_flavor{ .match = "WebSearch", .verb = "consulting the internet" }]],
		web_fetch [[= tool_flavor{ .match = "WebFetch", .verb = "consulting the internet" }]],
		todo [[= tool_flavor{ .match = "TodoWrite", .verb = "making a list" }]],
		skill [[= tool_flavor{ .match = "Skill", .verb = "reading the manual" }]],
	};

	struct phase_run {
		task_phase phase = task_phase::scope;
		std::string agent_id;
		std::uint32_t first_row = 0;
		std::int64_t started = 0;
		std::string summary;
		std::uint32_t findings = 0;
	};

	struct phase_gate {
		task_phase from = task_phase::scope;
		std::string summary;
		std::string note;
		std::uint32_t findings = 0;
	};

	struct handoff_capture {
		std::filesystem::path scratch;
		std::filesystem::path record;
		std::filesystem::path touched;
		std::filesystem::path diff;
		std::filesystem::path log;
		std::filesystem::path root;
		std::string heading;
		std::string summary;
		std::vector<std::filesystem::path> written;
		std::vector<transcript_row> edits;
	};

	struct queued_build {
		std::string id;
		std::string agent;
		std::string profile;
		std::string config;
		std::filesystem::path cwd;
		std::filesystem::path project;
		std::vector<std::string> settings;
		std::string scenario;
		time exit_after{};
		build_runner::build_target target = build_runner::build_target::game;
		bool run = false;
		bool run_only = false;
		bool forced = false;
		const config::worktree* tree = nullptr;
		time requested;
		build_hold reported = build_hold::none;
		std::string blocker;
	};

	struct retry_state {
		bool held = false;
		std::uint32_t attempts = 0;
		bool waiting = false;
	};

	struct usage_window {
		std::string label;
		double utilization = 0.0;
		std::int64_t resets_at = 0;
	};

	struct account_usage {
		std::vector<usage_window> windows;
		std::string error;
		std::int64_t fetched = 0;
	};

	struct past_chat {
		std::string agent_id;
		std::filesystem::path path;
		std::string summary;
		std::int64_t modified = 0;
		bool summarized = false;
	};

	struct session {
		std::uint32_t id = 0;
		std::string name;
		std::filesystem::path cwd;
		session_info info;
		[[= archive_skip{}]] void* process = nullptr;
		[[= archive_skip{}]] void* job = nullptr;
		[[= archive_skip{}]] void* output = nullptr;
		[[= archive_skip{}]] void* input = nullptr;
		[[= archive_skip{}]] std::string pending;
		[[= archive_skip{}]] std::vector<transcript_row> rows;
		[[= archive_skip{}]] std::int64_t message_chars = 0;
		[[= archive_skip{}]] std::size_t counted_rows = 0;
		[[= archive_skip{}]] bool hydrated = false;
		[[= archive_skip{}]] transcript_view digest;
		[[= archive_skip{}]] transcript_view full;
		[[= archive_skip{}]] transcript_view changes;
		transcript_mode mode = transcript_mode::digest;
		bool changes_open = false;
		float changes_ratio = 0.55f;
		[[= archive_skip{}]] gui::layout::split_drag_state changes_drag;
		[[= archive_skip{}]] std::optional<std::uint32_t> changes_focus;
		gui::text_buffer draft;
		[[= archive_skip{}]] gui::text_area_state draft_state;
		[[= archive_skip{}]] gui::image_attachments attachments;
		[[= archive_skip{}]] gui::text_input_state name_state;
		[[= archive_skip{}]] std::vector<std::uint32_t> expanded_groups;
		bool hibernating = false;
		std::string wake_prompt;
		std::vector<phase_run> runs;
		std::optional<phase_gate> gate;
		std::optional<phase_gate> pending_transition;
		[[= archive_skip{}]]
		task::pending<bool> handoff;
		std::string model_id;
		agent_effort requested_effort = agent_effort::inherit;
		[[= archive_skip{}]] std::string launched_model_id;
		[[= archive_skip{}]] agent_effort launched_effort = agent_effort::inherit;
		[[= archive_skip{}]] gui::dropdown_state model_dropdown;
		std::unordered_map<gse::id, std::int64_t> unbuilt;
		std::unordered_map<gse::id, touched_source> touched;
		std::vector<blamed_error> blame;
		gse::id blame_build;
		retry_state retry;
		bool turn_open = false;
		std::uint32_t resume_attempts = 0;
		std::int64_t limited_until = 0;
		[[= archive_skip{}]] bool running = false;
		[[= archive_skip{}]] bool stale = false;
		[[= archive_skip{}]] std::optional<clock> think_clock;
		[[= archive_skip{}]] std::optional<clock> recent_turn;
		[[= archive_skip{}]] std::optional<clock> last_write;
		[[= archive_skip{}]] bool wrote_this_turn = false;
		[[= archive_skip{}]] bool published_presence = false;
		[[= archive_skip{}]] std::string action;
		[[= archive_skip{}]] std::uint32_t next_control = 0;
	};

	struct [[= system_state<"Agent">{}, = settings::category<"Agent">{}]] data {
		std::vector<session> sessions;
		std::uint32_t next_id = 0;
		std::uint32_t active = 0;
		std::uint32_t renaming = 0;
		std::uint32_t pending_close = 0;
		[[= archive_skip{}]] bool naming_new_chat = false;
		[[= archive_skip{}]] std::string new_chat_name;
		[[= archive_skip{}]] gui::text_input_state new_chat_name_state;
		gui::interaction::click_state tab_click;
		gui::tab_strip_state tab_strip;
		rectf info_anchor;
		bool info_open = false;
		bool overview_active = true;
		[[
			= settings::describe<"Model new chats start on. Options come from the model list claude itself caches, so new releases appear without an editor update. Leave it empty to pass no --model flag.">{}
		]]
		settings::choice default_model;

		[[
			= settings::describe<"Reasoning effort new chats start on. 'Default' passes no --effort flag, so your global effortLevel setting applies.">{}
		]]
		agent_effort default_effort = agent_effort::inherit;
		[[= archive_skip{}]] rectf history_anchor;
		[[= archive_skip{}]] bool history_open = false;
		[[= archive_skip{}]] std::vector<past_chat> history;
		std::unordered_map<std::string, std::string> chat_names;
		bool initialized = false;
		std::vector<blamed_error> unclaimed;
		id unclaimed_build;
		[[= archive_skip{}]] std::unordered_map<id, std::int64_t> built;
		[[= archive_skip{}]] time next_built_poll;
		[[= archive_skip{}]] std::unique_ptr<http::client> usage_client;
		[[= archive_skip{}]] id usage_ticket;
		[[= archive_skip{}]] account_usage usage;
		[[= archive_skip{}]] net::probe link;
		[[= archive_skip{}]] std::optional<clock> link_clock;
		[[= archive_skip{}]] std::uint32_t link_misses = 0;
		[[= archive_skip{}]] time next_inbox_poll;
		[[= archive_skip{}]] std::vector<build_inbox::presence> presence;
		[[= archive_skip{}]] time next_presence_write;
		[[= archive_skip{}]] std::vector<queued_build> inbox_queue;
		[[= archive_skip{}]] std::vector<queued_build> inbox_active;
		[[= archive_skip{}]] time inbox_dispatch_deadline;
		[[= archive_skip{}]] bool inbox_started = false;
		[[= archive_skip{}]] task::pending<draft_ready> draft;
	};
}
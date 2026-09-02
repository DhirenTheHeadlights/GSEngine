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

	struct transcript_row {
		row_kind kind = row_kind::note;
		std::string text;
		std::string detail;
		std::string uuid;
		std::filesystem::path file;
		std::vector<std::string> removed;
		std::vector<std::string> added;
		[[= archive_skip{}]] std::optional<std::uint32_t> start_line;
	};

	struct start_request {
		std::string prompt;
		std::filesystem::path cwd;
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
	};

	struct group_marker {
		std::uint32_t row = 0;
		std::uint32_t line = 0;
		std::uint32_t rows = 0;
	};

	struct link_marker {
		std::uint32_t first_line = 0;
		std::uint32_t last_line = 0;
		std::uint32_t row = 0;
	};

	constexpr std::uint32_t unplaced_line = ~0u;

	struct diff_view {
		std::uint32_t row = 0;
		std::uint32_t line = unplaced_line;
		std::uint32_t width = 0;
		std::size_t columns = 0;
		std::size_t overflow = 0;
		gui::scroll_state scroll;
	};

	struct touched_source {
		id build_key;
		std::int64_t mtime = 0;
		bool wrote = false;
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
		std::uint32_t blocker = 0;
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
		rate_limited [[= state_style{
			.label = "usage limit reached",
			.color = &gui::style::color_warning,
		}]],
	};

	struct queued_build {
		std::string id;
		std::string agent;
		build_runner::build_target target = build_runner::build_target::game;
		bool run = false;
		bool forced = false;
		const config::worktree* tree = nullptr;
		time requested;
		build_hold reported = build_hold::none;
		std::uint32_t blocker = 0;
	};

	struct retry_state {
		std::string prompt;
		std::vector<std::filesystem::path> images;
		std::uint32_t attempts = 0;
		bool waiting = false;
	};

	struct attachment {
		std::filesystem::path path;
		vec2u size;
		resource::handle<texture> preview;
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
		[[= archive_skip{}]] std::size_t flushed_rows = 0;
		[[= archive_skip{}]] float wrap_width = 0.f;
		[[= archive_skip{}]] std::uint64_t style_key = 0;
		[[= archive_skip{}]] std::vector<diff_view> diffs;
		[[= archive_skip{}]] gui::text_buffer buffer;
		[[= archive_skip{}]] gui::text_area_state view;
		gui::text_buffer draft;
		[[= archive_skip{}]] gui::text_area_state draft_state;
		[[= archive_skip{}]] std::vector<attachment> attachments;
		[[= archive_skip{}]] gui::text_input_state name_state;
		[[= archive_skip{}]] std::vector<gui::text_span> spans;
		[[= archive_skip{}]] std::vector<gui::text_block> blocks;
		[[= archive_skip{}]] std::vector<link_marker> links;
		[[= archive_skip{}]] std::vector<std::uint32_t> line_rows;
		[[= archive_skip{}]] std::vector<group_marker> groups;
		[[= archive_skip{}]] std::vector<std::uint32_t> expanded_groups;
		[[= archive_skip{}]] gse::id log_id;
		bool hibernating = false;
		std::string wake_prompt;
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
		std::int64_t limited_until = 0;
		[[= archive_skip{}]] bool running = false;
		[[= archive_skip{}]] bool stale = false;
		[[= archive_skip{}]] std::optional<clock> think_clock;
		[[= archive_skip{}]] std::optional<clock> recent_turn;
		[[= archive_skip{}]] bool wrote_this_turn = false;
		[[= archive_skip{}]] std::string action;
		[[= archive_skip{}]] std::uint32_t next_control = 0;
	};

	struct [[= system_state<"Agent">{}, = settings::category<"Agent">{}]] data {
		std::vector<session> sessions;
		std::uint32_t next_id = 0;
		std::uint32_t active = 0;
		std::uint32_t renaming = 0;
		std::uint32_t pending_close = 0;
		std::uint32_t next_attachment = 0;
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
		[[= archive_skip{}]] net::probe link;
		[[= archive_skip{}]] std::optional<clock> link_clock;
		[[= archive_skip{}]] std::uint32_t link_misses = 0;
		[[= archive_skip{}]] time next_inbox_poll;
		[[= archive_skip{}]] std::vector<queued_build> inbox_queue;
		[[= archive_skip{}]] std::vector<queued_build> inbox_active;
		[[= archive_skip{}]] time inbox_dispatch_deadline;
	};
}
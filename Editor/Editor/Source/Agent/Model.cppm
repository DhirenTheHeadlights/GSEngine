export module gse.ide.agent:model;

import std;
import gse;

import gse.ide.build;
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
		std::unordered_map<gse::id, std::int64_t> unbuilt;
		std::unordered_map<gse::id, touched_source> touched;
		std::vector<blamed_error> blame;
		gse::id blame_build;
		[[= archive_skip{}]] retry_state retry;
		[[= archive_skip{}]] bool running = false;
		[[= archive_skip{}]] bool stale = false;
		[[= archive_skip{}]] std::optional<clock> think_clock;
		[[= archive_skip{}]] std::optional<clock> recent_turn;
		[[= archive_skip{}]] std::string action;
		[[= archive_skip{}]] std::uint32_t next_control = 0;
	};

	struct [[= system_state<"Agent">{}]] data {
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
	};
}
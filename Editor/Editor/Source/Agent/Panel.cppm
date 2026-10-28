export module gse.ide.agent:panel;

import std;
import gse;

import gse.ide.build;
import gse.ide.navigation;

import :chrome;
import :model;

export namespace gse::ide::agent {
	auto draw_panel(
		gui::builder& ui,
		data& d,
		channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out
	) -> void;

	auto draw_sync_chat(
		gui::builder& ui,
		data& d,
		const rectf& body,
		channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out
	) -> void;
}

namespace gse::ide::agent {
	constexpr std::size_t max_input_rows = 12;
	constexpr std::size_t max_question_lines = 4;
	constexpr std::size_t min_question_lines = 2;
	constexpr std::size_t max_option_lines = 3;
	constexpr float question_body_share = 0.6f;
	constexpr float plan_body_share = 0.6f;
	constexpr float plan_split_share = 0.8f;
	constexpr float error_strip_fill = 0.12f;
	constexpr float error_strip_border = 0.45f;
	constexpr std::string_view unattributed_label = "unattributed";
	constexpr std::string_view revise_label = "Revise";
	constexpr std::string_view page_back_label = "<";
	constexpr std::string_view page_forward_label = ">";
	constexpr std::string_view cycle_back_label = "<<";
	constexpr std::string_view cycle_forward_label = ">>";
	constexpr std::string_view catch_up_label = "Catch up";
	constexpr std::string_view fork_open_label = "Ask about this stage";
	constexpr std::string_view fork_busy_label = "Ask (wait for the turn to end)";
	constexpr std::string_view fork_ask_label = "Ask";
	constexpr std::string_view fork_close_label = "Close";
	constexpr float fork_answer_share = 0.35f;
	constexpr std::size_t history_visible_rows = 12;
	constexpr std::int64_t base_context_window = 200'000;
	constexpr std::int64_t large_context_window = 1'000'000;
	constexpr float chars_per_token = 3.6f;

	auto input_text(
		const gui::text_buffer& buffer
	) -> std::string;

	auto input_empty(
		const gui::text_buffer& buffer
	) -> bool;

	auto reset_input(
		session& s
	) -> void;

	auto fill_input(
		session& s,
		std::string_view text
	) -> void;

	auto agent_context_tag() -> id;

	auto local_time_label(
		std::int64_t unix_seconds
	) -> std::string;

	auto usage_label(
		const usage_window& window
	) -> std::string;

	auto tool_output_label(
		const session_info& info
	) -> std::string;

	auto draw_token_prompt(gui::builder& ui, data& d, const rectf& body) -> void;

	auto draw_session_info(
		gui::builder& ui,
		data& d,
		const rectf& body
	) -> void;

	auto history_label(
		const data& d,
		const past_chat& chat
	) -> std::string_view;

	auto draw_history(
		gui::builder& ui,
		data& d,
		const rectf& body
	) -> void;

	auto chat_at(
		data& d,
		std::size_t index
	) -> std::vector<session>::iterator;

	auto session_tab_id(
		std::uint32_t session_id
	) -> id;

	auto overview_tab_id() -> id;

	auto overview_task(
		const chrome_plan& plan,
		const session& s
	) -> std::string;

	auto draw_overview(
		gui::builder& ui,
		data& d,
		const rectf& area
	) -> void;

	auto draw_session_tabs(
		gui::builder& ui,
		data& d,
		const rectf& body
	) -> float;

	auto draw_close_confirm(
		gui::builder& ui,
		data& d,
		const rectf& body
	) -> void;

	auto draw_new_chat_prompt(
		gui::builder& ui,
		data& d,
		const rectf& body
	) -> void;

	auto draw_transcript_hint(
		const gui::draw_context& ctx,
		const std::string& hint,
		const rectf& area
	) -> void;

	auto draw_transcript(
		gui::builder& ui,
		session& s,
		transcript_view& v,
		const rectf& area,
		const rectf& host,
		channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out
	) -> void;

	auto draw_canvases(
		gui::builder& ui,
		const session& s,
		transcript_view& v,
		const rectf& area,
		const rectf& host
	) -> void;

	auto draw_canvas_viewer(
		gui::builder& ui,
		const canvas_record& record,
		transcript_view& v,
		const rectf& host,
		id canvas
	) -> void;

	auto shown_view(
		session& s
	) -> transcript_view&;

	auto note_visits(
		data& d
	) -> void;

	auto draw_since_banner(
		const gui::draw_context& ctx,
		std::string_view digest,
		const rectf& area,
		const rectf& button
	) -> void;

	auto draw_fork(
		gui::builder& ui,
		session& s,
		const chrome_plan& plan,
		const lifecycle& life,
		const plan_paging& paging,
		const rectf& area
	) -> void;

	auto fork_extent(
		const gui::draw_context& ctx,
		const session& s,
		const lifecycle& life,
		const plan_paging& paging,
		float room
	) -> float;

	auto draw_lifecycle(
		gui::builder& ui,
		data& d,
		session& s,
		const chrome_plan& plan,
		const rectf& area,
		const rectf& body,
		channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out
	) -> void;

	auto draw_body_view(
		gui::builder& ui,
		data& d,
		session& s,
		const chrome_plan& plan,
		const rectf& area,
		const rectf& body,
		channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out
	) -> void;

	auto draw_view_toggles(
		gui::builder& ui,
		session& s,
		const chrome_plan& plan,
		const rectf& area
	) -> float;

	auto cancel_sync(
		session& s
	) -> void;

	auto draw_sync_header(
		gui::builder& ui,
		session& s,
		const rectf& area
	) -> void;

	auto draw_status(
		gui::builder& ui,
		session& s,
		const chrome_plan& plan,
		const rectf& area
	) -> void;

	struct question_block {
		std::vector<std::string> head;
		std::vector<std::string> hint;
		std::vector<std::vector<std::string>> options;
		float line_h = 0.f;
		float body = 0.f;
		float label = 0.f;
		float extent = 0.f;
	};

	auto question_prompt(
		const pending_question& asked
	) -> std::string;

	auto wrapped_block(
		const gui::draw_context& ctx,
		std::string_view text,
		float width,
		std::size_t limit
	) -> std::vector<std::string>;

	auto layout_question(
		const gui::draw_context& ctx,
		const pending_question& asked,
		float width,
		float room
	) -> question_block;

	auto question_extent(
		const gui::draw_context& ctx,
		const session& s,
		const chrome_plan& plan,
		const question_block& laid
	) -> float;

	auto question_option_rows(
		const gui::draw_context& ctx,
		const question_block& laid,
		const rectf& area
	) -> std::vector<rectf>;

	auto draw_question_body(
		const gui::draw_context& ctx,
		const question_block& laid,
		const rectf& area,
		std::span<const gui::interaction::press> presses
	) -> void;

	auto draw_question(
		gui::builder& ui,
		session& s,
		const rectf& area,
		const question_block& laid
	) -> void;

	struct plan_block {
		std::string head;
		std::string diffs_link;
		std::string overflow;
		std::string approve;
		std::vector<std::vector<std::string>> steps;
		std::vector<std::size_t> groups;
		std::vector<vec4f gui::style::*> marks;
		std::vector<std::string> files;
		std::vector<std::vector<std::size_t>> links;
		std::string page_label;
		std::string cycle_label;
		plan_paging paging;
		float summary_h = 0.f;
		float line_h = 0.f;
		float steps_w = 0.f;
		float files_w = 0.f;
		float gutter = 0.f;
		float button_h = 0.f;
		float extent = 0.f;
	};

	struct plan_rects {
		std::vector<rectf> steps;
		std::vector<rectf> files;
		rectf summary;
		rectf diffs;
		rectf back;
		rectf forward;
		rectf page;
		rectf cycle_back;
		rectf cycle_forward;
		rectf cycle_page;
		rectf diffs_link;
		rectf revise;
		rectf approve;
	};

	auto sync_plan_summary(
		const gui::draw_context& ctx,
		transcript_view& v,
		std::string_view summary,
		float width
	) -> void;

	auto sync_error_page(
		const gui::draw_context& ctx,
		transcript_view& v,
		const build_fix* fix,
		float width
	) -> void;

	auto sync_ledger(
		const gui::draw_context& ctx,
		transcript_view& v,
		const goal_dashboard* goal,
		float width
	) -> void;

	auto layout_goal(
		const gui::draw_context& ctx,
		plan_block& laid,
		const goal_dashboard* goal,
		float room
	) -> void;

	auto sync_step_diffs(
		const gui::draw_context& ctx,
		transcript_view& v,
		std::vector<transcript_row>& rows,
		std::span<const step_edit* const> edits,
		std::optional<std::size_t> group,
		float width
	) -> void;

	auto layout_plan(
		const gui::draw_context& ctx,
		std::span<const build_inbox::plan_step> steps,
		const step_progress& progress,
		std::size_t summary_lines,
		plan_paging paging,
		std::string_view approve,
		float ratio,
		float width,
		float room
	) -> plan_block;

	auto plan_extent(
		const gui::draw_context& ctx,
		const session& s,
		const chrome_plan& plan,
		const plan_block& laid
	) -> float;

	auto plan_rects_of(
		const gui::draw_context& ctx,
		const plan_block& laid,
		const rectf& area
	) -> plan_rects;

	auto draw_plan_body(
		const gui::draw_context& ctx,
		const plan_block& laid,
		const rectf& area,
		std::span<const gui::interaction::press> presses,
		std::optional<std::size_t> focus,
		float shown,
		id key
	) -> void;

	auto draw_plan_summary(
		const gui::draw_context& ctx,
		transcript_view& v,
		const rectf& rect,
		float shown,
		id key,
		id& hot,
		id& active,
		id& focus
	) -> void;

	auto split_of(
		data& d,
		split_kind kind
	) -> split_memory&;

	auto draw_split(
		const gui::draw_context& ctx,
		const rectf& container,
		float& ratio,
		gui::layout::split_drag_state& drag,
		float minimum,
		float thickness,
		channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out
	) -> gui::layout::split_result;

	auto plan_split_kind(
		page_kind kind
	) -> split_kind;

	auto draw_plan(
		gui::builder& ui,
		session& s,
		split_memory& columns,
		const rectf& area,
		const plan_block& laid,
		std::uint32_t& page,
		std::string_view key,
		channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out
	) -> void;

	auto followup_width(
		const gui::draw_context& ctx,
		const session& s,
		const chrome_plan& plan,
		const split_memory& split,
		float width
	) -> float;

	auto draw_followup(
		gui::builder& ui,
		session& s,
		const chrome_plan& plan,
		split_memory& split,
		const rectf& whole,
		float column,
		const rectf& body,
		channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out
	) -> void;

	auto submit_draft(
		session& s
	) -> void;

	auto draw_model_controls(
		gui::builder& ui,
		session& s,
		const rectf& model_rect,
		const rectf& effort_rect
	) -> void;

	struct input_layout {
		rectf area;
		rectf body;
		const chrome_plan& plan;
	};

	auto draw_input(
		gui::builder& ui,
		session& s,
		const input_layout& layout
	) -> void;

	auto context_window_for(
		const session_info& info
	) -> std::int64_t;

	auto message_tokens(
		session& s
	) -> std::int64_t;

	auto draw_context_bar(
		const gui::draw_context& ctx,
		session& s,
		const rectf& area
	) -> void;
}
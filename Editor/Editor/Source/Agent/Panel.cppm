export module gse.ide.agent:panel;

import std;
import gse;

import gse.ide.navigation;

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

	auto draw_session_info(
		const gui::draw_context& ctx,
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
		channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out
	) -> void;

	auto shown_view(
		session& s
	) -> transcript_view&;

	auto draw_view_toggles(
		gui::builder& ui,
		session& s,
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

	auto draw_phase_pipeline(
		const gui::draw_context& ctx,
		const session& s,
		const rectf& area,
		const rectf& clip
	) -> float;

	auto draw_phase(
		gui::builder& ui,
		session& s,
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
		const session& s
	) -> std::string;

	auto wrapped_block(
		const gui::draw_context& ctx,
		std::string_view text,
		float width,
		std::size_t limit
	) -> std::vector<std::string>;

	auto layout_question(
		const gui::draw_context& ctx,
		const session& s,
		float width,
		float room
	) -> question_block;

	auto draw_question(
		gui::builder& ui,
		session& s,
		const rectf& area,
		const question_block& laid
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

	auto activity_label(
		const data& d,
		const session& s
	) -> std::string;

	auto draw_activity_line(
		const gui::draw_context& ctx,
		const data& d,
		const session& s,
		const rectf& area,
		const rectf& clip
	) -> void;

	auto draw_activity(
		const gui::draw_context& ctx,
		const data& d,
		const session& s,
		const rectf& area
	) -> void;
}
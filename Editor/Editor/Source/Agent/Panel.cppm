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
}

namespace gse::ide::agent {
	constexpr std::size_t max_input_rows = 12;
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

	auto attach_image(
		data& d,
		shared_view<asset::data> assets,
		window::clipboard_image pasted
	) -> void;

	auto agent_context_tag() -> id;

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

	auto draw_transcript(
		gui::builder& ui,
		data& d,
		const rectf& area,
		channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out
	) -> void;

	auto draw_attachments(
		gui::builder& ui,
		session& s,
		const rectf& area
	) -> void;

	auto draw_model_controls(
		gui::builder& ui,
		session& s,
		const rectf& model_rect,
		const rectf& effort_rect
	) -> void;

	auto draw_input(
		gui::builder& ui,
		session& s,
		const rectf& area
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

	auto draw_activity(
		const gui::draw_context& ctx,
		const data& d,
		const session& s,
		const rectf& area
	) -> void;
}
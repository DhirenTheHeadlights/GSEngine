export module gse.graphics:gui_chrome;

import std;

import gse.core;
import gse.ecs;
import gse.math;

import :gui;
import :types;
import :styles;
import :builder;
import :render_layer;
import :symbols;

namespace gse::gui {
	[[nodiscard]] auto has_side_accent(
		const menu& m
	) -> bool;

	[[nodiscard]] auto is_floating_menu(
		const menu& m
	) -> bool;

	[[nodiscard]] auto tab_group_grab_extent(
		const menu& m,
		const style& sty
	) -> float;

	[[nodiscard]] auto popout_close_button_rect(
		const rectf& title_bar_rect,
		const style& sty
	) -> rectf;

	struct window_caption_buttons {
		rectf minimize;
		rectf maximize;
		rectf close;

		[[nodiscard]] auto extent() const -> float;
	};

	[[nodiscard]] auto window_caption_buttons_for(
		const rectf& title_bar_rect,
		const style& sty
	) -> window_caption_buttons;

	[[nodiscard]] auto window_caption_grip_rect(
		const font_set& fonts,
		const menu& m,
		const style& sty,
		const rectf& title_bar_rect
	) -> rectf;

	auto draw_window_caption_buttons(
		data& d,
		viewport_state& vp,
		const input::state& input_state,
		const menu& current_menu,
		const rectf& title_bar_rect,
		render_layer layer
	) -> void;

	auto draw_dock_space(
		data& d,
		const style& sty,
		const dock::space& space
	) -> void;

	auto remove_tab_from_host(
		viewport_state& vp,
		std::string_view menu_name
	) -> void;

	auto draw_menu_chrome(
		data& d,
		viewport_state& vp,
		const input::state& input_state,
		menu& current_menu,
		render_layer layer
	) -> void;

	auto draw_tab_bar(
		data& d,
		viewport_state& vp,
		const input::state& input_state,
		menu& current_menu,
		const rectf& title_bar_rect,
		render_layer layer
	) -> void;
}

export namespace gse::gui {
	[[nodiscard]] auto menu_chrome_height(
		const font_set& fonts,
		const menu& m,
		const style& sty,
		float width
	) -> float;

	[[nodiscard]] auto tab_index_at(
		const font_set& fonts,
		const menu& m,
		const style& sty,
		const rectf& title_bar_rect,
		vec2f mouse
	) -> std::optional<std::uint32_t>;

	auto caption_button(
		builder& b,
		const rectf& rect,
		const std::string& key,
		std::span<const symbol::stroke> glyph,
		vec4f hover_color,
		bool enabled = true
	) -> bool;
}

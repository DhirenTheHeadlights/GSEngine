export module gse.graphics:gui_screen;

import std;

import gse.os;
import gse.core;
import gse.ecs;
import gse.math;

import :gui;
import :types;
import :builder;
import :menu_stack;
import :ui_renderer;

namespace gse::gui {
	auto draw_screen_caption(
		builder& b,
		screen& top,
		const rectf& bar_rect,
		const rectf& full_rect,
		channel_write<ui_focus_request, popout_toggle, set_cursor_shape_request, renderer::sprite_command, renderer::text_command, context_menu_result, window_close_request, window_minimize_request, window_toggle_maximize_request, window_chrome_metrics_request> channels
	) -> void;

	auto process_screen(
		data& d,
		viewport_state& vp,
		const input::state& input_state,
		vec2f viewport_size,
		channel_write<ui_focus_request, popout_toggle, set_cursor_shape_request, renderer::sprite_command, renderer::text_command, context_menu_result, window_close_request, window_minimize_request, window_toggle_maximize_request, window_chrome_metrics_request> channels
	) -> void;
}

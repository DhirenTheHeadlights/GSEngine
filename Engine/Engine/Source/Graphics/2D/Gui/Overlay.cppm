export module gse.graphics:gui_overlay;

import std;

import gse.os;
import gse.core;
import gse.ecs;
import gse.math;

import :gui;
import :types;
import :menu_stack;
import :ui_renderer;

namespace gse::gui {
	auto draw_drag_ghost(
		data& d,
		viewport_state& vp
	) -> void;

	auto update_tooltip(
		data& d,
		viewport_state& vp
	) -> void;

	auto process_context_menu(
		data& d,
		viewport_state& vp,
		const input::state& input_state,
		channel_write<ui_focus_request, popout_toggle, set_cursor_shape_request, renderer::sprite_command, renderer::text_command, context_menu_result, window_close_request, window_minimize_request, window_toggle_maximize_request, window_chrome_metrics_request, window_panel_drag_request> channels
	) -> void;
}

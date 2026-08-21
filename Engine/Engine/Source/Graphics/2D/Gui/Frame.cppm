export module gse.graphics:gui_frame;

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
	auto begin_viewport_frame(
		data& d,
		viewport_state& vp,
		shared_view<window::data> window_s,
		vec2f viewport_size
	) -> void;

	auto update_viewport_interaction(
		data& d,
		viewport_state& vp,
		shared_view<window::data> window_s,
		shared_view<input::data> input_state
	) -> void;

	auto update_viewport(
		data& d,
		viewport_state& vp,
		const input::state& input_st,
		vec2f viewport_size,
		channel_read<push_screen_request, pop_screen_request, clear_screens_request, set_manual_cursor_request, menu_content, popout_closed> requests_in,
		channel_write<ui_focus_request, popout_toggle, set_cursor_shape_request, renderer::sprite_command, renderer::text_command, context_menu_result, window_close_request, window_minimize_request, window_toggle_maximize_request, window_chrome_metrics_request> ui_out
	) -> void;
}

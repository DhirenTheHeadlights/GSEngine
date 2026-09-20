export module gse.graphics:gui_screen;

import gse.math;
import gse.os;

import :builder;
import :gui;
import :menu_stack;
import :types;

namespace gse::gui {
	auto draw_screen_caption(
		builder& b,
		viewport_state& vp,
		screen& top,
		const rectf& bar_rect,
		const rectf& full_rect
	) -> void;

	auto process_screen(
		data& d,
		viewport_state& vp,
		const input::state& input_state,
		vec2f viewport_size
	) -> void;
}
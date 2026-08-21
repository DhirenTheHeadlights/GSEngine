export module gse.graphics:gui_drag_resize;

import std;

import gse.os;
import gse.core;
import gse.ecs;
import gse.math;

import :gui;
import :types;
import :styles;

namespace gse::gui {
	auto handle_idle_state(
		const font_set& fonts,
		viewport_state& vp,
		const input::state& input_state,
		vec2f mouse_position,
		bool mouse_held,
		const style& style
	) -> state;

	auto handle_dragging_state(
		viewport_state& vp,
		const states::dragging& current,
		shared_view<window::data> window_s,
		vec2f mouse_position,
		bool mouse_held
	) -> state;

	auto handle_resizing_state(
		viewport_state& vp,
		const states::resizing& current,
		vec2f mouse_position,
		bool mouse_held,
		const style& style,
		shared_view<window::data> window_s
	) -> state;

	auto handle_resizing_divider_state(
		viewport_state& vp,
		const states::resizing_divider& current,
		vec2f mouse_position,
		bool mouse_held,
		const style& style
	) -> state;

	auto handle_pending_drag_state(
		viewport_state& vp,
		const states::pending_drag& current,
		vec2f mouse_position,
		bool mouse_held
	) -> state;
}

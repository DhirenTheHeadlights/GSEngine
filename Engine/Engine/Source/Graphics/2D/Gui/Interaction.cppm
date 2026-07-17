export module gse.graphics:interaction;

import std;

import gse.core;
import gse.time;
import gse.math;

export namespace gse::gui::interaction {
	struct click_state {
		clock since_last;
		vec2f last_pos{};
		int count = 0;
	};

	auto register_click(
		click_state& state,
		vec2f pos
	) -> int;

	auto mark_hot(
		id& hot,
		id widget,
		bool hovered
	) -> void;
	auto grab_active(
		id& active,
		id widget,
		bool pressed_on_widget
	) -> void;
	auto release_active(
		id& active,
		id widget,
		bool released
	) -> bool;
	auto release_active(
		id& active,
		std::span<const id> owned,
		bool released
	) -> bool;
	auto activate_on_click(
		id& active,
		id widget,
		bool hovered,
		bool pressed_on_widget,
		bool released
	) -> bool;
}

auto gse::gui::interaction::register_click(click_state& state, const vec2f pos) -> int {
	constexpr time multi_click_interval = milliseconds(400);
	constexpr float multi_click_slop = 4.f;
	const auto elapsed = state.since_last.reset();
	const bool near_last = elapsed <= multi_click_interval
		&& std::abs(pos.x() - state.last_pos.x()) <= multi_click_slop
		&& std::abs(pos.y() - state.last_pos.y()) <= multi_click_slop;
	state.count = near_last ? state.count % 3 + 1 : 1;
	state.last_pos = pos;
	return state.count;
}

auto gse::gui::interaction::mark_hot(id& hot, const id widget, const bool hovered) -> void {
	if (hovered) {
		hot = widget;
	}
}

auto gse::gui::interaction::grab_active(id& active, const id widget, const bool pressed_on_widget) -> void {
	if (pressed_on_widget && !active.exists()) {
		active = widget;
	}
}

auto gse::gui::interaction::release_active(id& active, const id widget, const bool released) -> bool {
	if (released && active == widget) {
		active = {};
		return true;
	}
	return false;
}

auto gse::gui::interaction::release_active(id& active, const std::span<const id> owned, const bool released) -> bool {
	if (!released) {
		return false;
	}
	for (const id w : owned) {
		if (active == w) {
			active = {};
			return true;
		}
	}
	return false;
}

auto gse::gui::interaction::activate_on_click(id& active, const id widget, const bool hovered, const bool pressed_on_widget, const bool released) -> bool {
	grab_active(active, widget, pressed_on_widget);
	return release_active(active, widget, released) && hovered;
}

export module gse.graphics:interaction;

import std;

import gse.core;
import gse.time;
import gse.math;
import :types;
import :widget_skin;

export namespace gse::gui::interaction {
	struct click_state {
		clock since_last;
		vec2f last_pos{};
		int count = 0;
	};

	struct press {
		id widget;
		bool enabled = true;
		bool hovered = false;
		bool held = false;
		bool activated = false;
		bool selected = false;

		template <typename T>
		[[nodiscard]] auto select(
			const state_set<T>& states
		) const -> T;
	};

	struct press_info {
		rectf rect;
		bool enabled = true;
		bool selected = false;
	};

	auto register_click(
		click_state& state,
		vec2f pos
	) -> int;

	auto press_in_rect(
		const draw_context& ctx,
		id& hot,
		id& active,
		id widget,
		const press_info& info
	) -> press;

	struct dismiss_info {
		rectf body;
		std::span<const rectf> keep_open{};
		bool suppressed = false;
	};

	[[nodiscard]] auto dismissed_by_outside_press(
		const draw_context& ctx,
		const dismiss_info& info
	) -> bool;
}

namespace gse::gui::interaction {
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

	struct press_signals {
		bool hovered = false;
		bool pressed_on_widget = false;
		bool released = false;
		bool enabled = true;
		bool selected = false;
	};

	auto press_from(
		id& hot,
		id& active,
		id widget,
		const press_signals& signals
	) -> press;
}

auto gse::gui::interaction::register_click(click_state& state, const vec2f pos) -> int {
	constexpr time multi_click_interval = milliseconds(400.f);
	constexpr float multi_click_slop = 4.f;
	const auto elapsed = state.since_last.reset();
	const bool near_last = elapsed <= multi_click_interval
		&& std::abs(pos.x() - state.last_pos.x()) <= multi_click_slop
		&& std::abs(pos.y() - state.last_pos.y()) <= multi_click_slop;
	state.count = near_last ? state.count % 3 + 1 : 1;
	state.last_pos = pos;
	return state.count;
}

template <typename T>
auto gse::gui::interaction::press::select(const state_set<T>& states) const -> T {
	if (!enabled) {
		return states.disabled;
	}
	if (selected) {
		return states.selected;
	}
	if (held) {
		return states.active;
	}
	if (hovered) {
		return states.hot;
	}
	return states.idle;
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

auto gse::gui::interaction::press_from(id& hot, id& active, const id widget, const press_signals& signals) -> press {
	const bool live = signals.enabled && signals.hovered;
	mark_hot(hot, widget, live);
	const bool activated = activate_on_click(active, widget, live, signals.enabled && signals.pressed_on_widget, signals.released);
	return {
		.widget = widget,
		.enabled = signals.enabled,
		.hovered = live,
		.held = signals.enabled && active == widget,
		.activated = activated,
		.selected = signals.selected,
	};
}

auto gse::gui::interaction::press_in_rect(const draw_context& ctx, id& hot, id& active, const id widget, const press_info& info) -> press {
	return press_from(hot, active, widget, {
		.hovered = ctx.hovers(info.rect),
		.pressed_on_widget = ctx.mouse_pressed_for(info.rect),
		.released = ctx.mouse_released(),
		.enabled = info.enabled,
		.selected = info.selected,
	});
}

auto gse::gui::interaction::dismissed_by_outside_press(const draw_context& ctx, const dismiss_info& info) -> bool {
	if (info.suppressed || !ctx.mouse_pressed()) {
		return false;
	}
	const vec2f position = ctx.mouse_position();
	if (info.body.contains(position)) {
		return false;
	}
	return !std::ranges::any_of(info.keep_open, [position](const rectf& rect) {
		return rect.contains(position);
	});
}

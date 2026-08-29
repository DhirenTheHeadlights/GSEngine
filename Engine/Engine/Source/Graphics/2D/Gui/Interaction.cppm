export module gse.graphics:interaction;

import std;

import gse.core;
import gse.time;
import gse.math;
import :types;

export namespace gse::gui::interaction {
	struct click_state {
		clock since_last;
		vec2f last_pos{};
		int count = 0;
	};

	struct press_palette {
		vec4f idle;
		vec4f hot;
		vec4f active;
		vec4f disabled;
	};

	struct press {
		id widget;
		bool enabled = true;
		bool hovered = false;
		bool held = false;
		bool activated = false;

		[[nodiscard]] auto color(
			const press_palette& colors
		) const -> vec4f;
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
		const rectf& rect,
		bool enabled = true
	) -> press;
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

	auto press_from(
		id& hot,
		id& active,
		id widget,
		bool hovered,
		bool pressed_on_widget,
		bool released,
		bool enabled
	) -> press;
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

auto gse::gui::interaction::press::color(const press_palette& colors) const -> vec4f {
	if (!enabled) {
		return colors.disabled;
	}
	if (held) {
		return colors.active;
	}
	if (hovered) {
		return colors.hot;
	}
	return colors.idle;
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

auto gse::gui::interaction::press_from(id& hot, id& active, const id widget, const bool hovered, const bool pressed_on_widget, const bool released, const bool enabled) -> press {
	const bool live = enabled && hovered;
	mark_hot(hot, widget, live);
	const bool activated = activate_on_click(active, widget, live, enabled && pressed_on_widget, released);
	return {
		.widget = widget,
		.enabled = enabled,
		.hovered = live,
		.held = enabled && active == widget,
		.activated = activated,
	};
}

auto gse::gui::interaction::press_in_rect(const draw_context& ctx, id& hot, id& active, const id widget, const rectf& rect, const bool enabled) -> press {
	return press_from(hot, active, widget, ctx.hovers(rect), ctx.mouse_pressed_for(rect), ctx.mouse_released(), enabled);
}

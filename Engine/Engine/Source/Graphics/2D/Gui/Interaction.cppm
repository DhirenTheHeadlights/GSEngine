export module gse.graphics:interaction;

import std;

import gse.core;

export namespace gse::gui::interaction {
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

export module gse.runtime:gui_api;

import std;

import gse.graphics;

import :core_api;

export namespace gse::gui {
	auto panel(
		const std::string& name,
		const std::function<void(builder&)>& build
	) -> void;

	auto panel(
		const std::string& name,
		int priority,
		const std::function<void(builder&)>& build
	) -> void;
}

auto gse::gui::panel(const std::string& name, const std::function<void(builder&)>& build) -> void {
	channel_add(menu_content{ .menu = name, .build = build });
}

auto gse::gui::panel(const std::string& name, const int priority, const std::function<void(builder&)>& build) -> void {
	channel_add(menu_content{ .menu = name, .priority = priority, .build = build });
}

export module gse.graphics:gui_menu;

import std;

import gse.core;
import gse.ecs;
import gse.math;

import :gui;
import :types;
import :builder;
import :render_layer;

namespace gse::gui {
	auto begin_menu(
		viewport_state& vp,
		const std::string& name
	) -> bool;

	auto end_menu(
		viewport_state& vp
	) -> void;

	auto calculate_display_rect(
		viewport_state& vp,
		const menu& m
	) -> rectf;

	auto process_menu(
		data& d,
		viewport_state& vp,
		const input::state& input_state,
		const std::string& name,
		render_layer layer,
		const std::function<void(builder&)>& build
	) -> void;
}

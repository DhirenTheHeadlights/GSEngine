export module gse.graphics:gui_hud;

import std;

import gse.core;
import gse.ecs;
import gse.os;

import :builder;
import :gui;

namespace gse::gui {
	auto process_hud(
		context& run_ctx,
		data& d,
		viewport_state& vp,
		const input::state& input_state,
		std::span<const hud_content> contents
	) -> void;
}

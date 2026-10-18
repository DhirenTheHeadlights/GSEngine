export module gse.graphics:gui_hud;

import std;

import gse.os;

import :builder;
import :gui;

namespace gse::gui {
	auto process_hud(
		data& d,
		viewport_state& vp,
		const input::state& input_state,
		std::span<const hud_content> contents
	) -> void;
}

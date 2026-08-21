export module gse.graphics:gui_scale;

import std;

import gse.os;
import gse.assets;
import gse.core;
import gse.ecs;
import gse.math;
import gse.meta;

import :gui;
import :types;
import :styles;

namespace gse::gui {
	[[nodiscard]] auto intern_text(
		data& d,
		std::string_view text
	) -> std::string_view;

	auto usable_screen_rect(
		float top_inset,
		shared_view<window::data> window_s
	) -> rectf;

	auto sync_monitor_scale(
		data& d,
		viewport_state& vp,
		const std::string& monitor_key
	) -> void;

	auto scale_factor_for(
		const data& d,
		const viewport_state& vp,
		float viewport_height
	) -> float;

	auto reload_font(
		data& d,
		shared_view<asset::data> assets
	) -> void;
}

export namespace gse::gui {
	auto apply_scale(
		const data& d,
		const viewport_state& vp,
		style sty,
		float viewport_height
	) -> style;
}

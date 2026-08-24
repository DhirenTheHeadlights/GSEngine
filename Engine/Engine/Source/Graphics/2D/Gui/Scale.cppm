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
		const rectf& frame_rect
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

	auto font_available(
		std::span<const std::string> available,
		std::string_view name
	) -> bool;

	auto variant_of(
		std::span<const std::string> available,
		std::string_view base,
		std::span<const std::string_view> suffixes
	) -> std::string;

	auto assign_faces(
		font_set& fonts,
		shared_view<asset::data> assets,
		const std::string& ui_name,
		const std::string& code_name
	) -> void;

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

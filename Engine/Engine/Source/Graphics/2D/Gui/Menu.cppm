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
	[[nodiscard]] auto hosts_menu(
		const viewport_state& vp,
		std::string_view name
	) -> bool;

	[[nodiscard]] auto claims_content(
		const data& d,
		const viewport_state& vp,
		std::string_view name
	) -> bool;

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

	auto update_menu_zoom(
		viewport_state& vp,
		menu& m,
		const input::state& input_state,
		const rectf& display_rect,
		render_layer layer,
		std::uint32_t menu_z
	) -> void;

	auto process_menu(
		context& run_ctx,
		data& d,
		viewport_state& vp,
		const input::state& input_state,
		const std::string& name,
		render_layer layer,
		bool translucent,
		const draw_fn& build
	) -> void;
}

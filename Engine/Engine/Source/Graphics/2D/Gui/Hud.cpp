module gse.graphics:gui_hud_impl;

import gse.assets;
import gse.concurrency;
import gse.config;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.gpu;
import gse.math;
import gse.meta;
import gse.os;
import gse.save;
import gse.time;
import std;

import :builder;
import :gui;
import :gui_hud;
import :ids;
import :styles;
import :types;
import :widget_context;

auto gse::gui::process_hud(context& run_ctx, data& d, viewport_state& vp, const input::state& input_state, const std::span<const hud_content> contents) -> void {
	if (!vp.fstate.active) {
		return;
	}

	if (contents.empty()) {
		return;
	}

	std::vector<const hud_content*> ordered;
	ordered.reserve(contents.size());
	for (const hud_content& content : contents) {
		ordered.push_back(&content);
	}
	std::ranges::sort(ordered, {}, [](const hud_content* content) -> std::string_view {
		return content->name;
	});

	const rectf surface_rect = vp.frame_rect;

	if (!vp.hud_surface) {
		vp.hud_surface.emplace(
			"__gui_hud__",
			menu_data{
				.rect = surface_rect,
				.parent_id = id(),
			}
		);
	}
	vp.hud_surface->rect = surface_rect;

	std::uint32_t z_order = 0;

	for (const hud_content* content : ordered) {
		vec2f layout_cursor = surface_rect.top_left();

		ids::scope _(content->name);

		widget_context ctx{ widget_context_init(d, vp, content->layer, ++z_order, {
			.current_menu = *vp.hud_surface,
			.style = vp.fstate.sty,
			.layout_cursor = layout_cursor,
			.clip = surface_rect,
		}), input_state };

		d.context = &ctx;

		builder b{
			.ctx = ctx,
			.hot_widget_id = vp.hot_widget_id,
			.active_widget_id = vp.active_widget_id,
			.focus_widget_id = vp.focus_widget_id,
		};

		content->build(b, run_ctx);

		d.context = nullptr;
	}
}

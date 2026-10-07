export module gse.graphics:modal_surface;

import gse.core;
import gse.math;
import std;

import :render_layer;
import :styles;
import :types;

namespace gse::gui {
	struct modal_surface {
		rectf host;
		rectf panel;
	};

	[[nodiscard]] auto draw_modal_surface(
		const draw_context& ctx,
		const modal_surface& surface
	) -> layer_scope;
}

auto gse::gui::draw_modal_surface(const draw_context& ctx, const modal_surface& surface) -> layer_scope {
	const style& sty = ctx.style;

	layer_scope scope = ctx.scoped_layer(render_layer::modal);

	ctx.register_hit_region(render_layer::modal, surface.host);
	ctx.queue_sprite({
		.rect = surface.host,
		.color = sty.color_scrim,
		.texture = ctx.blank_texture,
	});
	ctx.queue_sprite({
		.rect = rectf::from_position_size(
			{ surface.panel.left() + sty.modal_shadow_offset, surface.panel.top() - sty.modal_shadow_offset },
			{ surface.panel.width(), surface.panel.height() }
		),
		.color = sty.color_shadow,
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius_menu,
	});
	ctx.queue_sprite({
		.rect = surface.panel,
		.color = { vec3f(sty.color_menu_body), 1.f },
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius_menu,
	});

	return scope;
}

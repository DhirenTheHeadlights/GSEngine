export module gse.graphics:modal_surface;

import gse.core;
import gse.math;
import std;

import :render_layer;
import :styles;
import :surface_paint;
import :types;
import :widget_skin;

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

	const modal_skin& skin = sty.modal;

	ctx.register_hit_region(render_layer::modal, surface.host);

	draw::fill_surface(ctx, surface.host, skin.scrim, {});

	draw::fill_surface(
		ctx,
		rectf::from_position_size(
			{ surface.panel.left() + sty.modal_shadow_offset, surface.panel.top() - sty.modal_shadow_offset },
			{ surface.panel.width(), surface.panel.height() }
		),
		skin.shadow,
		{}
	);

	draw::fill_surface(ctx, surface.panel, skin.panel, {});

	return scope;
}

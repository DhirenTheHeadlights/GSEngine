export module gse.graphics:separator_widget;

import gse.core;
import gse.math;

import :styles;
import :surface_paint;
import :types;
import :widget_skin;

export namespace gse::gui {
	struct separator {
		using result = void;
		static auto draw(
			const draw_context& ctx,
			id&,
			id&,
			id&
		) -> void;
	};
}

auto gse::gui::separator::draw(const draw_context& ctx, id&, id&, id&) -> void {
	if (!ctx.current_menu) {
		return;
	}

	const rectf content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });
	const float half_spacing = ctx.style.padding * 0.5f;

	ctx.layout_cursor.y() -= half_spacing;

	const float line_height = ctx.style.separator_thickness;
	const rectf line_rect = rectf::from_position_size(
		{ content_rect.left(), ctx.layout_cursor.y() },
		{ content_rect.width(), line_height }
	);

	draw::fill_surface(ctx, line_rect, ctx.style.separator_line, {});

	ctx.layout_cursor.y() -= line_height + half_spacing;
}
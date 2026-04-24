export module gse.graphics:separator_widget;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import :types;
import :styles;
import :builder;

export namespace gse::gui {
	struct separator {
		using result = void;
		static auto draw(const draw_context& ctx, id&, id&, id&) -> void;
	};
}

auto gse::gui::separator::draw(const draw_context& ctx, id&, id&, id&) -> void {
	if (!ctx.current_menu) {
		return;
	}

	const ui_rect content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });
	const float half_spacing = ctx.style.padding * 0.5f;

	ctx.layout_cursor.y() -= half_spacing;

	const float line_height = 1.f;
	const ui_rect line_rect = ui_rect::from_position_size(
		{ content_rect.left(), ctx.layout_cursor.y() },
		{ content_rect.width(), line_height }
	);

	ctx.queue_sprite({
		.rect = line_rect,
		.color = ctx.style.color_border,
		.texture = ctx.blank_texture
	});

	ctx.layout_cursor.y() -= line_height + half_spacing;
}

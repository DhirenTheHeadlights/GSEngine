export module gse.graphics:section_widget;

import std;

import gse.utility;

import :types;
import :styles;
import :builder;

export namespace gse::gui {
	struct section {
		using result = void;
		struct params {
			std::string_view title;
		};
		static auto draw(const draw_context& ctx, params p, id&, id&, id&) -> void;
	};
}

auto gse::gui::section::draw(const draw_context& ctx, const params p, id&, id&, id&) -> void {
	if (!ctx.current_menu) {
		return;
	}

	const ui_rect content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });

	ctx.layout_cursor.y() -= ctx.style.section_spacing;

	const float text_height = ctx.font->line_height(ctx.style.font_size);

	ctx.queue_text({
		.font = ctx.font,
		.text = std::string(p.title),
		.position = {
			content_rect.left(),
			ctx.layout_cursor.y() + ctx.style.font_size / 2.f
		},
		.scale = ctx.style.font_size,
		.color = ctx.style.color_text_secondary,
		.clip_rect = content_rect
	});

	ctx.layout_cursor.y() -= text_height;

	constexpr float line_height = 1.f;
	const ui_rect line_rect = ui_rect::from_position_size(
		{ content_rect.left(), ctx.layout_cursor.y() },
		{ content_rect.width(), line_height }
	);

	ctx.queue_sprite({
		.rect = line_rect,
		.color = ctx.style.color_border,
		.texture = ctx.blank_texture
	});

	ctx.layout_cursor.y() -= line_height + ctx.style.padding * 0.5f;
}

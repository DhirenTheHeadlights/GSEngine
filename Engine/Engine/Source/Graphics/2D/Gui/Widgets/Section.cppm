export module gse.graphics:section_widget;

import std;

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
	struct section {
		using result = void;
		struct params {
			std::string_view title;
			std::string_view subtitle = {};
		};
		static auto draw(
			const draw_context& ctx,
			params p,
			id&,
			id&,
			id&
		) -> void;
	};
}

auto gse::gui::section::draw(const draw_context& ctx, const params p, id&, id&, id&) -> void {
	if (!ctx.current_menu) {
		return;
	}

	const ui_rect content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });

	ctx.layout_cursor.y() -= ctx.style.section_spacing_above;

	const float header_size = ctx.style.font_size * ctx.style.section_header_size_mult;
	const float title_top = ctx.layout_cursor.y();
	const float bar_height = ctx.font->ascender_height(header_size);

	const ui_rect bar_rect =
		ui_rect::from_position_size({ content_rect.left(), title_top }, { ctx.style.accent_bar_width, bar_height });

	ctx.queue_sprite({
		.rect = bar_rect,
		.color = ctx.style.color_accent,
		.texture = ctx.blank_texture,
		.corner_radius = ctx.style.accent_bar_width * 0.5f,
	});

	const float text_left = content_rect.left() + ctx.style.accent_bar_width + ctx.style.padding * 0.6f;

	ctx.queue_text({
		.font = ctx.font,
		.text = std::string(p.title),
		.position = { text_left, title_top },
		.scale = header_size,
		.color = ctx.style.color_section_header,
		.clip_rect = content_rect,
	});

	ctx.layout_cursor.y() -= ctx.font->line_height(header_size);

	if (!p.subtitle.empty()) {
		ctx.layout_cursor.y() -= 4.f;
		ctx.queue_text({
			.font = ctx.font,
			.text = std::string(p.subtitle),
			.position = { text_left, ctx.layout_cursor.y() },
			.scale = ctx.style.font_size,
			.color = ctx.style.color_text_secondary,
			.clip_rect = content_rect,
		});
		ctx.layout_cursor.y() -= ctx.font->line_height(ctx.style.font_size);
	}

	ctx.layout_cursor.y() -= ctx.style.section_spacing_below;
}

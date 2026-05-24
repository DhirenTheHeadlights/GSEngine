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
import :layout_ops;
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

	const auto& sty = ctx.style;
	namespace lo = layout;

	const ui_rect content_rect = ctx.current_menu->rect.inset({ sty.padding, sty.padding });

	lo::skip(ctx, sty.section_spacing_above);

	const float header_size = sty.font_size * sty.section_header_size_mult;
	const ui_rect header_row = lo::reserve_row(ctx, ctx.font->line_height(header_size));
	const float title_top = header_row.top();
	const float bar_height = ctx.font->line_height(header_size);

	const ui_rect bar_rect = ui_rect::from_position_size(
		{ content_rect.left(), title_top },
		{ sty.accent_bar_width, bar_height }
	);

	ctx.queue_sprite({
		.rect = bar_rect,
		.color = sty.color_accent,
		.texture = ctx.blank_texture,
		.corner_radius = sty.accent_bar_width * 0.5f,
	});

	const float text_left = content_rect.left() + sty.accent_bar_width + sty.padding * 0.6f;

	ctx.queue_text({
		.font = ctx.font,
		.text = std::string(p.title),
		.position = { text_left, title_top },
		.scale = header_size,
		.color = sty.color_section_header,
		.clip_rect = content_rect,
	});

	if (!p.subtitle.empty()) {
		lo::skip(ctx, sty.item_spacing);
		const ui_rect subtitle_row = lo::reserve_row(ctx, ctx.font->line_height(sty.font_size));
		ctx.queue_text({
			.font = ctx.font,
			.text = std::string(p.subtitle),
			.position = { text_left, subtitle_row.top() },
			.scale = sty.font_size,
			.color = sty.color_text_secondary,
			.clip_rect = content_rect,
		});
	}

	lo::skip(ctx, sty.section_spacing_below);
}

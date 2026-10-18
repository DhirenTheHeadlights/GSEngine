export module gse.graphics:nav_item_widget;

import gse.assets;
import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.gpu;
import gse.math;
import gse.os;
import gse.time;
import std;

import :font;
import :ids;
import :interaction;
import :styles;
import :surface_paint;
import :types;
import :widget_skin;

export namespace gse::gui {
	struct nav_item {
		using result = bool;
		struct params {
			std::string_view text;
			bool selected = false;
			resource::handle<font> font{};
		};
		static auto draw(
			const draw_context& ctx,
			const params& p,
			id& hot,
			id& active,
			id&
		) -> bool;
	};
}

auto gse::gui::nav_item::draw(const draw_context& ctx, const params& p, id& hot, id& active, id&) -> bool {
	const auto fnt = ctx.fonts.face_or(p.font);
	const auto fnt_view = fnt.resolve();
	if (!ctx.current_menu) {
		return false;
	}

	const id widget_id = ids::make(p.text);

	const float widget_height = fnt_view->line_height(ctx.style.font_size) + ctx.style.padding * 0.8f;
	const rectf content_rect = ctx.current_menu->rect.inset({ ctx.style.padding * 0.5f, 0.f });

	const rectf row_rect = rectf::from_position_size(
		{ content_rect.left(), ctx.layout_cursor.y() },
		{ content_rect.width(), widget_height }
	);

	const item_skin& skin = ctx.style.nav_item;

	const interaction::press row = interaction::press_in_rect(ctx, hot, active, widget_id, {
		.rect = row_rect,
		.selected = p.selected,
	});

	draw::fill_surface(ctx, row_rect, row.select(skin.surfaces), {
		.anim = widget_id,
	});

	if (p.selected) {
		const float bar_h = widget_height * 0.55f;
		draw::fill_surface(
			ctx,
			rectf::from_position_size(
				{ row_rect.left(), row_rect.center().y() + bar_h * 0.5f },
				{ ctx.style.accent_bar_width, bar_h }
			),
			skin.accent_bar,
			{}
		);
	}

	draw::label(ctx, row_rect.inset({ ctx.style.padding, 0.f }), p.text, skin.label, {
		.color = row.select(skin.label_colors),
		.font = p.font,
		.clip_rect = row_rect,
	});

	ctx.layout_cursor.y() -= widget_height + ctx.style.item_spacing;

	return row.activated;
}
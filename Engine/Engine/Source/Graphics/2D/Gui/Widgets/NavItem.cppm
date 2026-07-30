export module gse.graphics:nav_item_widget;

import std;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import :types;
import :font;
import :ids;
import :styles;
import :builder;
import :interaction;

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
	const auto fnt = p.font.valid() ? p.font : ctx.fonts.text;
	if (!ctx.current_menu) {
		return false;
	}

	const id widget_id = ids::make(p.text);

	const float widget_height = fnt->line_height(ctx.style.font_size) + ctx.style.padding * 0.8f;
	const rectf content_rect = ctx.current_menu->rect.inset({ ctx.style.padding * 0.5f, 0.f });

	const rectf row_rect = rectf::from_position_size(
		{ content_rect.left(), ctx.layout_cursor.y() },
		{ content_rect.width(), widget_height }
	);

	const bool hovered = row_rect.contains(ctx.input.mouse_position()) && ctx.input_available();
	const bool released = ctx.input.mouse_button_released(mouse_button::button_1);

	interaction::mark_hot(hot, widget_id, hovered);
	const bool activated = interaction::activate_on_click(active, widget_id, hovered, ctx.mouse_pressed_for(row_rect), released);

	vec4f target_bg{ 0.f, 0.f, 0.f, 0.f };
	if (p.selected) {
		target_bg = ctx.style.color_accent_dim;
	}
	else if (active == widget_id) {
		target_bg = ctx.style.color_widget_hovered;
	}
	else if (hot == widget_id) {
		target_bg = ctx.style.color_widget_hovered;
		target_bg.w() *= 0.55f;
	}

	ctx.queue_sprite({
		.rect = row_rect,
		.color = ctx.animated_color(widget_id, target_bg),
		.texture = ctx.blank_texture,
		.corner_radius = ctx.style.corner_radius,
	});

	if (p.selected) {
		const float bar_h = widget_height * 0.55f;
		const rectf bar_rect = rectf::from_position_size(
			{ row_rect.left(), row_rect.center().y() + bar_h * 0.5f },
			{ ctx.style.accent_bar_width, bar_h }
		);
		ctx.queue_sprite({
			.rect = bar_rect,
			.color = ctx.style.color_accent,
			.texture = ctx.blank_texture,
			.corner_radius = ctx.style.accent_bar_width * 0.5f,
		});
	}

	const vec4f text_color = p.selected ? ctx.style.color_text : ctx.style.color_text_secondary;
	const float text_x = row_rect.left() + ctx.style.padding;
	ctx.queue_text({
		.font = fnt,
		.text = p.text,
		.position = { text_x, row_rect.center().y() + fnt->vertical_center_offset(ctx.style.font_size) },
		.scale = ctx.style.font_size,
		.color = text_color,
		.clip_rect = row_rect,
	});

	ctx.layout_cursor.y() -= widget_height + ctx.style.item_spacing;

	return activated;
}

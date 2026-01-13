export module gse.graphics:selectable_widget;

import std;

import gse.platform;
import gse.utility;

import :types;
import :ids;

export namespace gse::gui::draw {
	auto selectable(
		const draw_context& ctx,
		const std::string& name,
		bool selected,
		id& hot_widget_id,
		id& active_widget_id
	) -> bool;
}

auto gse::gui::draw::selectable(const draw_context& ctx, const std::string& name, const bool selected, id& hot_widget_id, id& active_widget_id) -> bool {
	if (!ctx.current_menu) {
		return false;
	}

	const id widget_id = ids::make(name);

	const float widget_height = ctx.font->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
	const ui_rect content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });
	const ui_rect row_rect = ui_rect::from_position_size(
		{ content_rect.left(), ctx.layout_cursor.y() },
		{ content_rect.width(), widget_height }
	);

	const bool hovered = row_rect.contains(ctx.input.mouse_position());

	if (hovered) {
		hot_widget_id = widget_id;
	}

	if (hovered && ctx.input.mouse_button_pressed(mouse_button::button_1)) {
		active_widget_id = widget_id;
	}

	unitless::vec4 bg = ctx.style.color_widget_background;
	if (selected || active_widget_id == widget_id) {
		bg = ctx.style.color_widget_fill;
	} else if (hot_widget_id == widget_id) {
		bg = ctx.style.color_dock_widget;
	}

	ctx.queue_sprite({
		.rect = row_rect,
		.color = bg,
		.texture = ctx.blank_texture
	});

	const float text_w = ctx.font->width(name, ctx.style.font_size);
	const unitless::vec2 text_pos = {
		row_rect.center().x() - text_w / 2.f,
		row_rect.center().y() + ctx.style.font_size / 2.f
	};

	ctx.queue_text({
		.font = ctx.font,
		.text = name,
		.position = text_pos,
		.scale = ctx.style.font_size,
		.clip_rect = row_rect
	});

	ctx.layout_cursor.y() -= widget_height + ctx.style.padding;

	bool clicked = false;
	if (ctx.input.mouse_button_released(mouse_button::button_1) && active_widget_id == widget_id) {
		clicked = hovered;
		active_widget_id = {};
	}

	return clicked;
}
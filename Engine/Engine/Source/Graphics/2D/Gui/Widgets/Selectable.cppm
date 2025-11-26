export module gse.graphics:selectable_widget;

import std;

import gse.platform;

import :types;
import :ids;
import :text_renderer;

export namespace gse::gui::draw {
    auto selectable(
        const widget_context& context,
        const std::string& name,
        bool selected,
        id& hot_widget_id,
        id active_widget_id
    ) -> void;
}

auto gse::gui::draw::selectable(const widget_context& context, const std::string& name, const bool selected, id& hot_widget_id, id active_widget_id) -> void {
    if (!context.current_menu) return;

    const auto widget_id = ids::make(name);

    const float widget_height = context.font->line_height(context.style.font_size) + context.style.padding * 0.5f;
    const auto content_rect = context.current_menu->rect.inset({ context.style.padding, context.style.padding });

    const ui_rect row_rect = ui_rect::from_position_size(
        { content_rect.left(), context.layout_cursor.y() },
        { content_rect.width(), widget_height }
    );

    if (row_rect.contains(mouse::position())) {
        hot_widget_id = widget_id;
    }

    unitless::vec4 bg = context.style.color_widget_background;
    if (selected) {
        bg = context.style.color_widget_fill;
    }
    else if (hot_widget_id == widget_id) {
        bg = context.style.color_dock_widget;
    }
    if (active_widget_id == widget_id) {
        bg = context.style.color_widget_fill;
    }

    context.sprite_renderer.queue({
		.rect = row_rect,
		.color = bg,
		.texture = context.blank_texture
    });

    const float text_w = context.font->width(name, context.style.font_size);
    const unitless::vec2 text_pos = {
		row_rect.center().x() - text_w / 2.f,
		row_rect.center().y() + context.style.font_size / 2.f
    };

    context.text_renderer.draw_text({
		.font = context.font,
		.text = name,
		.position = text_pos,
		.scale = context.style.font_size,
		.clip_rect = row_rect
    });

    context.layout_cursor.y() -= widget_height + context.style.padding;
}

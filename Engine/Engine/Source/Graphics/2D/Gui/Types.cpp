module gse.graphics;

import std;

import :types;
import :font;
import :texture;
import :ui_renderer;
import :styles;

import gse.math;
import gse.core;
import gse.time;

gse::gui::menu::menu(std::string_view tag, const menu_data& data)
    : identifiable(tag)
    , identifiable_owned(data.parent_id)
    , rect(data.rect)
    , dock_split_ratio(data.dock_split_ratio)
    , docked_to(data.docked_to) {
    tab_contents.emplace_back(tag);
}

auto gse::gui::draw_context::queue_sprite(renderer::sprite_command cmd) const -> void {
    cmd.layer = current_layer;
    cmd.z_order = current_z_order;
    sprites.push_back(std::move(cmd));
}

auto gse::gui::draw_context::queue_text(renderer::text_command cmd) const -> void {
    cmd.layer = current_layer;
    cmd.z_order = current_z_order;
    texts.push_back(std::move(cmd));
}

auto gse::gui::draw_context::input_available() const -> bool {
    return static_cast<std::uint8_t>(current_layer) >= static_cast<std::uint8_t>(input_layer);
}

auto gse::gui::draw_context::set_tooltip(const id& widget_id, const std::string& text) const -> void {
    if (!tooltip || text.empty()) {
        return;
    }

    tooltip->pending_widget_id = widget_id;
    tooltip->text = text;
    tooltip->position = input.mouse_position();
}

auto gse::gui::draw_context::next_row(const float height_multiplier) const -> ui_rect {
    if (!current_menu) {
        return {};
    }

    const float row_height = (font->line_height(style.font_size) + style.padding * style.widget_height_padding) * height_multiplier;
    const ui_rect content_rect = current_menu->rect.inset({ style.padding, style.padding });

    const ui_rect row = ui_rect::from_position_size(
        { content_rect.left(), layout_cursor.y() },
        { content_rect.width(), row_height }
    );

    layout_cursor.y() -= row_height + style.padding + style.item_spacing;
    return row;
}

auto gse::gui::draw_context::animated_color(const id& widget_id, const vec4f target, const float speed) const -> vec4f {
    const auto key = widget_id.number();
    auto it = widget_anim_colors.find(key);
    if (it == widget_anim_colors.end()) {
        widget_anim_colors[key] = target;
        return target;
    }

    const float dt = system_clock::dt<time>().as<seconds>();
    const float t = std::clamp(speed * dt, 0.f, 1.f);
    it->second = it->second + (target - it->second) * t;
    return it->second;
}

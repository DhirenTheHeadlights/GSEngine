export module gse.graphics:menu_bar;

import std;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.math;

import :types;
import :font;
import :texture;
import :ui_renderer;

export namespace gse::menu_bar {
    struct context {
        resource::handle<font> font;
        resource::handle<texture> blank_texture;
        const gui::style& style;
        std::vector<renderer::sprite_command>& sprites;
        std::vector<renderer::text_command>& texts;
    };

    struct state {
        bool settings_hovered = false;
        bool settings_open = false;
    };

    auto update(
        state& state,
        const context& ctx,
        const input::state& input,
        vec2f viewport_size
    ) -> void;

    auto height(
        const gui::style& style
    ) -> float;

    auto bar_rect(
        const gui::style& style,
        vec2f viewport_size
    ) -> gui::ui_rect;
}

namespace gse::menu_bar {
    auto draw_gear_icon(
        std::vector<renderer::sprite_command>& commands,
        vec2f center,
        float size,
        vec4f color,
        resource::handle<texture> texture
    ) -> void;
}

auto gse::menu_bar::height(const gui::style& style) -> float {
    return style.menu_bar_height;
}

auto gse::menu_bar::bar_rect(const gui::style& style, const vec2f viewport_size) -> gui::ui_rect {
    return gui::ui_rect::from_position_size(
        { 0.f, viewport_size.y() },
        { viewport_size.x(), height(style) }
    );
}

auto gse::menu_bar::update(state& state, const context& ctx, const input::state& input, const vec2f viewport_size) -> void {
    const auto& sty = ctx.style;
    constexpr float button_size = 24.f;
    
    const gui::ui_rect bar = bar_rect(sty, viewport_size);
    
    ctx.sprites.push_back({
        .rect = bar,
        .color = sty.color_menu_bar,
        .texture = ctx.blank_texture
    });
    
    if (ctx.font.valid()) {
        ctx.texts.push_back({
            .font = ctx.font,
            .text = "GSEngine",
            .position = {
                bar.left() + sty.padding,
                bar.center().y() + sty.font_size * 0.35f
            },
            .scale = sty.font_size,
            .color = sty.color_text,
            .clip_rect = bar
        });
    }
    
    const vec2f settings_center = {
        bar.right() - sty.padding - button_size * 0.5f,
        bar.center().y()
    };
    
    const gui::ui_rect settings_hit_rect = gui::ui_rect::from_position_size(
        { settings_center.x() - button_size * 0.5f, settings_center.y() + button_size * 0.5f },
        { button_size, button_size }
    );
    
    const vec2f mouse_pos = input.mouse_position();
    state.settings_hovered = settings_hit_rect.contains(mouse_pos);
    
    if (state.settings_hovered && input.mouse_button_pressed(mouse_button::button_1)) {
        state.settings_open = !state.settings_open;
    }
    
    const vec4f gear_color = state.settings_hovered ? sty.color_icon_hovered : sty.color_icon;
    draw_gear_icon(ctx.sprites, settings_center, button_size * 0.4f, gear_color, ctx.blank_texture);
}

auto gse::menu_bar::draw_gear_icon(std::vector<renderer::sprite_command>& commands, const vec2f center, const float size, const vec4f color, const resource::handle<texture> texture) -> void {
    constexpr float thickness = 2.f;
    constexpr int tooth_count = 6;
    
    const float inner_radius = size * 0.5f;
    const float outer_radius = size * 0.85f;
    
    for (int i = 0; i < tooth_count; ++i) {
        const float angle_rad = static_cast<float>(i) * (2.f * 3.14159f / static_cast<float>(tooth_count));
        const vec2f dir = { std::cos(angle_rad), std::sin(angle_rad) };
        
        const vec2f inner_point = center + dir * inner_radius;
        const vec2f outer_point = center + dir * outer_radius;
        
        const vec2f mid = (inner_point + outer_point) * 0.5f;
        const float len = outer_radius - inner_radius;
        
        commands.push_back({
            .rect = gui::ui_rect::from_position_size(
                { mid.x() - thickness * 0.5f, mid.y() + len * 0.5f },
                { thickness, len }
            ),
            .color = color,
            .texture = texture,
            .rotation = angle{ angle_rad }
        });
    }
    
    constexpr float center_size = 3.f;
    commands.push_back({
        .rect = gui::ui_rect::from_position_size(
            { center.x() - center_size, center.y() + thickness * 0.5f },
            { center_size * 2.f, thickness }
        ),
        .color = color,
        .texture = texture
    });

    commands.push_back({
        .rect = gui::ui_rect::from_position_size(
            { center.x() - thickness * 0.5f, center.y() + center_size },
            { thickness, center_size * 2.f }
        ),
        .color = color,
        .texture = texture
    });
}
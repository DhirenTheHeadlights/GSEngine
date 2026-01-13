export module gse.graphics:menu_bar;

import std;

import gse.platform;
import gse.physics.math;

import :types;
import :font;
import :texture;
import :sprite_renderer;
import :text_renderer;

export namespace gse::menu_bar {
    struct context {
        resource::handle<font> font;
        resource::handle<texture> blank_texture;
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
        unitless::vec2 viewport_size
    ) -> void;

    auto height(
    ) -> float;

    auto bar_rect(
        unitless::vec2 viewport_size
    ) -> gui::ui_rect;
}

namespace gse::menu_bar {
    auto draw_gear_icon(
        std::vector<renderer::sprite_command>& commands,
        unitless::vec2 center,
        float size,
        unitless::vec4 color,
        resource::handle<texture> texture
    ) -> void;
}

auto gse::menu_bar::height() -> float {
    return 32.f;
}

auto gse::menu_bar::bar_rect(const unitless::vec2 viewport_size) -> gui::ui_rect {
    return gui::ui_rect::from_position_size(
        { 0.f, viewport_size.y() },
        { viewport_size.x(), height() }
    );
}

auto gse::menu_bar::update(state& state, const context& ctx, const input::state& input, const unitless::vec2 viewport_size) -> void {
    constexpr float bar_height = 32.f;
    constexpr float padding = 10.f;
    constexpr float button_size = 24.f;
    
    const unitless::vec4 bar_color = { 0.08f, 0.08f, 0.08f, 1.0f };
    const unitless::vec4 text_color = { 0.9f, 0.9f, 0.9f, 1.0f };
    const unitless::vec4 icon_color = { 0.7f, 0.7f, 0.7f, 1.0f };
    const unitless::vec4 icon_hover_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    
    const gui::ui_rect bar_rect = gui::ui_rect::from_position_size(
        { 0.f, viewport_size.y() },
        { viewport_size.x(), bar_height }
    );
    
    ctx.sprites.push_back({
        .rect = bar_rect,
        .color = bar_color,
        .texture = ctx.blank_texture
    });
    
    if (ctx.font.valid()) {
	    constexpr float font_size = 16.f;
	    ctx.texts.push_back({
            .font = ctx.font,
            .text = "GSEngine",
            .position = {
                bar_rect.left() + padding,
                bar_rect.center().y() + font_size * 0.35f
            },
            .scale = font_size,
            .clip_rect = bar_rect
        });
    }
    
    const unitless::vec2 settings_center = {
        bar_rect.right() - padding - button_size * 0.5f,
        bar_rect.center().y()
    };
    
    const gui::ui_rect settings_hit_rect = gui::ui_rect::from_position_size(
        { settings_center.x() - button_size * 0.5f, settings_center.y() + button_size * 0.5f },
        { button_size, button_size }
    );
    
    const unitless::vec2 mouse_pos = input.mouse_position();
    state.settings_hovered = settings_hit_rect.contains(mouse_pos);
    
    if (state.settings_hovered && input.mouse_button_pressed(mouse_button::button_1)) {
        state.settings_open = !state.settings_open;
    }
    
    const unitless::vec4 gear_color = state.settings_hovered ? icon_hover_color : icon_color;
    draw_gear_icon(ctx.sprites, settings_center, button_size * 0.4f, gear_color, ctx.blank_texture);
}

auto gse::menu_bar::draw_gear_icon(std::vector<renderer::sprite_command>& commands, const unitless::vec2 center, const float size, const unitless::vec4 color, const resource::handle<texture> texture) -> void {
    constexpr float thickness = 2.f;
    constexpr int tooth_count = 6;
    
    const float inner_radius = size * 0.5f;
    const float outer_radius = size * 0.85f;
    
    for (int i = 0; i < tooth_count; ++i) {
        const float angle_rad = static_cast<float>(i) * (2.f * 3.14159f / static_cast<float>(tooth_count));
        const unitless::vec2 dir = { std::cos(angle_rad), std::sin(angle_rad) };
        
        const unitless::vec2 inner_point = center + dir * inner_radius;
        const unitless::vec2 outer_point = center + dir * outer_radius;
        
        const unitless::vec2 mid = (inner_point + outer_point) * 0.5f;
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
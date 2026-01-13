export module gse.graphics:settings_panel;

import std;

import gse.platform;
import gse.utility;
import gse.physics.math;

import :types;
import :font;
import :texture;
import :sprite_renderer;
import :text_renderer;
import :menu_bar;
import :ids;

export namespace gse::settings_panel {
    struct settings {
        bool vsync = true;
        bool fullscreen = false;
        bool show_fps = false;
        bool show_profiler = false;
        float render_scale = 1.0f;
        int shadow_quality = 2;
    };

    struct state {
        settings current;
        id hot_id;
        id active_id;
    };

    struct context {
        resource::handle<font> font;
        resource::handle<texture> blank_texture;
        std::vector<renderer::sprite_command>& sprites;
        std::vector<renderer::text_command>& texts;
        const input::state& input;
        render_layer layer = render_layer::popup;
    };

    auto update(
        state& state,
        const context& ctx,
        const gui::ui_rect& rect,
        bool is_open
    ) -> void;

    auto default_panel_rect(
        unitless::vec2 viewport_size,
        float menu_bar_height
    ) -> gui::ui_rect;
}

namespace gse::settings_panel {
    struct layout_state {
        unitless::vec2 cursor;
        gui::ui_rect content_rect;
        float row_height;
        const context* ctx;
        state* panel_state;
    };

    auto draw_section_header(
        layout_state& layout,
        const std::string& text
    ) -> void;

    auto draw_toggle(
        layout_state& layout,
        const std::string& label,
        bool& value
    ) -> void;

    auto draw_slider(
        layout_state& layout,
        const std::string& label,
        float& value,
        float min,
        float max
    ) -> void;

    auto draw_choice(
        layout_state& layout,
        const std::string& label,
        int& value,
        std::span<const std::string_view> options
    ) -> void;
}

auto gse::settings_panel::default_panel_rect(const unitless::vec2 viewport_size, const float menu_bar_height) -> gui::ui_rect {
    constexpr float panel_width = 300.f;
    constexpr float panel_height = 360.f;
    constexpr float padding = 10.f;

    const float panel_right = viewport_size.x() - padding;
    const float panel_top = viewport_size.y() - menu_bar_height - padding;

    return gui::ui_rect::from_position_size(
        { panel_right - panel_width, panel_top },
        { panel_width, panel_height }
    );
}

auto gse::settings_panel::update(state& state, const context& ctx, const gui::ui_rect& rect, const bool is_open) -> void {
    if (!is_open) {
        return;
    }

    gui::ids::scope settings_scope("settings_panel");
    const gui::style sty{};

    auto body_color = sty.color_menu_body;
    body_color.a() = 1;

    constexpr float border_thickness = 1.f;
    const unitless::vec4 border_color = { 0.1f, 0.1f, 0.1f, 1.0f };

    // Border - top
    ctx.sprites.push_back({
        .rect = gui::ui_rect::from_position_size(
            { rect.left(), rect.top() },
            { rect.width(), border_thickness }
        ),
        .color = border_color,
        .texture = ctx.blank_texture,
        .layer = ctx.layer
    });

    // Border - bottom
    ctx.sprites.push_back({
        .rect = gui::ui_rect::from_position_size(
            { rect.left(), rect.bottom() + border_thickness },
            { rect.width(), border_thickness }
        ),
        .color = border_color,
        .texture = ctx.blank_texture,
        .layer = ctx.layer
    });

    // Border - left
    ctx.sprites.push_back({
        .rect = gui::ui_rect::from_position_size(
            { rect.left() - border_thickness, rect.top() },
            { border_thickness, rect.height() }
        ),
        .color = border_color,
        .texture = ctx.blank_texture,
        .layer = ctx.layer
    });

    // Border - right
    ctx.sprites.push_back({
        .rect = gui::ui_rect::from_position_size(
            { rect.right(), rect.top() },
            { border_thickness, rect.height() }
        ),
        .color = border_color,
        .texture = ctx.blank_texture,
        .layer = ctx.layer
    });
    
    // Panel background
    ctx.sprites.push_back({
        .rect = rect,
        .color = body_color,
        .texture = ctx.blank_texture,
        .layer = ctx.layer
    });
    
    // Title bar
    const gui::ui_rect title_rect = gui::ui_rect::from_position_size(
        rect.top_left(),
        { rect.width(), sty.title_bar_height }
    );
    
    ctx.sprites.push_back({
        .rect = title_rect,
        .color = sty.color_title_bar,
        .texture = ctx.blank_texture,
        .layer = ctx.layer
    });
    
    if (ctx.font.valid()) {
        ctx.texts.push_back({
            .font = ctx.font,
            .text = "Settings",
            .position = {
                title_rect.left() + sty.padding,
                title_rect.center().y() + sty.font_size * 0.35f
            },
            .scale = sty.font_size,
            .clip_rect = title_rect,
            .layer = ctx.layer
        });
    }
    
    const gui::ui_rect content_rect = gui::ui_rect::from_position_size(
        { rect.left() + sty.padding, rect.top() - sty.title_bar_height - sty.padding },
        { rect.width() - sty.padding * 2.f, rect.height() - sty.title_bar_height - sty.padding * 2.f }
    );
    
    const float row_height = ctx.font.valid() ? ctx.font->line_height(sty.font_size) + sty.padding : 24.f;
    
    layout_state layout{
        .cursor = content_rect.top_left(),
        .content_rect = content_rect,
        .row_height = row_height,
        .ctx = &ctx,
        .panel_state = &state
    };
    
    draw_section_header(layout, "Display");
    draw_toggle(layout, "VSync", state.current.vsync);
    draw_toggle(layout, "Fullscreen", state.current.fullscreen);
    draw_slider(layout, "Render Scale", state.current.render_scale, 0.5f, 2.0f);
    
    layout.cursor.y() -= sty.padding;
    
    draw_section_header(layout, "Graphics");
    static constexpr std::array shadow_options = {
        std::string_view("Off"),
        std::string_view("Low"),
        std::string_view("Medium"),
        std::string_view("High")
    };
    draw_choice(layout, "Shadows", state.current.shadow_quality, shadow_options);
    
    layout.cursor.y() -= sty.padding;
    
    draw_section_header(layout, "Debug");
    draw_toggle(layout, "Show FPS", state.current.show_fps);
    draw_toggle(layout, "Show Profiler", state.current.show_profiler);
    
    if (ctx.input.mouse_button_released(mouse_button::button_1)) {
        state.active_id.reset();
    }
}

auto gse::settings_panel::draw_section_header(layout_state& layout, const std::string& text) -> void {
    const gui::style sty{};

    const gui::ui_rect row_rect = gui::ui_rect::from_position_size(
        layout.cursor,
        { layout.content_rect.width(), layout.row_height }
    );

    if (layout.ctx->font.valid()) {
        layout.ctx->texts.push_back({
            .font = layout.ctx->font,
            .text = text,
            .position = {
                row_rect.left(),
                row_rect.center().y() + sty.font_size * 0.35f
            },
            .scale = sty.font_size,
            .color = { 0.5f, 0.5f, 0.5f, 1.0f },
            .clip_rect = layout.content_rect,
            .layer = layout.ctx->layer
        });
    }

    layout.cursor.y() -= layout.row_height;
}

auto gse::settings_panel::draw_toggle(layout_state& layout, const std::string& label, bool& value) -> void {
    const gui::style sty{};
    const auto& ctx = *layout.ctx;
    const auto& [font, blank_texture, sprites, texts, input, layer] = ctx;
    
    constexpr float toggle_width = 40.f;
    constexpr float toggle_height = 20.f;
    constexpr float knob_padding = 2.f;
    
    const gui::ui_rect row_rect = gui::ui_rect::from_position_size(
        layout.cursor,
        { layout.content_rect.width(), layout.row_height }
    );
    
    const gui::ui_rect toggle_rect = gui::ui_rect::from_position_size(
        { row_rect.right() - toggle_width, row_rect.center().y() + toggle_height * 0.5f },
        { toggle_width, toggle_height }
    );
    
    const unitless::vec2 mouse_pos = input.mouse_position();
    const bool hovered = toggle_rect.contains(mouse_pos);
    const id toggle_id = gui::ids::make(label);
    
    if (hovered && input.mouse_button_pressed(mouse_button::button_1)) {
        layout.panel_state->active_id = toggle_id;
    }
    
    if (hovered && layout.panel_state->active_id == toggle_id && input.mouse_button_released(mouse_button::button_1)) {
        value = !value;
    }
    
    const unitless::vec4 track_color = value 
        ? unitless::vec4{ 0.2f, 0.6f, 0.3f, 1.0f }
        : unitless::vec4{ 0.3f, 0.3f, 0.3f, 1.0f };
    
    sprites.push_back({
        .rect = toggle_rect,
        .color = track_color,
        .texture = blank_texture,
        .layer = layer
    });
    
    constexpr float knob_size = toggle_height - knob_padding * 2.f;
    const float knob_x = value 
        ? toggle_rect.right() - knob_size - knob_padding
        : toggle_rect.left() + knob_padding;
    
    const gui::ui_rect knob_rect = gui::ui_rect::from_position_size(
        { knob_x, toggle_rect.top() - knob_padding },
        { knob_size, knob_size }
    );
    
    unitless::vec4 knob_color = { 0.9f, 0.9f, 0.9f, 1.0f };
    if (hovered) {
        knob_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    }
    
    sprites.push_back({
        .rect = knob_rect,
        .color = knob_color,
        .texture = blank_texture,
        .layer = layer
    });
    
    if (font.valid()) {
        texts.push_back({
            .font = font,
            .text = label,
            .position = { 
                layout.cursor.x() + sty.padding, 
                row_rect.center().y() + sty.font_size * 0.35f 
            },
            .scale = sty.font_size,
            .clip_rect = layout.content_rect,
            .layer = layer
        });
    }
    
    layout.cursor.y() -= layout.row_height;
}

auto gse::settings_panel::draw_slider(layout_state& layout, const std::string& label, float& value, const float min, const float max) -> void {
    const gui::style sty{};
    const auto& ctx = *layout.ctx;
    const auto& [font, blank_texture, sprites, texts, input, layer] = ctx;
    
    constexpr float slider_width = 100.f;
    constexpr float slider_height = 6.f;
    constexpr float knob_size = 14.f;
    
    const gui::ui_rect row_rect = gui::ui_rect::from_position_size(
        layout.cursor,
        { layout.content_rect.width(), layout.row_height }
    );
    
    const gui::ui_rect track_rect = gui::ui_rect::from_position_size(
        { row_rect.right() - slider_width, row_rect.center().y() + slider_height * 0.5f },
        { slider_width, slider_height }
    );
    
    const unitless::vec2 mouse_pos = input.mouse_position();
    const id slider_id = gui::ids::make(label + "_slider");
    
    const gui::ui_rect hit_rect = gui::ui_rect::from_position_size(
        { track_rect.left() - knob_size * 0.5f, track_rect.center().y() + knob_size * 0.5f },
        { track_rect.width() + knob_size, knob_size }
    );
    
    const bool hovered = hit_rect.contains(mouse_pos);
    
    if (hovered && input.mouse_button_pressed(mouse_button::button_1)) {
        layout.panel_state->active_id = slider_id;
    }
    
    if (layout.panel_state->active_id == slider_id) {
        const float t = std::clamp((mouse_pos.x() - track_rect.left()) / track_rect.width(), 0.f, 1.f);
        value = min + t * (max - min);
    }
    
    sprites.push_back({
        .rect = track_rect,
        .color = { 0.3f, 0.3f, 0.3f, 1.0f },
        .texture = blank_texture,
        .layer = layer
    });
    
    const float t = (value - min) / (max - min);
    const gui::ui_rect fill_rect = gui::ui_rect::from_position_size(
        track_rect.top_left(),
        { track_rect.width() * t, slider_height }
    );
    
    sprites.push_back({
        .rect = fill_rect,
        .color = { 0.3f, 0.5f, 0.8f, 1.0f },
        .texture = blank_texture,
        .layer = layer
    });
    
    const float knob_x = track_rect.left() + track_rect.width() * t - knob_size * 0.5f;
    const gui::ui_rect knob_rect = gui::ui_rect::from_position_size(
        { knob_x, track_rect.center().y() + knob_size * 0.5f },
        { knob_size, knob_size }
    );
    
    unitless::vec4 knob_color = { 0.9f, 0.9f, 0.9f, 1.0f };
    if (hovered || layout.panel_state->active_id == slider_id) {
        knob_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    }
    
    sprites.push_back({
        .rect = knob_rect,
        .color = knob_color,
        .texture = blank_texture,
        .layer = layer
    });
    
    if (font.valid()) {
        texts.push_back({
            .font = font,
            .text = label,
            .position = { 
                layout.cursor.x() + sty.padding, 
                row_rect.center().y() + sty.font_size * 0.35f 
            },
            .scale = sty.font_size,
            .clip_rect = layout.content_rect,
            .layer = layer
        });
        
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.2f", value);
        const float value_width = font->width(buf, sty.font_size);
        
        texts.push_back({
            .font = font,
            .text = buf,
            .position = { 
                track_rect.left() - value_width - sty.padding, 
                row_rect.center().y() + sty.font_size * 0.35f 
            },
            .scale = sty.font_size,
            .color = { 0.7f, 0.7f, 0.7f, 1.0f },
            .clip_rect = layout.content_rect,
            .layer = layer
        });
    }
    
    layout.cursor.y() -= layout.row_height;
}

auto gse::settings_panel::draw_choice(layout_state& layout, const std::string& label, int& value, const std::span<const std::string_view> options) -> void {
    const gui::style sty{};
    const auto& ctx = *layout.ctx;
    const auto& [font, blank_texture, sprites, texts, input, layer] = ctx;
    
    const gui::ui_rect row_rect = gui::ui_rect::from_position_size(
        layout.cursor,
        { layout.content_rect.width(), layout.row_height }
    );
    
    float max_option_width = 60.f;
    if (font.valid()) {
        for (const auto& opt : options) {
            max_option_width = std::max(max_option_width, font->width(std::string(opt), sty.font_size) + sty.padding * 2.f);
        }
    }
    
    const gui::ui_rect choice_rect = gui::ui_rect::from_position_size(
        { row_rect.right() - max_option_width, row_rect.center().y() + layout.row_height * 0.4f },
        { max_option_width, layout.row_height * 0.8f }
    );
    
    const unitless::vec2 mouse_pos = input.mouse_position();
    const bool hovered = choice_rect.contains(mouse_pos);
    const id choice_id = gui::ids::make(label + "_choice");
    
    if (hovered && input.mouse_button_pressed(mouse_button::button_1)) {
        layout.panel_state->active_id = choice_id;
    }
    
    if (hovered && layout.panel_state->active_id == choice_id && input.mouse_button_released(mouse_button::button_1)) {
        value = (value + 1) % static_cast<int>(options.size());
    }
    
    unitless::vec4 bg_color = { 0.25f, 0.25f, 0.25f, 1.0f };
    if (hovered) {
        bg_color = { 0.35f, 0.35f, 0.35f, 1.0f };
    }
    
    sprites.push_back({
        .rect = choice_rect,
        .color = bg_color,
        .texture = blank_texture,
        .layer = layer
    });
    
    if (font.valid() && value >= 0 && value < static_cast<int>(options.size())) {
        const std::string opt_text(options[value]);
        const float text_width = font->width(opt_text, sty.font_size);
        
        texts.push_back({
            .font = font,
            .text = opt_text,
            .position = { 
                choice_rect.center().x() - text_width * 0.5f, 
                choice_rect.center().y() + sty.font_size * 0.35f 
            },
            .scale = sty.font_size,
            .clip_rect = choice_rect,
            .layer = layer
        });
    }
    
    if (font.valid()) {
        texts.push_back({
            .font = font,
            .text = label,
            .position = { 
                layout.cursor.x() + sty.padding, 
                row_rect.center().y() + sty.font_size * 0.35f 
            },
            .scale = sty.font_size,
            .clip_rect = layout.content_rect,
            .layer = layer
        });
    }
    
    layout.cursor.y() -= layout.row_height;
}
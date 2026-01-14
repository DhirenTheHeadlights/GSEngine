export module gse.graphics:settings_panel;

import std;

import gse.platform;
import gse.utility;
import gse.physics.math;

import :types;
import :font;
import :texture;
import :ui_renderer;
import :menu_bar;
import :ids;
import :styles;

export namespace gse::settings_panel {
    struct settings {
        bool vsync = true;
        bool fullscreen = false;
        bool show_fps = false;
        bool show_profiler = false;
        float render_scale = 1.0f;
        int shadow_quality = 2;
        gui::theme ui_theme = gui::theme::dark;
    };

    struct state {
        settings current;
        id hot_id;
        id active_id;
    };

    struct context {
        resource::handle<font> font;
        resource::handle<texture> blank_texture;
        const gui::style& style;
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
        const gui::style& style,
        unitless::vec2 viewport_size
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

auto gse::settings_panel::default_panel_rect(const gui::style& style, const unitless::vec2 viewport_size) -> gui::ui_rect {
    constexpr float panel_width = 300.f;
    constexpr float panel_height = 360.f;

    const float panel_right = viewport_size.x() - style.padding;
    const float panel_top = viewport_size.y() - style.menu_bar_height - style.padding;

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
    const auto& sty = ctx.style;

    auto body_color = sty.color_menu_body;
    body_color.a() = 1.f;

    constexpr float border_thickness = 1.f;

    ctx.sprites.push_back({
        .rect = gui::ui_rect::from_position_size(
            { rect.left(), rect.top() },
            { rect.width(), border_thickness }
        ),
        .color = sty.color_border,
        .texture = ctx.blank_texture,
        .layer = ctx.layer
    });

    ctx.sprites.push_back({
        .rect = gui::ui_rect::from_position_size(
            { rect.left(), rect.bottom() + border_thickness },
            { rect.width(), border_thickness }
        ),
        .color = sty.color_border,
        .texture = ctx.blank_texture,
        .layer = ctx.layer
    });

    ctx.sprites.push_back({
        .rect = gui::ui_rect::from_position_size(
            { rect.left() - border_thickness, rect.top() },
            { border_thickness, rect.height() }
        ),
        .color = sty.color_border,
        .texture = ctx.blank_texture,
        .layer = ctx.layer
    });

    ctx.sprites.push_back({
        .rect = gui::ui_rect::from_position_size(
            { rect.right(), rect.top() },
            { border_thickness, rect.height() }
        ),
        .color = sty.color_border,
        .texture = ctx.blank_texture,
        .layer = ctx.layer
    });
    
    ctx.sprites.push_back({
        .rect = rect,
        .color = body_color,
        .texture = ctx.blank_texture,
        .layer = ctx.layer
    });
    
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
            .color = sty.color_text,
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

    static constexpr std::array theme_options = {
        std::string_view("Dark"),
        std::string_view("Darker"),
        std::string_view("Light"),
        std::string_view("High Contrast")
    };
    int theme_index = static_cast<int>(state.current.ui_theme);
    draw_choice(layout, "Theme", theme_index, theme_options);
    state.current.ui_theme = static_cast<gui::theme>(theme_index);

    layout.cursor.y() -= sty.padding;
    
    if (ctx.input.mouse_button_released(mouse_button::button_1)) {
        state.active_id.reset();
    }
}

auto gse::settings_panel::draw_section_header(layout_state& layout, const std::string& text) -> void {
    const auto& sty = layout.ctx->style;

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
            .color = sty.color_text_secondary,
            .clip_rect = layout.content_rect,
            .layer = layout.ctx->layer
        });
    }

    layout.cursor.y() -= layout.row_height;
}

auto gse::settings_panel::draw_toggle(layout_state& layout, const std::string& label, bool& value) -> void {
    const auto& sty = layout.ctx->style;
    const auto& ctx = *layout.ctx;
    const auto& [font, blank_texture, style, sprites, texts, input, layer] = ctx;
    
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
    
    const unitless::vec4 track_color = value ? sty.color_toggle_on : sty.color_toggle_off;
    
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
    
    const unitless::vec4 knob_color = hovered ? sty.color_handle_hovered : sty.color_handle;
    
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
            .color = sty.color_text,
            .clip_rect = layout.content_rect,
            .layer = layer
        });
    }
    
    layout.cursor.y() -= layout.row_height;
}

auto gse::settings_panel::draw_slider(layout_state& layout, const std::string& label, float& value, const float min, const float max) -> void {
    const auto& sty = layout.ctx->style;
    const auto& ctx = *layout.ctx;
    const auto& [font, blank_texture, style, sprites, texts, input, layer] = ctx;
    
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
        .color = sty.color_widget_background,
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
        .color = sty.color_slider_fill,
        .texture = blank_texture,
        .layer = layer
    });
    
    const float knob_x = track_rect.left() + track_rect.width() * t - knob_size * 0.5f;
    const gui::ui_rect knob_rect = gui::ui_rect::from_position_size(
        { knob_x, track_rect.center().y() + knob_size * 0.5f },
        { knob_size, knob_size }
    );
    
    const bool is_active = layout.panel_state->active_id == slider_id;
    const unitless::vec4 knob_color = (hovered || is_active) ? sty.color_handle_hovered : sty.color_handle;
    
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
            .color = sty.color_text,
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
            .color = sty.color_text_secondary,
            .clip_rect = layout.content_rect,
            .layer = layer
        });
    }
    
    layout.cursor.y() -= layout.row_height;
}

auto gse::settings_panel::draw_choice(layout_state& layout, const std::string& label, int& value, const std::span<const std::string_view> options) -> void {
    const auto& sty = layout.ctx->style;
    const auto& ctx = *layout.ctx;
    const auto& [font, blank_texture, style, sprites, texts, input, layer] = ctx;
    
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
    
    const unitless::vec4 bg_color = hovered ? sty.color_widget_hovered : sty.color_widget_background;
    
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
            .color = sty.color_text,
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
            .color = sty.color_text,
            .clip_rect = layout.content_rect,
            .layer = layer
        });
    }
    
    layout.cursor.y() -= layout.row_height;
}
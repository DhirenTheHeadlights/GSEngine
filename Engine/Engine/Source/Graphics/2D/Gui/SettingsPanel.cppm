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
import :dropdown_widget;
import :scroll_widget;
import :input_layers;

export namespace gse::settings_panel {
    using pending_value = std::variant<bool, float, std::size_t>;

    struct pending_change {
        std::string category;
        std::string name;
        std::function<void(save::property_base&)> apply;
        pending_value value;
        bool requires_restart = false;
    };

    enum class popup_type {
        none,
        restart_required
    };

    struct state {
        id hot_id;
        id active_id;
        gui::dropdown_state dropdown;
        gui::scroll_state scroll;
        std::vector<pending_change> pending_changes;
        popup_type active_popup = popup_type::none;
        bool has_pending_restart = false;

        [[nodiscard]] auto find_pending(const std::string_view category, const std::string_view name) const -> const pending_change* {
            for (const auto& change : pending_changes) {
                if (change.category == category && change.name == name) {
                    return &change;
                }
            }
            return nullptr;
        }
    };

    struct context {
        resource::handle<font> font;
        resource::handle<texture> blank_texture;
        const gui::style& style;
        std::vector<renderer::sprite_command>& sprites;
        std::vector<renderer::text_command>& texts;
        const input::state& input;
        render_layer layer = render_layer::popup;
        std::function<void(save::update_request)> publish_update;
        std::function<void()> request_save;
        gui::tooltip_state* tooltip = nullptr;
        gui::input_layer* input_layers = nullptr;
    };

    auto update(
        state& state,
        const context& ctx,
        const gui::ui_rect& rect,
        bool is_open,
        const save::system& save_sys
    ) -> bool;

    auto default_panel_rect(
        const gui::style& style,
        unitless::vec2 viewport_size
    ) -> gui::ui_rect;
}

namespace gse::settings_panel {
    struct layout_state {
        unitless::vec2 cursor;
        gui::ui_rect content_rect;
        gui::ui_rect clip_rect;
        float row_height;
        const context* ctx;
        state* panel_state;

        [[nodiscard]] auto is_row_visible() const -> bool {
            const float row_top = cursor.y();
            const float row_bottom = cursor.y() - row_height;
            return row_top >= clip_rect.bottom() && row_bottom <= clip_rect.top();
        }
    };

    auto set_tooltip(
        const context& ctx,
        const id& widget_id,
        const std::string& text
    ) -> void;

    auto draw_section_header(
        layout_state& layout,
        const std::string& text
    ) -> void;

    auto draw_property(
        layout_state& layout, 
        const save::property_base& prop
    ) -> void;

    auto draw_toggle(
        layout_state& layout,
        const save::property_base& prop
    ) -> void;

    auto draw_slider(
        layout_state& layout,
        const save::property_base& prop
    ) -> void;

    auto draw_choice(
        layout_state& layout,
        const save::property_base& prop
    ) -> void;

    auto draw_apply_button(
        layout_state& layout,
        state& panel_state
    ) -> bool;

    auto draw_restart_popup(
        state& panel_state,
        const context& ctx,
        const gui::ui_rect& panel_rect
    ) -> void;

    auto queue_change(
        state& panel_state,
        const save::property_base& prop,
        pending_value value,
        std::function<void(save::property_base&)> apply
    ) -> void;

    auto dropdown_blocking_input(
        const layout_state& layout
    ) -> bool;
}

auto gse::settings_panel::default_panel_rect(const gui::style& style, const unitless::vec2 viewport_size) -> gui::ui_rect {
    const float usable_height = viewport_size.y() - style.menu_bar_height;

    constexpr float screen_fill_ratio = 0.8f;
    const float panel_width = viewport_size.x() * screen_fill_ratio;
    const float panel_height = usable_height * screen_fill_ratio;

    const float center_x = viewport_size.x() * 0.5f;
    const float center_y = usable_height * 0.5f;

    return gui::ui_rect::from_position_size(
        { center_x - panel_width * 0.5f, center_y + panel_height * 0.5f },
        { panel_width, panel_height }
    );
}

auto gse::settings_panel::set_tooltip(const context& ctx, const id& widget_id, const std::string& text) -> void {
    if (!ctx.tooltip || text.empty()) {
        return;
    }

    ctx.tooltip->pending_widget_id = widget_id;
    ctx.tooltip->text = text;
    ctx.tooltip->position = ctx.input.mouse_position();
}

auto gse::settings_panel::dropdown_blocking_input(const layout_state& layout) -> bool {
    if (!layout.ctx->input_layers) {
        return false;
    }
    return !layout.ctx->input_layers->input_available_at(
        layout.ctx->layer,
        layout.ctx->input.mouse_position()
    );
}

auto gse::settings_panel::update(state& state, const context& ctx, const gui::ui_rect& rect, const bool is_open, const save::system& save_sys) -> bool {
    if (!is_open) {
        return false;
    }

    gui::ids::scope settings_scope("settings_panel");
    const auto& sty = ctx.style;
    bool should_close = false;

    auto body_color = sty.color_menu_body;
    body_color.w() = 1.f;

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

    const float close_button_size = sty.title_bar_height * 0.6f;
    const gui::ui_rect close_button_rect = gui::ui_rect::from_position_size(
        { title_rect.right() - close_button_size - sty.padding, title_rect.center().y() + close_button_size * 0.5f },
        { close_button_size, close_button_size }
    );

    const unitless::vec2 mouse_pos = ctx.input.mouse_position();
    const bool close_hovered = close_button_rect.contains(mouse_pos);

    const unitless::vec4 close_bg_color = close_hovered
        ? unitless::vec4(0.8f, 0.2f, 0.2f, 1.0f)
        : sty.color_widget_background;

    ctx.sprites.push_back({
        .rect = close_button_rect,
        .color = close_bg_color,
        .texture = ctx.blank_texture,
        .layer = ctx.layer
    });

    if (ctx.font.valid()) {
        ctx.texts.push_back({
            .font = ctx.font,
            .text = "X",
            .position = {
                close_button_rect.center().x() - ctx.font->width("X", sty.font_size) * 0.5f,
                close_button_rect.center().y() + sty.font_size * 0.35f
            },
            .scale = sty.font_size,
            .color = sty.color_text,
            .clip_rect = close_button_rect,
            .layer = ctx.layer
        });
    }

    if (close_hovered && ctx.input.mouse_button_pressed(mouse_button::button_1)) {
        should_close = true;
    }

    const float row_height = ctx.font.valid()
        ? ctx.font->line_height(sty.font_size) + sty.padding
        : 24.f;

    const float button_area_height = row_height + sty.padding * 3.f;

    const gui::ui_rect scroll_rect = gui::ui_rect::from_position_size(
        { rect.left() + sty.padding, rect.top() - sty.title_bar_height - sty.padding },
        { rect.width() - sty.padding * 2.f, rect.height() - sty.title_bar_height - sty.padding * 2.f - button_area_height }
    );

    const gui::scroll_config scroll_config{
        .scrollbar_width = 6.f * (sty.font_size / 14.f),
        .scrollbar_min_height = 20.f,
        .scroll_speed = row_height * 2.f,
        .smooth_factor = 0.2f,
        .auto_hide_scrollbar = true,
        .smooth_scrolling = true
    };

    auto scroll_ctx = gui::scroll::begin(state.scroll, scroll_rect, sty, ctx.input, scroll_config);

    layout_state layout{
        .cursor = { scroll_rect.left(), scroll_rect.top() + state.scroll.offset },
        .content_rect = scroll_rect,
        .clip_rect = scroll_rect,
        .row_height = row_height,
        .ctx = &ctx,
        .panel_state = &state
    };

    for (const auto category : save_sys.categories()) {
        draw_section_header(layout, std::string(category));

        for (const auto* prop : save_sys.properties_in(category)) {
            draw_property(layout, *prop);
        }

        layout.cursor.y() -= sty.padding;
    }

    gui::scroll::end(state.scroll, scroll_ctx, layout.cursor.y(), sty, ctx.input, ctx.blank_texture, ctx.sprites, ctx.layer, scroll_config);

    const gui::ui_rect button_area_rect = gui::ui_rect::from_position_size(
        { rect.left() + sty.padding, scroll_rect.bottom() - sty.padding },
        { rect.width() - sty.padding * 2.f, button_area_height }
    );

    layout.cursor = button_area_rect.top_left();
    layout.content_rect = button_area_rect;
    layout.clip_rect = button_area_rect;

    if (draw_apply_button(layout, state)) {
        bool needs_restart = false;
        for (const auto& change : state.pending_changes) {
            if (change.requires_restart) {
                needs_restart = true;
                break;
            }
        }

        for (const auto& change : state.pending_changes) {
            ctx.publish_update({
                .category = change.category,
                .name = change.name,
                .apply = change.apply
            });
        }

        state.pending_changes.clear();

        if (needs_restart) {
            state.active_popup = popup_type::restart_required;
            state.has_pending_restart = true;
        }
    }

    if (state.active_popup == popup_type::restart_required) {
        draw_restart_popup(state, ctx, rect);
    }

    if (ctx.input.mouse_button_released(mouse_button::button_1)) {
        state.active_id.reset();
    }

    return should_close;
}

auto gse::settings_panel::draw_section_header(layout_state& layout, const std::string& text) -> void {
    const auto& sty = layout.ctx->style;

    layout.cursor.y() -= layout.row_height;

    if (!layout.is_row_visible()) {
        return;
    }

    const gui::ui_rect row_rect = gui::ui_rect::from_position_size(
        { layout.cursor.x(), layout.cursor.y() + layout.row_height },
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
            .clip_rect = layout.clip_rect,
            .layer = layout.ctx->layer
        });
    }
}

auto gse::settings_panel::draw_property(layout_state& layout, const save::property_base& prop) -> void {
    if (const auto type = prop.type(); type == typeid(bool)) {
        draw_toggle(layout, prop);
    }
    else if (prop.constraint_kind() == save::constraint_type::enumeration) {
        draw_choice(layout, prop);
    }
    else if (prop.constraint_kind() == save::constraint_type::range) {
        draw_slider(layout, prop);
    }
    else if (type == typeid(float) || type == typeid(int)) {
        draw_slider(layout, prop);
    }
}

auto gse::settings_panel::draw_toggle(layout_state& layout, const save::property_base& prop) -> void {
    const auto& sty = layout.ctx->style;
    const auto& ctx = *layout.ctx;

    layout.cursor.y() -= layout.row_height;

    if (!layout.is_row_visible()) {
        return;
    }

    constexpr float toggle_width = 40.f;
    constexpr float toggle_height = 20.f;
    constexpr float knob_padding = 2.f;

    const gui::ui_rect row_rect = gui::ui_rect::from_position_size(
        { layout.cursor.x(), layout.cursor.y() + layout.row_height },
        { layout.content_rect.width(), layout.row_height }
    );

    const gui::ui_rect toggle_rect = gui::ui_rect::from_position_size(
        { row_rect.right() - toggle_width, row_rect.center().y() + toggle_height * 0.5f },
        { toggle_width, toggle_height }
    );

    const unitless::vec2 mouse_pos = ctx.input.mouse_position();
    const bool hovered = row_rect.contains(mouse_pos) && layout.clip_rect.contains(mouse_pos);
    const id toggle_id = gui::ids::make(std::string(prop.name()));

    const auto* pending = layout.panel_state->find_pending(prop.category(), prop.name());
    const bool value = pending ? std::get<bool>(pending->value) : prop.as_bool();

    if (hovered) {
        set_tooltip(ctx, toggle_id, std::string(prop.description()));
    }

    const bool toggle_hovered = toggle_rect.contains(mouse_pos) && layout.clip_rect.contains(mouse_pos) && !dropdown_blocking_input(layout);
    if (toggle_hovered && ctx.input.mouse_button_pressed(mouse_button::button_1)) {
        layout.panel_state->active_id = toggle_id;
    }

    if (toggle_hovered && layout.panel_state->active_id == toggle_id &&
        ctx.input.mouse_button_released(mouse_button::button_1)) {
        const bool new_value = !value;
        queue_change(*layout.panel_state, prop, new_value, [new_value](save::property_base& p) {
            p.set_bool(new_value);
        });
    }

    const unitless::vec4 track_color = value ? sty.color_toggle_on : sty.color_toggle_off;

    ctx.sprites.push_back({
        .rect = toggle_rect,
        .color = track_color,
        .texture = ctx.blank_texture,
        .layer = ctx.layer
    });

    constexpr float knob_size = toggle_height - knob_padding * 2.f;
    const float knob_x = value
        ? toggle_rect.right() - knob_size - knob_padding
        : toggle_rect.left() + knob_padding;

    const gui::ui_rect knob_rect = gui::ui_rect::from_position_size(
        { knob_x, toggle_rect.top() - knob_padding },
        { knob_size, knob_size }
    );

    const unitless::vec4 knob_color = toggle_hovered ? sty.color_handle_hovered : sty.color_handle;

    ctx.sprites.push_back({
        .rect = knob_rect,
        .color = knob_color,
        .texture = ctx.blank_texture,
        .layer = ctx.layer
    });

    if (ctx.font.valid()) {
        auto label = std::string(prop.name());
        if (prop.requires_restart()) {
            label += " *";
        }

        ctx.texts.push_back({
            .font = ctx.font,
            .text = label,
            .position = {
                row_rect.left() + sty.padding,
                row_rect.center().y() + sty.font_size * 0.35f
            },
            .scale = sty.font_size,
            .color = prop.requires_restart() ? sty.color_text_secondary : sty.color_text,
            .clip_rect = layout.clip_rect,
            .layer = ctx.layer
        });
    }
}

auto gse::settings_panel::draw_slider(layout_state& layout, const save::property_base& prop) -> void {
    const auto& sty = layout.ctx->style;
    const auto& ctx = *layout.ctx;

    layout.cursor.y() -= layout.row_height;

    if (!layout.is_row_visible()) {
        return;
    }

    constexpr float slider_width = 100.f;
    constexpr float slider_height = 6.f;
    constexpr float knob_size = 14.f;

    const gui::ui_rect row_rect = gui::ui_rect::from_position_size(
        { layout.cursor.x(), layout.cursor.y() + layout.row_height },
        { layout.content_rect.width(), layout.row_height }
    );

    const gui::ui_rect track_rect = gui::ui_rect::from_position_size(
        { row_rect.right() - slider_width, row_rect.center().y() + slider_height * 0.5f },
        { slider_width, slider_height }
    );

    const unitless::vec2 mouse_pos = ctx.input.mouse_position();
    const id slider_id = gui::ids::make(std::string(prop.name()) + "_slider");

    if (row_rect.contains(mouse_pos) && layout.clip_rect.contains(mouse_pos)) {
        set_tooltip(ctx, slider_id, std::string(prop.description()));
    }

    const gui::ui_rect hit_rect = gui::ui_rect::from_position_size(
        { track_rect.left() - knob_size * 0.5f, track_rect.center().y() + knob_size * 0.5f },
        { track_rect.width() + knob_size, knob_size }
    );

    const bool hovered = hit_rect.contains(mouse_pos) && layout.clip_rect.contains(mouse_pos) && !dropdown_blocking_input(layout);

    if (hovered && ctx.input.mouse_button_pressed(mouse_button::button_1)) {
        layout.panel_state->active_id = slider_id;
    }

    const float min_val = prop.range_min();
    const float max_val = prop.range_max();

    const auto* pending = layout.panel_state->find_pending(prop.category(), prop.name());
    float current_val = pending ? std::get<float>(pending->value) : prop.as_float();

    if (layout.panel_state->active_id == slider_id) {
        const float t = std::clamp(
            (mouse_pos.x() - track_rect.left()) / track_rect.width(),
            0.f, 1.f
        );
        const float new_val = min_val + t * (max_val - min_val);

        queue_change(*layout.panel_state, prop, new_val, [new_val](save::property_base& p) {
            p.set_float(new_val);
        });

        current_val = new_val;
    }

    ctx.sprites.push_back({
        .rect = track_rect,
        .color = sty.color_widget_background,
        .texture = ctx.blank_texture,
        .layer = ctx.layer
    });

    const float t = (current_val - min_val) / (max_val - min_val);
    const gui::ui_rect fill_rect = gui::ui_rect::from_position_size(
        track_rect.top_left(),
        { track_rect.width() * t, slider_height }
    );

    ctx.sprites.push_back({
        .rect = fill_rect,
        .color = sty.color_slider_fill,
        .texture = ctx.blank_texture,
        .layer = ctx.layer
    });

    const float knob_x = track_rect.left() + track_rect.width() * t - knob_size * 0.5f;
    const gui::ui_rect knob_rect = gui::ui_rect::from_position_size(
        { knob_x, track_rect.center().y() + knob_size * 0.5f },
        { knob_size, knob_size }
    );

    const bool is_active = layout.panel_state->active_id == slider_id;
    const unitless::vec4 knob_color = (hovered || is_active)
        ? sty.color_handle_hovered
        : sty.color_handle;

    ctx.sprites.push_back({
        .rect = knob_rect,
        .color = knob_color,
        .texture = ctx.blank_texture,
        .layer = ctx.layer
    });

    if (ctx.font.valid()) {
        auto label = std::string(prop.name());
        if (prop.requires_restart()) {
            label += " *";
        }

        ctx.texts.push_back({
            .font = ctx.font,
            .text = label,
            .position = {
                row_rect.left() + sty.padding,
                row_rect.center().y() + sty.font_size * 0.35f
            },
            .scale = sty.font_size,
            .color = prop.requires_restart() ? sty.color_text_secondary : sty.color_text,
            .clip_rect = layout.clip_rect,
            .layer = ctx.layer
        });

        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.2f", current_val);
        const float value_width = ctx.font->width(buf, sty.font_size);

        ctx.texts.push_back({
            .font = ctx.font,
            .text = buf,
            .position = {
                track_rect.left() - value_width - sty.padding,
                row_rect.center().y() + sty.font_size * 0.35f
            },
            .scale = sty.font_size,
            .color = sty.color_text_secondary,
            .clip_rect = layout.clip_rect,
            .layer = ctx.layer
        });
    }
}

auto gse::settings_panel::draw_choice(layout_state& layout, const save::property_base& prop) -> void {
    const auto& sty = layout.ctx->style;
    const auto& ctx = *layout.ctx;

    const id dropdown_id = gui::ids::make(std::string(prop.name()) + "_dropdown");
    const bool is_open = layout.panel_state->dropdown.open_dropdown_id == dropdown_id;

    layout.cursor.y() -= layout.row_height;

    if (!layout.is_row_visible() && !is_open) {
        return;
    }

    const gui::ui_rect row_rect = gui::ui_rect::from_position_size(
        { layout.cursor.x(), layout.cursor.y() + layout.row_height },
        { layout.content_rect.width(), layout.row_height }
    );

    float max_option_width = 80.f;
    if (ctx.font.valid()) {
        for (std::size_t i = 0; i < prop.enum_count(); ++i) {
            const auto label = prop.enum_label(i);
            max_option_width = std::max(
                max_option_width,
                ctx.font->width(std::string(label), sty.font_size) + sty.padding * 4.f
            );
        }
    }

    const gui::ui_rect header_rect = gui::ui_rect::from_position_size(
        { row_rect.right() - max_option_width, row_rect.center().y() + layout.row_height * 0.4f },
        { max_option_width, layout.row_height * 0.8f }
    );

    const unitless::vec2 mouse_pos = ctx.input.mouse_position();

    if (row_rect.contains(mouse_pos) && layout.clip_rect.contains(mouse_pos)) {
        set_tooltip(ctx, dropdown_id, std::string(prop.description()));
    }

    const auto* pending = layout.panel_state->find_pending(prop.category(), prop.name());
    const std::size_t current_index = pending ? std::get<std::size_t>(pending->value) : prop.enum_index();

    const bool header_hovered = header_rect.contains(mouse_pos) && layout.clip_rect.contains(mouse_pos);
    if (header_hovered && ctx.input.mouse_button_pressed(mouse_button::button_1)) {
        layout.panel_state->active_id = dropdown_id;
    }

    if (header_hovered && layout.panel_state->active_id == dropdown_id &&
        ctx.input.mouse_button_released(mouse_button::button_1)) {
        if (is_open) {
            layout.panel_state->dropdown.open_dropdown_id.reset();
        }
        else {
            layout.panel_state->dropdown.open_dropdown_id = dropdown_id;
        }
        layout.panel_state->active_id.reset();
    }

    unitless::vec4 header_bg = sty.color_widget_background;
    if (is_open) {
        header_bg = sty.color_widget_active;
    }
    else if (layout.panel_state->active_id == dropdown_id) {
        header_bg = sty.color_widget_active;
    }
    else if (header_hovered) {
        header_bg = sty.color_widget_hovered;
    }

    ctx.sprites.push_back({
        .rect = header_rect,
        .color = header_bg,
        .texture = ctx.blank_texture,
        .layer = ctx.layer
    });

    if (ctx.font.valid()) {
        const auto current_label = prop.enum_label(current_index);
        const std::string opt_text(current_label);

        ctx.texts.push_back({
            .font = ctx.font,
            .text = opt_text,
            .position = {
                header_rect.left() + sty.padding,
                header_rect.center().y() + sty.font_size * 0.35f
            },
            .scale = sty.font_size,
            .color = sty.color_text,
            .clip_rect = header_rect,
            .layer = ctx.layer
        });

        const std::string arrow = is_open ? "^" : "v";
        const float arrow_width = ctx.font->width(arrow, sty.font_size);
        ctx.texts.push_back({
            .font = ctx.font,
            .text = arrow,
            .position = {
                header_rect.right() - arrow_width - sty.padding,
                header_rect.center().y() + sty.font_size * 0.35f
            },
            .scale = sty.font_size,
            .color = sty.color_text_secondary,
            .clip_rect = header_rect,
            .layer = ctx.layer
        });

        auto label = std::string(prop.name());
        if (prop.requires_restart()) {
            label += " *";
        }

        ctx.texts.push_back({
            .font = ctx.font,
            .text = label,
            .position = {
                row_rect.left() + sty.padding,
                row_rect.center().y() + sty.font_size * 0.35f
            },
            .scale = sty.font_size,
            .color = prop.requires_restart() ? sty.color_text_secondary : sty.color_text,
            .clip_rect = layout.clip_rect,
            .layer = ctx.layer
        });
    }

    if (is_open && prop.enum_count() > 0) {
        constexpr std::size_t max_visible = 8;
        const std::size_t visible_count = std::min(prop.enum_count(), max_visible);
        const float option_height = layout.row_height * 0.8f;
        const float list_height = static_cast<float>(visible_count) * option_height;

        const gui::ui_rect list_rect = gui::ui_rect::from_position_size(
            { header_rect.left(), header_rect.bottom() },
            { max_option_width, list_height }
        );

        if (ctx.input_layers) {
            ctx.input_layers->register_hit_region(render_layer::modal, list_rect);
        }

        constexpr float border = 1.f;
        ctx.sprites.push_back({
            .rect = list_rect.inset({ -border, -border }),
            .color = sty.color_border,
            .texture = ctx.blank_texture,
            .layer = render_layer::modal
        });

        ctx.sprites.push_back({
            .rect = list_rect,
            .color = sty.color_menu_body,
            .texture = ctx.blank_texture,
            .layer = render_layer::modal
        });

        float option_y = list_rect.top();
        for (std::size_t i = 0; i < visible_count; ++i) {
            const gui::ui_rect option_rect = gui::ui_rect::from_position_size(
                { list_rect.left(), option_y },
                { list_rect.width(), option_height }
            );

            const bool option_hovered = option_rect.contains(mouse_pos);

            unitless::vec4 option_bg = sty.color_menu_body;
            if (i == current_index) {
                option_bg = sty.color_widget_selected;
            }
            else if (option_hovered) {
                option_bg = sty.color_widget_hovered;
            }

            ctx.sprites.push_back({
                .rect = option_rect,
                .color = option_bg,
                .texture = ctx.blank_texture,
                .layer = render_layer::modal
            });

            if (ctx.font.valid()) {
                ctx.texts.push_back({
                    .font = ctx.font,
                    .text = std::string(prop.enum_label(i)),
                    .position = {
                        option_rect.left() + sty.padding,
                        option_rect.center().y() + sty.font_size * 0.35f
                    },
                    .scale = sty.font_size,
                    .color = sty.color_text,
                    .clip_rect = option_rect,
                    .layer = render_layer::modal
                });
            }

            if (option_hovered && ctx.input.mouse_button_pressed(mouse_button::button_1)) {
                queue_change(*layout.panel_state, prop, i, [i](save::property_base& p) {
                    p.set_enum_index(i);
                });
                layout.panel_state->dropdown.open_dropdown_id.reset();
            }

            option_y -= option_height;
        }

        if (!list_rect.contains(mouse_pos) &&
            !header_rect.contains(mouse_pos) &&
            ctx.input.mouse_button_pressed(mouse_button::button_1)) {
            layout.panel_state->dropdown.open_dropdown_id.reset();
        }
    }
}

auto gse::settings_panel::queue_change(state& panel_state, const save::property_base& prop, const pending_value value, std::function<void(save::property_base&)> apply) -> void {
    const auto cat = std::string(prop.category());
    const auto name = std::string(prop.name());

    bool matches_original = false;
    match(value)
        .if_is([&](const bool val) {
	        matches_original = (val == prop.as_bool());
        })
        .else_if_is([&](const float val) {
	        matches_original = (std::abs(val - prop.as_float()) < 0.0001f);
        })
        .else_if_is([&](const std::size_t val) {
	        matches_original = (val == prop.enum_index());
        });

    for (auto it = panel_state.pending_changes.begin(); it != panel_state.pending_changes.end(); ++it) {
        if (it->category == cat && it->name == name) {
            if (matches_original) {
                panel_state.pending_changes.erase(it);
            } else {
                it->value = value;
                it->apply = std::move(apply);
            }
            return;
        }
    }

    if (!matches_original) {
        panel_state.pending_changes.push_back({
            .category = cat,
            .name = name,
            .apply = std::move(apply),
            .value = value,
            .requires_restart = prop.requires_restart()
        });
    }
}

auto gse::settings_panel::draw_apply_button(layout_state& layout, state& panel_state) -> bool {
    const auto& sty = layout.ctx->style;
    const auto& ctx = *layout.ctx;

    constexpr float button_width = 120.f;
    const float button_height = layout.row_height;

    const gui::ui_rect button_rect = gui::ui_rect::from_position_size(
        { layout.content_rect.center().x() - button_width * 0.5f, layout.cursor.y() },
        { button_width, button_height }
    );

    const unitless::vec2 mouse_pos = ctx.input.mouse_position();
    const bool hovered = button_rect.contains(mouse_pos) && !dropdown_blocking_input(layout);
    const id button_id = gui::ids::make("apply_settings");

    const bool has_changes = !panel_state.pending_changes.empty();

    if (hovered && ctx.input.mouse_button_pressed(mouse_button::button_1) && has_changes) {
        panel_state.active_id = button_id;
    }

    unitless::vec4 bg_color = sty.color_widget_background;
    if (!has_changes) {
        bg_color = sty.color_widget_background * 0.5f;
        bg_color.w() = 1.f;
    } else if (panel_state.active_id == button_id) {
        bg_color = sty.color_widget_active;
    } else if (hovered) {
        bg_color = sty.color_widget_hovered;
    }

    ctx.sprites.push_back({
        .rect = button_rect,
        .color = bg_color,
        .texture = ctx.blank_texture,
        .layer = ctx.layer
    });

    if (ctx.font.valid()) {
        const std::string text = has_changes
            ? "Apply (" + std::to_string(panel_state.pending_changes.size()) + ")"
            : "Apply";

        const float text_width = ctx.font->width(text, sty.font_size);
        const unitless::vec2 text_pos = {
            button_rect.center().x() - text_width * 0.5f,
            button_rect.center().y() + sty.font_size * 0.35f
        };

        ctx.texts.push_back({
            .font = ctx.font,
            .text = text,
            .position = text_pos,
            .scale = sty.font_size,
            .color = has_changes ? sty.color_text : sty.color_text_secondary,
            .clip_rect = button_rect,
            .layer = ctx.layer
        });
    }

    layout.cursor.y() -= button_height + sty.padding;

    bool clicked = false;
    if (ctx.input.mouse_button_released(mouse_button::button_1) &&
        panel_state.active_id == button_id && hovered && has_changes) {
        clicked = true;
    }

    return clicked;
}

auto gse::settings_panel::draw_restart_popup(state& panel_state, const context& ctx, const gui::ui_rect& panel_rect) -> void {
    const auto& sty = ctx.style;

    constexpr float popup_width = 350.f;
    constexpr float popup_height = 160.f;
    constexpr float btn_width = 120.f;
    constexpr float btn_height = 32.f;
    constexpr float btn_gap = 24.f;

    const gui::ui_rect popup_rect = gui::ui_rect::from_position_size(
        { panel_rect.center().x() - popup_width * 0.5f, panel_rect.center().y() + popup_height * 0.5f },
        { popup_width, popup_height }
    );

    // Darken background
    ctx.sprites.push_back({
        .rect = panel_rect,
        .color = unitless::vec4(0.f, 0.f, 0.f, 0.5f),
        .texture = ctx.blank_texture,
        .layer = render_layer::modal
    });

    // Border first (behind popup)
    constexpr float border = 2.f;
    ctx.sprites.push_back({
        .rect = popup_rect.inset({ -border, -border }),
        .color = sty.color_border,
        .texture = ctx.blank_texture,
        .layer = render_layer::modal
    });

    // Popup background
    ctx.sprites.push_back({
        .rect = popup_rect,
        .color = sty.color_menu_body,
        .texture = ctx.blank_texture,
        .layer = render_layer::modal
    });

    const float title_font_size = sty.font_size * 1.2f;
    const float content_top = popup_rect.top() - sty.padding;

    if (ctx.font.valid()) {
        // Title - near top of popup
        const std::string title = "Restart Required";
        const float title_width = ctx.font->width(title, title_font_size);
        ctx.texts.push_back({
            .font = ctx.font,
            .text = title,
            .position = {
                popup_rect.center().x() - title_width * 0.5f,
                content_top - title_font_size * 0.3f
            },
            .scale = title_font_size,
            .color = sty.color_text,
            .layer = render_layer::modal
        });

        // Message - center of popup
        const std::string msg = "Some settings require a restart.";
        const float msg_width = ctx.font->width(msg, sty.font_size);
        ctx.texts.push_back({
            .font = ctx.font,
            .text = msg,
            .position = {
                popup_rect.center().x() - msg_width * 0.5f,
                popup_rect.center().y() + sty.font_size * 0.3f
            },
            .scale = sty.font_size,
            .color = sty.color_text_secondary,
            .layer = render_layer::modal
        });
    }

    // Buttons - near bottom of popup
    const float total_btns_width = btn_width * 2.f + btn_gap;
    const float btn_start_x = popup_rect.center().x() - total_btns_width * 0.5f;
    const float btn_y = popup_rect.bottom() + sty.padding + btn_height;

    const gui::ui_rect later_rect = gui::ui_rect::from_position_size(
        { btn_start_x, btn_y },
        { btn_width, btn_height }
    );

    const gui::ui_rect restart_rect = gui::ui_rect::from_position_size(
        { btn_start_x + btn_width + btn_gap, btn_y },
        { btn_width, btn_height }
    );

    const unitless::vec2 mouse_pos = ctx.input.mouse_position();

    const bool later_hovered = later_rect.contains(mouse_pos);
    ctx.sprites.push_back({
        .rect = later_rect,
        .color = later_hovered ? sty.color_widget_hovered : sty.color_widget_background,
        .texture = ctx.blank_texture,
        .layer = render_layer::modal
    });

    if (ctx.font.valid()) {
        const std::string later_text = "Later";
        const float later_width = ctx.font->width(later_text, sty.font_size);
        ctx.texts.push_back({
            .font = ctx.font,
            .text = later_text,
            .position = {
                later_rect.center().x() - later_width * 0.5f,
                later_rect.center().y() + sty.font_size * 0.35f
            },
            .scale = sty.font_size,
            .color = sty.color_text,
            .layer = render_layer::modal
        });
    }

    const bool restart_hovered = restart_rect.contains(mouse_pos);
    ctx.sprites.push_back({
        .rect = restart_rect,
        .color = restart_hovered ? unitless::vec4(0.3f, 0.6f, 0.3f, 1.f) : unitless::vec4(0.2f, 0.5f, 0.2f, 1.f),
        .texture = ctx.blank_texture,
        .layer = render_layer::modal
    });

    if (ctx.font.valid()) {
        const std::string restart_text = "Restart Now";
        const float restart_width = ctx.font->width(restart_text, sty.font_size);
        ctx.texts.push_back({
            .font = ctx.font,
            .text = restart_text,
            .position = {
                restart_rect.center().x() - restart_width * 0.5f,
                restart_rect.center().y() + sty.font_size * 0.35f
            },
            .scale = sty.font_size,
            .color = sty.color_text,
            .layer = render_layer::modal
        });
    }

    if (ctx.input.mouse_button_pressed(mouse_button::button_1)) {
        if (later_hovered) {
            panel_state.active_popup = popup_type::none;
        } else if (restart_hovered) {
            panel_state.active_popup = popup_type::none;
            if (ctx.request_save) {
                ctx.request_save();
            }
            app::restart();
        }
    }
}
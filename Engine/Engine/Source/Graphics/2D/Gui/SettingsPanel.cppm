export module gse.graphics:settings_panel;

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
import gse.meta;
import gse.save;

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
        std::string action_being_rebound;

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
        std::function<void()> request_restart;
        gui::tooltip_state* tooltip = nullptr;
        gui::input_layer* input_layers = nullptr;
        std::function<std::vector<actions::action_binding_info>()> all_bindings;
        std::function<void(std::string_view, key)> rebind;
        std::function<key()> pressed_key;
    };

    auto update(
        state& state,
        const context& ctx,
        const gui::ui_rect& rect,
        bool is_open,
        const save::state& save_sys
    ) -> bool;

    auto default_panel_rect(
        const gui::style& style,
        vec2f viewport_size
    ) -> gui::ui_rect;
}

namespace gse::settings_panel {
    struct layout_state {
        vec2f cursor;
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

    auto draw_key_binding(
        layout_state& layout,
        const std::string& action_name,
        key current_key,
        key default_key
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


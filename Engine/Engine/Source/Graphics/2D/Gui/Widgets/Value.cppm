export module gse.graphics:value_widget;

import std;

import gse.physics.math;

import :types;
import :styles;

export namespace gse::gui::draw {
    template <is_arithmetic T>
    auto value(
        const draw_context& ctx,
        const std::string& name,
        T value
    ) -> void;

    template <is_quantity T, auto Unit = typename T::default_unit{}>
    auto value(
        const draw_context& ctx,
        const std::string& name,
        T value
    ) -> void;

    template <typename T, int N, auto Unit = typename T::default_unit{}>
    auto vec(
        const draw_context& ctx,
        const std::string& name,
        const vec_t<T, N>& vec
    ) -> void;

    template <typename T, int N>
    auto vec(
        const draw_context& ctx,
        const std::string& name,
        unitless::vec_t<T, N> vec
    ) -> void;
}

namespace gse::gui::draw {
    auto value_box(
        const draw_context& ctx,
        const std::string& value,
        const ui_rect& rect
    ) -> void;

    template <std::size_t N>
    auto value_row(
        const draw_context& ctx,
        const std::string& name,
        const std::array<std::string, N>& values
    ) -> void;
}

template <gse::is_arithmetic T>
auto gse::gui::draw::value(const draw_context& ctx, const std::string& name, T value) -> void {
    if constexpr (std::is_floating_point_v<T>) {
        value_row<1>(ctx, name, { std::format("{:.2f}", value) });
    } else {
        value_row<1>(ctx, name, { std::format("{}", value) });
    }
}

template <gse::is_quantity T, auto Unit>
auto gse::gui::draw::value(const draw_context& ctx, const std::string& name, T value) -> void {
    value_row<1>(ctx, name, { std::format("{:.2f} {}", value.template as<Unit>(), Unit.unit_name) });
}

template <typename T, int N, auto Unit>
auto gse::gui::draw::vec(const draw_context& ctx, const std::string& name, const vec_t<T, N>& vec) -> void {
    std::array<std::string, N> values;
    const auto val_unitless = vec.template as<Unit>();

    std::apply(
        [&](auto&&... args) {
            int i = 0;
            ((values[i++] = std::format("{:.2f}", args)), ...);
        },
        val_unitless.data
    );

    value_row<N>(ctx, name, values);
}

template <typename T, int N>
auto gse::gui::draw::vec(const draw_context& ctx, const std::string& name, unitless::vec_t<T, N> vec) -> void {
    std::array<std::string, N> values;

    std::apply(
        [&](auto&&... args) {
            int i = 0;
            ((values[i++] = std::format("{:.2f}", args)), ...);
        },
        vec.data
    );

    value_row<N>(ctx, name, values);
}

auto gse::gui::draw::value_box(const draw_context& ctx, const std::string& value, const ui_rect& rect) -> void {
    ctx.queue_sprite({
        .rect = rect,
        .color = ctx.style.color_widget_background,
        .texture = ctx.blank_texture
    });

    const float text_width = ctx.font->width(value, ctx.style.font_size);
    const unitless::vec2 text_pos = {
        rect.center().x() - text_width / 2.f,
        rect.center().y() + ctx.style.font_size / 2.f
    };

    ctx.queue_text({
        .font = ctx.font,
        .text = value,
        .position = text_pos,
        .scale = ctx.style.font_size,
        .color = ctx.style.color_text,
        .clip_rect = rect
    });
}

template <std::size_t N>
auto gse::gui::draw::value_row(const draw_context& ctx, const std::string& name, const std::array<std::string, N>& values) -> void {
    if (!ctx.current_menu) {
        return;
    }

    const float widget_height = ctx.font->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
    const ui_rect content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });

    const ui_rect row_rect = ui_rect::from_position_size(
        { content_rect.left(), ctx.layout_cursor.y() },
        { content_rect.width(), widget_height }
    );

    const float label_width = content_rect.width() * 0.4f;
    const unitless::vec2 label_pos = {
        row_rect.left(),
        row_rect.center().y() + ctx.style.font_size / 2.f
    };

    ctx.queue_text({
        .font = ctx.font,
        .text = name,
        .position = label_pos,
        .scale = ctx.style.font_size,
        .color = ctx.style.color_text,
        .clip_rect = row_rect
    });

    const float values_total_width = content_rect.width() - label_width;
    const float all_spacing = ctx.style.padding * std::max(0.0f, static_cast<float>(N - 1));
    const float value_box_width = (values_total_width - all_spacing) / static_cast<float>(N);

    unitless::vec2 current_box_pos = { row_rect.left() + label_width, row_rect.top() };

    for (const std::string& value_str : values) {
        const ui_rect box_rect = ui_rect::from_position_size(current_box_pos, { value_box_width, widget_height });
        value_box(ctx, value_str, box_rect);
        current_box_pos.x() += value_box_width + ctx.style.padding;
    }

    ctx.layout_cursor.y() -= widget_height + ctx.style.padding;
}
export module gse.graphics:value_widget;

import std;

import :types;
import :text_renderer;

export namespace gse::gui::draw {
	template <is_arithmetic T>
	auto value(
		widget_context& context,
		const std::string& name,
		T value
	) -> void;

	template <is_quantity T, auto Unit = typename T::default_unit{}>
	auto value(
		widget_context& context,
		const std::string& name,
		T value
	) -> void;

	template <typename T, int N, auto Unit = typename T::default_unit{}>
	auto vec(
		widget_context& context,
		const std::string& name,
		const vec_t<T, N>& vec
	) -> void;

	template <typename T, int N>
	auto vec(
		widget_context& context,
		const std::string& name,
		unitless::vec_t<T, N> vec
	) -> void;
}

namespace gse::gui::draw {
	auto value(
		const widget_context& context,
		const std::string& value,
		const ui_rect& rect
	) -> void;

	template <std::size_t N>
	auto row(
		widget_context& context,
		const std::string& name,
		const std::array<std::string, N>& values
	) -> void;
}

template <gse::is_arithmetic T>
auto gse::gui::draw::value(widget_context& context, const std::string& name, T value) -> void {
    if constexpr (std::is_floating_point_v<T>) {
        draw::row<1>(context, name, { std::format("{:.2f}", value) });
    }
    else {
        draw::row<1>(context, name, { std::format("{}", value) });
    }
}

template <gse::is_quantity T, auto Unit>
auto gse::gui::draw::value(widget_context& context, const std::string& name, T value) -> void {
    draw::row<1>(context, name, { std::format("{:.2f} {}", value.template as<Unit>(), Unit.unit_name) });
}

template <typename T, int N, auto Unit>
auto gse::gui::draw::vec(widget_context& context, const std::string& name, const vec_t<T, N>& vec) -> void {
    std::array<std::string, N> values;
    const auto val_unitless = vec.template as<Unit>();
    std::apply(
        [&](auto&&... args) {
	        int i = 0;
	        ((values[i++] = std::format("{:.2f}", args)), ...);
		}, 
        val_unitless.data
    );
    draw::row<N>(context, name, values);
}

template <typename T, int N>
auto gse::gui::draw::vec(widget_context& context, const std::string& name, unitless::vec_t<T, N> vec) -> void {
    std::array<std::string, N> values;
    std::apply(
        [&](auto&&... args) {
	        int i = 0;
	        ((values[i++] = std::format("{:.2f}", args)), ...);
		}, 
        vec.data
    );
    draw::row<N>(context, name, values);
}

auto gse::gui::draw::value(const widget_context& context, const std::string& value, const ui_rect& rect) -> void {
	context.sprite_renderer.queue({
        .rect = rect,
        .color = context.style.color_widget_background,
        .texture = context.blank_texture
    });

    const float text_width = context.font->width(value, context.style.font_size);
    const unitless::vec2 text_pos = {
        rect.center().x() - text_width / 2.f,
        rect.center().y() + context.style.font_size / 2.f
    };

    context.text_renderer.draw_text({
        .font = context.font,
        .text = value,
        .position = text_pos,
        .scale = context.style.font_size,
        .clip_rect = rect
    });
}


template <std::size_t N>
auto gse::gui::draw::row(widget_context& context, const std::string& name, const std::array<std::string, N>& values) -> void {
	if (!context.current_menu) return;

    const float widget_height = context.font->line_height(context.style.font_size) + context.style.padding * 0.5f;
    const auto content_rect = context.current_menu->rect.inset({ context.style.padding, context.style.padding });

    const ui_rect row_rect = ui_rect::from_position_size(
        { content_rect.left(), context.layout_cursor.y() },
        { content_rect.width(), widget_height }
    );

    const float label_width = content_rect.width() * 0.4f;
    const unitless::vec2 label_pos = {
        row_rect.left(),
        row_rect.center().y() + context.style.font_size / 2.f
    };
    context.text_renderer.draw_text({
        .font = context.font,
        .text = name,
        .position = label_pos,
        .scale = context.style.font_size,
        .clip_rect = row_rect
    });

    const float values_total_width = content_rect.width() - label_width;
    const float all_spacing = context.style.padding * std::max(0.0f, static_cast<float>(N - 1));
    const float value_box_width = (values_total_width - all_spacing) / static_cast<float>(N);

    unitless::vec2 current_box_pos = { row_rect.left() + label_width, row_rect.top() };

    for (const auto& value_str : values) {
        const ui_rect box_rect = ui_rect::from_position_size(current_box_pos, { value_box_width, widget_height });
        draw::value(context, value_str, box_rect);
        current_box_pos.x() += value_box_width + context.style.padding;
    }

    context.layout_cursor.y() -= widget_height + context.style.padding;
}

export module gse.graphics:slider_widget;

import std;

import gse.platform;
import gse.physics.math;

import :types;
import :ids;

export namespace gse::gui::draw {
	template <is_arithmetic T>
	auto slider(
		widget_context& context,
		const std::string& name,
		T& value,
		const T& min,
		const T& max,
		id& hot_widget_id,
		id& active_widget_id
	) -> void;

	template <internal::is_quantity T, auto Unit = typename T::default_unit{}>
	auto slider(
		widget_context& context,
		const std::string& name,
		T& value,
		const T& min,
		const T& max,
		id& hot_widget_id,
		id& active_widget_id
	) -> void;

	template <typename T, std::size_t N>
	auto slider(
		widget_context& context,
		const std::string& name,
		unitless::vec_t<T, N>& vec,
		const unitless::vec_t<T, N>& min,
		const unitless::vec_t<T, N>& max,
		id& hot_widget_id,
		id& active_widget_id
	) -> void;

	template <typename T, std::size_t N, auto Unit = typename T::default_unit{}>
	auto slider(
		widget_context& context,
		const std::string& name,
		vec_t<T, N>& vec,
		const vec_t<T, N>& min,
		const vec_t<T, N>& max,
		id& hot_widget_id,
		id& active_widget_id
	) -> void;
}

namespace gse::gui::draw {
	template <typename T>
	auto slider_box(
		const widget_context& context,
		const ui_rect& rect,
		id widget_id,
		T& value,
		const T& min,
		const T& max,
		id& hot_widget_id,
		id active_widget_id
	) -> void;

	template <typename T, std::size_t N>
	auto slider_row(
		widget_context& context,
		const std::string& name,
		std::array<T*, N>& value_ptrs,
		const std::array<T, N>& min_values,
		const std::array<T, N>& max_values,
		id& hot_widget_id,
		id& active_widget_id
	) -> void;

	
}

template <gse::is_arithmetic T>
auto gse::gui::draw::slider(widget_context& context, const std::string& name, T& value, const T& min, const T& max, id& hot_widget_id, id& active_widget_id) -> void {
	std::array value_ptrs = { &value };
	const std::array min_values = { min };
	const std::array max_values = { max };
	slider_row<T, 1>(context, name, value_ptrs, min_values, max_values, hot_widget_id, active_widget_id);
}

template <gse::internal::is_quantity T, auto Unit>
auto gse::gui::draw::slider(widget_context& context, const std::string& name, T& value, const T& min, const T& max, id& hot_widget_id, id& active_widget_id) -> void {
	std::array value_ptrs = { &value };
	const std::array min_values = { min };
	const std::array max_values = { max };
	const std::string name_with_unit = name + " (" + std::string(Unit.unit_name) + ")";
	slider_row<T, 1>(context, name_with_unit, value_ptrs, min_values, max_values, hot_widget_id, active_widget_id);
}

template <typename T, std::size_t N>
auto gse::gui::draw::slider(widget_context& context, const std::string& name, unitless::vec_t<T, N>& vec, const unitless::vec_t<T, N>& min, const unitless::vec_t<T, N>& max, id& hot_widget_id, id& active_widget_id) -> void {
	std::array<T*, N> value_ptrs;
	std::array<T, N> min_values;
	std::array<T, N> max_values;

	for (std::size_t i = 0; i < N; ++i) {
		value_ptrs[i] = &vec.data[i];
		min_values[i] = min.data[i];
		max_values[i] = max.data[i];
	}

	slider_row<T, N>(context, name, value_ptrs, min_values, max_values, hot_widget_id, active_widget_id);
}

template <typename T, std::size_t N, auto Unit>
auto gse::gui::draw::slider(widget_context& context, const std::string& name, vec_t<T, N>& vec, const vec_t<T, N>& min, const vec_t<T, N>& max, id& hot_widget_id, id& active_widget_id) -> void {
	std::array<T*, N> value_ptrs;
	std::array<T, N> min_values;
	std::array<T, N> max_values;

	for (std::size_t i = 0; i < N; ++i) {
		value_ptrs[i] = &vec.data[i];
		min_values[i] = min.data[i];
		max_values[i] = max.data[i];
	}

	const std::string name_with_unit = name + " (" + std::string(Unit.unit_name) + ")";
	slider_row<T, N>(context, name_with_unit, value_ptrs, min_values, max_values, hot_widget_id, active_widget_id);
}

template <typename T>
auto gse::gui::draw::slider_box(const widget_context& context, const ui_rect& rect, id widget_id, T& value, const T& min, const T& max, id& hot_widget_id, id active_widget_id) -> void {
	if (rect.contains(mouse::position())) {
		hot_widget_id = widget_id;
	}

	if (active_widget_id == widget_id && mouse::held(mouse_button::button_1)) {
		const float mouse_x = mouse::position().x();
		const float relative_x = mouse_x - rect.left();
		const float ratio = std::clamp(relative_x / rect.width(), 0.0f, 1.0f);
		value = min + ratio * (max - min);
	}

	if (value < min) value = min;
	if (value > max) value = max;

	context.sprite_renderer.queue({
		.rect = rect,
		.color = context.style.color_widget_background,
		.texture = context.blank_texture
	});

	float fill_ratio = 0.0f;
	const auto range = max - min;

	bool range_is_zero = false;
	if constexpr (internal::is_quantity<decltype(range)>) {
		if (range.template as<decltype(range)::default_unit>() == typename decltype(range)::value_type{ 0 }) range_is_zero = true;
	}
	else {
		if (range == T{ 0 }) range_is_zero = true;
	}

	if (!range_is_zero) {
		fill_ratio = static_cast<float>((value - min) / range);
	}

	const ui_rect fill_rect = ui_rect::from_position_size(
		rect.top_left(),
		{ rect.width() * fill_ratio, rect.height() }
	);

	context.sprite_renderer.queue({
		.rect = fill_rect,
		.color = context.style.color_widget_fill,
		.texture = context.blank_texture
	});

	const std::string value_str = std::format("{:.2f}", value);
	const float text_width = context.font->width(value_str, context.style.font_size);
	const unitless::vec2 value_text_pos = { rect.center().x() - text_width / 2.f, rect.center().y() + context.style.font_size / 2.f };

	context.text_renderer.draw_text({
		.font = context.font,
		.text = value_str,
		.position = value_text_pos,
		.scale = context.style.font_size,
		.clip_rect = rect
	});
}

template <typename T, std::size_t N>
auto gse::gui::draw::slider_row(widget_context& context, const std::string& name, std::array<T*, N>& value_ptrs, const std::array<T, N>& min_values, const std::array<T, N>& max_values, id& hot_widget_id, id& active_widget_id) -> void {
	if (!context.current_menu) return;

	const float widget_height = context.font->line_height(context.style.font_size) + context.style.padding * 0.5f;

	const auto content_rect = context.current_menu->rect.inset(
		{ context.style.padding, context.style.padding }
	);

	const ui_rect row_rect = ui_rect::from_position_size(
		{ content_rect.left(), context.layout_cursor.y() }, 
		{ content_rect.width(), widget_height }
	);

	const float label_width = content_rect.width() * 0.4f;
	context.text_renderer.draw_text({ 
		.font = context.font,
		.text = name,
		.position = {
			row_rect.left(),
			row_rect.center().y() + context.style.font_size / 2.f
		},
		.scale = context.style.font_size, .clip_rect = row_rect
	});

	const float values_total_width = content_rect.width() - label_width;
	const float all_spacing = context.style.padding * std::max(0.0f, static_cast<float>(N - 1));
	const float slider_box_width = (values_total_width - all_spacing) / static_cast<float>(N);

	unitless::vec2 current_box_pos = { row_rect.left() + label_width, row_rect.top() };

	for (std::size_t i = 0; i < N; ++i) {
		const ui_rect box_rect = ui_rect::from_position_size(
			current_box_pos,
			{ slider_box_width, widget_height }
		);

		const id box_id = ids::make(name + "##" + std::to_string(i));

		slider_box(context, box_rect, box_id, *value_ptrs[i], min_values[i], max_values[i], hot_widget_id, active_widget_id);

		current_box_pos.x() += slider_box_width + context.style.padding;
	}

	context.layout_cursor.y() -= widget_height + context.style.padding;
}
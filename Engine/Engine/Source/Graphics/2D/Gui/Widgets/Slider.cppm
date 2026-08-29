export module gse.graphics:slider_widget;

import std;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.math;
import gse.core;
import gse.meta;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import :types;
import :ids;
import :styles;
import :layout_ops;
import :builder;
import :interaction;

export namespace gse::gui::draw {
	template <is_arithmetic T>
	auto slider(
		const draw_context& ctx,
		const std::string& name,
		T& value,
		T min,
		T max,
		id& hot_widget_id,
		id& active_widget_id
	) -> void;

	template <
		internal::is_quantity T,
	auto Unit = typename T::default_unit{}
	>
	auto slider(
		const draw_context& ctx,
		const std::string& name,
		T& value,
		T min,
		T max,
		id& hot_widget_id,
		id& active_widget_id
	) -> void;

	template <typename T, std::size_t N>
	requires is_arithmetic<T>
	auto slider(
		const draw_context& ctx,
		const std::string& name,
		vec<T,
			N>& v,
		vec<T,
			N> min,
		vec<T,
			N> max,
		id& hot_widget_id,
		id& active_widget_id
	) -> void;

	template <
		internal::is_quantity T,
	std::size_t N,
	auto Unit = typename T::default_unit{}
	>
	auto slider(
		const draw_context& ctx,
		const std::string& name,
		vec<T,
			N>& v,
		vec<T,
			N> min,
		vec<T,
			N> max,
		id& hot_widget_id,
		id& active_widget_id
	) -> void;
}

export namespace gse::gui {
	template <is_arithmetic T>
	struct slider {
		using result = void;
		struct params {
			std::string_view name;
			T& value;
			T min;
			T max;
		};
		static auto draw(draw_context& ctx, params p, id& hot, id& active, id&) -> void {
			draw::slider(ctx, std::string(p.name), p.value, p.min, p.max, hot, active);
		}
	};

	template <internal::is_quantity T, auto Unit = typename T::default_unit{}>
	struct quantity_slider {
		using result = void;
		struct params {
			std::string_view name;
			T& value;
			T min;
			T max;
		};
		static auto draw(draw_context& ctx, params p, id& hot, id& active, id&) -> void {
			draw::slider<T, Unit>(ctx, std::string(p.name), p.value, p.min, p.max, hot, active);
		}
	};

	template <is_arithmetic T, std::size_t N>
	struct vec_slider {
		using result = void;
		struct params {
			std::string_view name;
			vec<T, N>& value;
			vec<T, N> min;
			vec<T, N> max;
		};
		static auto draw(draw_context& ctx, params p, id& hot, id& active, id&) -> void {
			draw::slider<T, N>(ctx, std::string(p.name), p.value, p.min, p.max, hot, active);
		}
	};
}

namespace gse::gui::draw {
	template <typename T>
	auto slider_box(
		const draw_context& ctx,
		const rectf& rect,
		id widget_id,
		T& value,
		T min,
		T max,
		id& hot_widget_id,
		id active_widget_id
	) -> void;

	template <typename T, std::size_t N>
	auto slider_row(
		const draw_context& ctx,
		const std::string& name,
		std::array<T*, N>& value_ptrs,
		const std::array<T, N>& min_values,
		const std::array<T, N>& max_values,
		id& hot_widget_id,
		id& active_widget_id
	) -> void;
}

template <gse::is_arithmetic T>
auto gse::gui::draw::slider(const draw_context& ctx, const std::string& name, T& value, T min, T max, id& hot_widget_id, id& active_widget_id) -> void {
	std::array value_ptrs = { &value };
	const std::array min_values = { min };
	const std::array max_values = { max };
	slider_row<T, 1>(ctx, name, value_ptrs, min_values, max_values, hot_widget_id, active_widget_id);
}

template <gse::internal::is_quantity T, auto Unit>
auto gse::gui::draw::slider(const draw_context& ctx, const std::string& name, T& value, T min, T max, id& hot_widget_id, id& active_widget_id) -> void {
	std::array value_ptrs = { &value };
	const std::array min_values = { min };
	const std::array max_values = { max };
	constexpr std::string_view unit_name = Unit.unit_name;
	std::string name_with_unit;
	name_with_unit.reserve(name.size() + unit_name.size() + 3);
	name_with_unit += name;
	name_with_unit += " (";
	name_with_unit += unit_name;
	name_with_unit += ")";
	slider_row<T, 1>(ctx, name_with_unit, value_ptrs, min_values, max_values, hot_widget_id, active_widget_id);
}

template <typename T, std::size_t N>
requires gse::is_arithmetic<T>
auto gse::gui::draw::slider(const draw_context& ctx, const std::string& name, vec<T, N>& v, vec<T, N> min, vec<T, N> max, id& hot_widget_id, id& active_widget_id) -> void {
	std::array<T*, N> value_ptrs;
	std::array<T, N> min_values;
	std::array<T, N> max_values;

	for (std::size_t i = 0; i < N; ++i) {
		value_ptrs[i] = &v[i];
		min_values[i] = min[i];
		max_values[i] = max[i];
	}

	slider_row<T, N>(ctx, name, value_ptrs, min_values, max_values, hot_widget_id, active_widget_id);
}

template <gse::internal::is_quantity T, std::size_t N, auto Unit>
auto gse::gui::draw::slider(const draw_context& ctx, const std::string& name, vec<T, N>& v, vec<T, N> min, vec<T, N> max, id& hot_widget_id, id& active_widget_id) -> void {
	std::array<T*, N> value_ptrs;
	std::array<T, N> min_values;
	std::array<T, N> max_values;

	for (std::size_t i = 0; i < N; ++i) {
		value_ptrs[i] = &v[i];
		min_values[i] = min[i];
		max_values[i] = max[i];
	}

	constexpr std::string_view unit_name = Unit.unit_name;
	std::string name_with_unit;
	name_with_unit.reserve(name.size() + unit_name.size() + 3);
	name_with_unit += name;
	name_with_unit += " (";
	name_with_unit += unit_name;
	name_with_unit += ")";
	slider_row<T, N>(ctx, name_with_unit, value_ptrs, min_values, max_values, hot_widget_id, active_widget_id);
}

template <typename T>
auto gse::gui::draw::slider_box(const draw_context& ctx, const rectf& rect, const id widget_id, T& value, T min, T max, id& hot_widget_id, const id active_widget_id) -> void {
	using underlying = internal::vec_storage_type_t<T>;
	auto& value_u = *reinterpret_cast<underlying*>(&value);
	const underlying min_u = internal::to_storage(min);
	const underlying max_u = internal::to_storage(max);

	if (ctx.hovers(rect)) {
		hot_widget_id = widget_id;
	}

	if (active_widget_id == widget_id && ctx.mouse_held()) {
		const float mouse_x = ctx.mouse_position().x();
		const float relative_x = mouse_x - rect.left();
		const float ratio = std::clamp(relative_x / rect.width(), 0.0f, 1.0f);
		const float raw = static_cast<float>(min_u) + ratio * static_cast<float>(max_u - min_u);
		if constexpr (std::is_integral_v<underlying>) {
			value_u = static_cast<underlying>(std::lround(raw));
		}
		else {
			value_u = static_cast<underlying>(raw);
		}
	}

	if (value_u < min_u) {
		value_u = min_u;
	}
	if (value_u > max_u) {
		value_u = max_u;
	}

	ctx.queue_sprite({
		.rect = rect,
		.color = ctx.style.color_widget_background,
		.texture = ctx.blank_texture,
		.corner_radius = ctx.style.corner_radius
	});

	float fill_ratio = 0.0f;
	const underlying range_u = max_u - min_u;
	if (range_u != underlying{ 0 }) {
		fill_ratio = static_cast<float>(value_u - min_u) / static_cast<float>(range_u);
	}

	const rectf fill_rect = rectf::from_position_size(
		rect.top_left(),
		{ rect.width() * fill_ratio, rect.height() }
	);

	ctx.queue_sprite({
		.rect = fill_rect,
		.color = ctx.style.color_slider_fill,
		.texture = ctx.blank_texture,
		.corner_radius = ctx.style.corner_radius
	});

	thread_local std::string value_str;
	value_str.clear();
	if constexpr (std::is_floating_point_v<underlying>) {
		std::format_to(std::back_inserter(value_str), "{:.2f}", value_u);
	}
	else {
		std::format_to(std::back_inserter(value_str), "{}", value_u);
	}
	const auto code_view = ctx.fonts.code.resolve();
	const float text_width = code_view->width(value_str, ctx.style.font_size);
	const vec2f value_text_pos = { rect.center().x() - text_width / 2.f,
		rect.center().y() + code_view->vertical_center_offset(ctx.style.font_size) };

	ctx.queue_text({
		.font = ctx.fonts.code,
		.text = value_str,
		.position = value_text_pos,
		.scale = ctx.style.font_size,
		.color = ctx.style.color_text,
		.clip_rect = rect
	});
}

template <typename T, std::size_t N>
auto gse::gui::draw::slider_row(const draw_context& ctx, const std::string& name, std::array<T*, N>& value_ptrs, const std::array<T, N>& min_values, const std::array<T, N>& max_values, id& hot_widget_id, id& active_widget_id) -> void {
	if (!ctx.current_menu) {
		return;
	}

	const auto& sty = ctx.style;
	namespace lo = layout;
	using spec = lo::size_spec;

	const auto text_view = ctx.fonts.text.resolve();
	const float widget_height = text_view->line_height(sty.font_size) + sty.padding * 0.5f;
	const rectf row_rect = lo::reserve_row(ctx, widget_height, sty.padding);

	const auto [label_rect, value_area] = lo::split_horizontal<2>(
		row_rect,
		{
			spec::ratio(0.4f),
			spec::flex(),
		}
	);

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = name,
		.position = { label_rect.left(), label_rect.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_text,
		.clip_rect = label_rect
	});

	std::array<spec, N> box_specs;
	box_specs.fill(spec::flex());
	const auto box_rects = lo::split_horizontal<N>(value_area, box_specs, sty.padding);

	std::array<id, N> box_ids;
	const std::uint64_t name_key = stable_id(name);
	for (std::size_t i = 0; i < N; ++i) {
		box_ids[i] = ids::make_from_key(hash_combine(name_key, i));
	}

	for (std::size_t i = 0; i < N; ++i) {
		slider_box(
			ctx,
			box_rects[i],
			box_ids[i],
			*value_ptrs[i],
			min_values[i],
			max_values[i],
			hot_widget_id,
			active_widget_id
		);
	}

	const auto hot_is_ours = std::ranges::any_of(
		box_ids,
		[&](const id b) {
			return b == hot_widget_id;
		}
	);

	interaction::grab_active(active_widget_id, hot_widget_id, hot_is_ours && ctx.mouse_pressed_for(value_area));

	interaction::release_active(
		active_widget_id,
		std::span<const id>(box_ids),
		ctx.mouse_released()
	);
}
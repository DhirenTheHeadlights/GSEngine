export module gse.graphics:value_widget;

import std;

import gse.math;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;

import :types;
import :styles;
import :builder;

export namespace gse::gui::draw {
	template <is_arithmetic T>
	auto value(
		const draw_context& ctx,
		const std::string& name,
		T value
	) -> void;

	template <
		is_quantity T,
	auto Unit = typename T::default_unit{}
	>
	auto value(
		const draw_context& ctx,
		const std::string& name,
		T value
	) -> void;

	template <
		typename T,
	std::size_t N,
	auto Unit = typename T::default_unit{}
	>
	auto vec(
		const draw_context& ctx,
		const std::string& name,
		const gse::
		vec<T,
				N>& v
	) -> void;

	template <typename T, std::size_t N>
	auto vec(
		const draw_context& ctx,
		const std::string& name,
		gse::
		vec<T,
				N> v
	) -> void;
}

export namespace gse::gui {
	template <is_arithmetic T>
	struct value {
		using result = void;
		struct params {
			std::string_view name;
			T val;
		};
		static auto draw(draw_context& ctx, params p, id&, id&, id&) -> void {
			draw::value(ctx, std::string(p.name), p.val);
		}
	};

	template <internal::is_quantity T, auto Unit = typename T::default_unit{}>
	struct quantity_value {
		using result = void;
		struct params {
			std::string_view name;
			T val;
		};
		static auto draw(draw_context& ctx, params p, id&, id&, id&) -> void {
			draw::value<T, Unit>(ctx, std::string(p.name), p.val);
		}
	};

	template <is_arithmetic T, std::size_t N>
	struct vec_value {
		using result = void;
		struct params {
			std::string_view name;
			vec<T, N> val;
		};
		static auto draw(draw_context& ctx, params p, id&, id&, id&) -> void {
			draw::vec(ctx, std::string(p.name), p.val);
		}
	};

	template <internal::is_quantity T, std::size_t N, auto Unit = typename T::default_unit{}>
	struct quantity_vec_value {
		using result = void;
		struct params {
			std::string_view name;
			vec<T, N> val;
		};
		static auto draw(draw_context& ctx, params p, id&, id&, id&) -> void {
			draw::vec<T, N, Unit>(ctx, std::string(p.name), p.val);
		}
	};
}

namespace gse::gui::draw {
	auto value_box(
		const draw_context& ctx,
		const std::string& value,
		const rectf& rect
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
		value_row<1>(
			ctx,
			name,
			{ std::format("{:.2f}", value) }
		);
	}
	else {
		value_row<1>(
			ctx,
			name,
			{ std::format("{}", value) }
		);
	}
}

template <gse::is_quantity T, auto Unit>
auto gse::gui::draw::value(const draw_context& ctx, const std::string& name, T value) -> void {
	value_row<1>(
		ctx,
		name,
		{ std::format("{:.2f:{}}", value, std::string_view(Unit.unit_name)) }
	);
}

template <typename T, std::size_t N, auto Unit>
auto gse::gui::draw::vec(const draw_context& ctx, const std::string& name, const gse::vec<T, N>& v) -> void {
	std::array<std::string, N> values;
	for (std::size_t i = 0; i < N; ++i) {
		values[i] = std::format("{:.2f:{}!}", v[i], std::string_view(Unit.unit_name));
	}
	value_row<N>(ctx, name, values);
}

template <typename T, std::size_t N>
auto gse::gui::draw::vec(const draw_context& ctx, const std::string& name, gse::vec<T, N> v) -> void {
	std::array<std::string, N> values;

	for (std::size_t i = 0; i < N; ++i) {
		values[i] = std::format("{:.2f}", v[i]);
	}
	value_row<N>(ctx, name, values);
}

auto gse::gui::draw::value_box(const draw_context& ctx, const std::string& value, const rectf& rect) -> void {
	ctx.queue_sprite({
		.rect = rect,
		.color = ctx.style.color_widget_background,
		.texture = ctx.blank_texture,
		.corner_radius = ctx.style.corner_radius
	});

	const auto code_view = ctx.fonts.code.resolve();
	const float text_width = code_view->width(value, ctx.style.font_size);
	const vec2f text_pos = { rect.center().x() - text_width / 2.f,
		rect.center().y() + code_view->vertical_center_offset(ctx.style.font_size) };

	ctx.queue_text({
		.font = ctx.fonts.code,
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

	const auto text_view = ctx.fonts.text.resolve();
	const float widget_height = text_view->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
	const rectf content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });

	const rectf row_rect = rectf::from_position_size(
		{ content_rect.left(), ctx.layout_cursor.y() },
		{ content_rect.width(), widget_height }
	);

	const float label_width = content_rect.width() * 0.4f;

	const rectf label_rect = rectf::from_position_size(
		row_rect.top_left(),
		{ label_width, widget_height }
	);

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = name,
		.position = { label_rect.left(), label_rect.center().y() + text_view->vertical_center_offset(ctx.style.font_size) },
		.scale = ctx.style.font_size,
		.color = ctx.style.color_text,
		.clip_rect = label_rect
	});

	const float values_total_width = content_rect.width() - label_width;
	const float all_spacing = ctx.style.padding * std::max(0.0f, static_cast<float>(N - 1));
	const float value_box_width = (values_total_width - all_spacing) / static_cast<float>(N);

	vec2f current_box_pos = { row_rect.left() + label_width, row_rect.top() };

	for (const std::string& value_str : values) {
		const rectf box_rect = rectf::from_position_size(
			current_box_pos,
			{ value_box_width, widget_height }
		);
		value_box(ctx, value_str, box_rect);
		current_box_pos.x() += value_box_width + ctx.style.padding;
	}

	ctx.layout_cursor.y() -= widget_height + ctx.style.padding;
}
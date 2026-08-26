export module gse.graphics:scroll_widget;

import std;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.meta;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;

import :font;
import :texture;
import :ui_renderer;
import :styles;
import :types;
import :ids;
import :input_layers;
import :cursor;

namespace gse::gui {
	auto auto_scroll_delta(
		float distance
	) -> float;

	auto apply_auto_scroll(
		scroll_axis& axis,
		float delta,
		bool smooth
	) -> void;

	auto draw_auto_scroll_anchor(
		const draw_context& ctx,
		vec2f anchor
	) -> void;

	auto draw_scroll_bar(
		const draw_context& ctx,
		const scroll_bar_result& bar,
		const scroll_axis& axis
	) -> void;

	auto scroll_axis_advance(
		const draw_context& ctx,
		const rectf& visible_rect,
		const scroll_config& config,
		scroll_axis& axis,
		float visible_extent,
		float wheel_amount,
		bool horizontal,
		bool show
	) -> void;

	auto run_scroll_end(
		draw_context& ctx,
		scroll_state& state,
		const rectf& visible_rect,
		float content_start_y,
		float content_width,
		const scroll_config& config
	) -> void;
}

auto gse::gui::auto_scroll_delta(const float distance) -> float {
	constexpr float dead_zone = 8.f;
	constexpr float speed_scale = 90.f;
	constexpr float distance_scale = 90.f;
	constexpr float max_speed = 2600.f;
	const float magnitude = std::max(0.f, std::abs(distance) - dead_zone);
	const float speed = std::min(max_speed, (std::exp(magnitude / distance_scale) - 1.f) * speed_scale);
	return std::copysign(speed * system_clock::dt<time>().as<seconds>(), distance);
}

auto gse::gui::apply_auto_scroll(scroll_axis& axis, const float delta, const bool smooth) -> void {
	if (smooth) {
		axis.target += delta;
	}
	else {
		axis.offset += delta;
		axis.target = axis.offset;
	}
}

auto gse::gui::draw_auto_scroll_anchor(const draw_context& ctx, const vec2f anchor) -> void {
	constexpr float size = 18.f;
	constexpr float thickness = 2.f;
	const vec4f color = ctx.style.color_accent;
	ctx.queue_sprite({
		.rect = rectf::from_position_size({ anchor.x() - thickness * 0.5f, anchor.y() + size * 0.5f }, { thickness, size }),
		.color = color,
		.texture = ctx.blank_texture,
		.layer = ctx.current_layer,
	});
	ctx.queue_sprite({
		.rect = rectf::from_position_size({ anchor.x() - size * 0.5f, anchor.y() + thickness * 0.5f }, { size, thickness }),
		.color = color,
		.texture = ctx.blank_texture,
		.layer = ctx.current_layer,
	});
}

auto gse::gui::update_scroll_bar(scroll_axis& axis, const scroll_bar_input& input) -> scroll_bar_result {
	axis.content = input.content_extent;
	const float max_scroll = std::max(0.f, input.content_extent - input.visible_extent);
	if (max_scroll <= 0.f || input.visible_extent <= 0.f || input.content_extent <= 0.f) {
		axis.offset = 0.f;
		axis.target = 0.f;
		axis.held = false;
		axis.hovered = false;
		return {};
	}

	axis.target = std::clamp(axis.target, 0.f, max_scroll);
	axis.offset = std::clamp(axis.offset, 0.f, max_scroll);

	const float track_extent = input.horizontal ? input.track_rect.width() : input.track_rect.height();
	const float thumb_len = std::max(input.min_thumb, input.visible_extent / input.content_extent * track_extent);
	const float travel = std::max(0.f, track_extent - thumb_len);
	const float ratio = max_scroll > 0.f ? axis.offset / max_scroll : 0.f;
	const float thumb_pos = input.horizontal
		? input.track_rect.left() + ratio * travel
		: input.track_rect.top() - ratio * travel;
	const rectf thumb_rect = input.horizontal
		? rectf::from_position_size({ thumb_pos, input.track_rect.top() }, { thumb_len, input.track_rect.height() })
		: rectf::from_position_size({ input.track_rect.left(), thumb_pos }, { input.track_rect.width(), thumb_len });

	const float mouse_axis = input.horizontal ? input.mouse.x() : input.mouse.y();
	axis.hovered = thumb_rect.contains(input.mouse);

	bool used_press = false;
	if (input.mouse_pressed && axis.hovered) {
		axis.held = true;
		axis.grab = mouse_axis - thumb_pos;
		used_press = true;
	}
	if (axis.held) {
		if (input.mouse_held) {
			const float new_pos = mouse_axis - axis.grab;
			const float new_ratio = travel > 0.f
				? (input.horizontal ? (new_pos - input.track_rect.left()) / travel : (input.track_rect.top() - new_pos) / travel)
				: 0.f;
			axis.offset = std::clamp(new_ratio, 0.f, 1.f) * max_scroll;
			axis.target = axis.offset;
		}
		else {
			axis.held = false;
		}
	}
	if (input.mouse_pressed && !axis.hovered && !axis.held && input.track_rect.contains(input.mouse)) {
		const float new_ratio = travel > 0.f
			? (input.horizontal ? (mouse_axis - input.track_rect.left() - thumb_len * 0.5f) / travel : (input.track_rect.top() - mouse_axis - thumb_len * 0.5f) / travel)
			: 0.f;
		axis.target = std::clamp(new_ratio, 0.f, 1.f) * max_scroll;
		axis.offset = axis.target;
		used_press = true;
	}

	return {
		.track_rect = input.track_rect,
		.thumb_rect = thumb_rect,
		.visible = true,
		.hovered = axis.hovered,
		.held = axis.held,
		.used_press = used_press,
	};
}

auto gse::gui::draw_scroll_bar(const draw_context& ctx, const scroll_bar_result& bar, const scroll_axis& axis) -> void {
	if (!bar.visible) {
		return;
	}

	vec4f track_color = ctx.style.color_widget_background;
	track_color.w() *= 0.3f;
	ctx.queue_sprite({
		.rect = bar.track_rect,
		.color = track_color,
		.texture = ctx.blank_texture,
		.layer = ctx.current_layer,
	});
	ctx.queue_sprite({
		.rect = bar.thumb_rect,
		.color = axis.held ? ctx.style.color_widget_active : (bar.hovered ? ctx.style.color_widget_hovered : ctx.style.color_widget_background),
		.texture = ctx.blank_texture,
		.layer = ctx.current_layer,
	});
}

auto gse::gui::scroll_region(draw_context& ctx, const scroll_region_info& info) -> scroll_handle {
	if (!ctx.current_menu) {
		return {};
	}

	const rectf content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });

	const float available_width = info.size.x() > 0.f ? info.size.x() : content_rect.right() - ctx.layout_cursor.x();

	const float available_height = info.size.y() > 0.f ? info.size.y() : ctx.layout_cursor.y() - content_rect.bottom();

	const rectf visible_rect = rectf::from_position_size(
		{ ctx.layout_cursor.x(), ctx.layout_cursor.y() },
		{ available_width, std::max(0.f, available_height) }
	);

	const std::uint64_t key = hash_combine(ids::current_seed(), stable_id(info.id));
	scroll_state& state = ctx.widget_scrolls[key];

	const float saved_layout_y = ctx.layout_cursor.y();
	return scroll_handle{ ctx, state, visible_rect, saved_layout_y, info.config };
}

auto gse::gui::scroll_axis_advance(const draw_context& ctx, const rectf& visible_rect, const scroll_config& config, scroll_axis& axis, const float visible_extent, const float wheel_amount, const bool horizontal, const bool show) -> void {
	const float max_scroll = std::max(0.f, axis.content - visible_extent);

	if (max_scroll > 0.f && std::abs(wheel_amount) > 0.001f) {
		(config.smooth_scrolling ? axis.target : axis.offset) -= wheel_amount * config.scroll_speed;
	}
	if (config.smooth_scrolling) {
		axis.target = std::clamp(axis.target, 0.f, max_scroll);
		axis.offset += (axis.target - axis.offset) * config.smooth_factor;
	}
	else {
		axis.offset = std::clamp(axis.offset, 0.f, max_scroll);
	}
	if (std::abs(axis.offset - axis.target) < 0.5f) {
		axis.offset = axis.target;
	}
	if (max_scroll <= 0.f) {
		axis.offset = 0.f;
		axis.target = 0.f;
		return;
	}
	if (!show) {
		return;
	}

	const float w = config.scrollbar_width;
	const vec2f mouse = ctx.mouse_position();
	const rectf track_rect = horizontal
		? rectf::from_position_size({ visible_rect.left(), visible_rect.bottom() + w }, { visible_extent, w })
		: rectf::from_position_size({ visible_rect.right() - w, visible_rect.top() }, { w, visible_extent });

	if (ctx.hit_regions) {
		ctx.hit_regions->register_resize_block(track_rect);
	}

	const scroll_bar_result bar = update_scroll_bar(axis, {
		.track_rect = track_rect,
		.visible_extent = visible_extent,
		.content_extent = axis.content,
		.horizontal = horizontal,
		.mouse = mouse,
		.mouse_pressed = ctx.mouse_pressed() && ctx.input_available(),
		.mouse_held = ctx.mouse_held(),
		.min_thumb = config.scrollbar_min_height,
	});
	if (bar.used_press) {
		ctx.consume_press(mouse_button::button_1);
	}
	draw_scroll_bar(ctx, bar, axis);
}

auto gse::gui::scroll_area(const draw_context& ctx, scroll_state& state, const rectf& visible_rect, const vec2f content_size, const scroll_config& config) -> vec2f {
	state.y.content = content_size.y();
	state.x.content = content_size.x();

	const float visible_h = visible_rect.height();
	const float visible_w = visible_rect.width();
	const float max_y = std::max(0.f, content_size.y() - visible_h);
	const float max_x = std::max(0.f, content_size.x() - visible_w);

	const vec2f wheel = ctx.scroll_delta_for(visible_rect);
	const bool shift = ctx.key_held(key::left_shift) || ctx.key_held(key::right_shift);
	const bool redirect = shift && max_x > 0.f;

	const bool in_region = visible_rect.contains(ctx.mouse_position());
	const bool can_scroll = max_x > 0.f || max_y > 0.f;
	if (can_scroll && ctx.mouse_pressed_for(visible_rect, mouse_button::button_3)) {
		state.auto_scroll_active = !state.auto_scroll_active;
		state.auto_scroll_anchor = ctx.mouse_position();
	}
	else if (state.auto_scroll_active && ctx.mouse_pressed(mouse_button::button_3)) {
		state.auto_scroll_active = false;
		ctx.consume_press(mouse_button::button_3);
	}
	if (state.auto_scroll_active && (!can_scroll || ctx.mouse_pressed() || ctx.mouse_pressed(mouse_button::button_2) || ctx.key_pressed(key::escape))) {
		state.auto_scroll_active = false;
	}
	if (state.auto_scroll_active) {
		const vec2f mouse = ctx.mouse_position();
		if (max_y > 0.f) {
			apply_auto_scroll(state.y, auto_scroll_delta(state.auto_scroll_anchor.y() - mouse.y()), config.smooth_scrolling);
		}
		if (max_x > 0.f) {
			apply_auto_scroll(state.x, auto_scroll_delta(mouse.x() - state.auto_scroll_anchor.x()), config.smooth_scrolling);
		}
		set_style(cursor::style::omni_move);
		draw_auto_scroll_anchor(ctx, state.auto_scroll_anchor);
	}

	const bool show_y = max_y > 0.f && (!config.auto_hide_scrollbar || in_region || state.y.held || state.y.hovered);
	const bool show_x = max_x > 0.f && (!config.auto_hide_scrollbar || in_region || state.x.held || state.x.hovered);

	scroll_axis_advance(ctx, visible_rect, config, state.y, visible_h, redirect ? 0.f : wheel.y(), false, show_y);
	scroll_axis_advance(ctx, visible_rect, config, state.x, visible_w, wheel.x() + (redirect ? wheel.y() : 0.f), true, show_x);

	return { state.x.offset, state.y.offset };
}

auto gse::gui::run_scroll_end(draw_context& ctx, scroll_state& state, const rectf& visible_rect, const float content_start_y, const float content_width, const scroll_config& config) -> void {
	const float content_height = content_start_y - ctx.layout_cursor.y();
	scroll_area(ctx, state, visible_rect, { std::max(content_width, visible_rect.width()), content_height }, config);
}

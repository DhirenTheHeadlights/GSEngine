export module gse.graphics:scroll_widget;

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

import :font;
import :texture;
import :ui_renderer;
import :styles;
import :types;
import :ids;

namespace gse::gui {
	auto run_scroll_end(
		draw_context& ctx,
		scroll_state& state,
		const ui_rect& visible_rect,
		float content_start_y,
		const scroll_config& config
	) -> void;
}

auto gse::gui::scroll_region(draw_context& ctx, const scroll_region_info& info) -> scroll_handle {
	if (!ctx.current_menu) {
		return {};
	}

	const ui_rect content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });

	const float available_width = info.size.x() > 0.f ? info.size.x() : content_rect.right() - ctx.layout_cursor.x();

	const float available_height = info.size.y() > 0.f ? info.size.y() : ctx.layout_cursor.y() - content_rect.bottom();

	const ui_rect visible_rect = ui_rect::from_position_size(
		{ ctx.layout_cursor.x(), ctx.layout_cursor.y() },
		{ available_width, std::max(
							   0.f,
							   available_height
						   ) }
	);

	const std::uint64_t key = hash_combine(ids::current_seed(), stable_id(info.id));
	scroll_state& state = ctx.widget_scrolls[key];

	const float saved_layout_y = ctx.layout_cursor.y();
	return scroll_handle{ ctx, state, visible_rect, saved_layout_y, info.config };
}

auto gse::gui::run_scroll_end(draw_context& ctx, scroll_state& state, const ui_rect& visible_rect, const float content_start_y, const scroll_config& config) -> void {
	const float visible_height = visible_rect.height();
	const float content_height = content_start_y - ctx.layout_cursor.y();
	state.content_height = content_height;

	const float max_scroll = std::max(0.f, content_height - visible_height);

	const vec2f mouse_pos = ctx.input.mouse_position();
	const bool mouse_in_region = visible_rect.contains(mouse_pos);

	if (max_scroll > 0.f) {
		const float scroll_delta = ctx.scroll_delta_for(visible_rect).y();
		if (std::abs(scroll_delta) > 0.001f) {
			if (config.smooth_scrolling) {
				state.target_offset -= scroll_delta * config.scroll_speed;
			}
			else {
				state.offset -= scroll_delta * config.scroll_speed;
			}
		}
	}

	if (config.smooth_scrolling) {
		state.target_offset = std::clamp(state.target_offset, 0.f, max_scroll);
		state.offset += (state.target_offset - state.offset) * config.smooth_factor;
	}
	else {
		state.offset = std::clamp(state.offset, 0.f, max_scroll);
	}

	if (std::abs(state.offset - state.target_offset) < 0.5f) {
		state.offset = state.target_offset;
	}

	if (max_scroll <= 0.f) {
		state.offset = 0.f;
		state.target_offset = 0.f;
		return;
	}

	const bool show_scrollbar =
		!config.auto_hide_scrollbar || mouse_in_region || state.scrollbar_held || state.scrollbar_hovered;

	if (!show_scrollbar) {
		return;
	}

	const float scrollbar_track_height = visible_height;
	const float scrollbar_height =
		std::max(
			config.scrollbar_min_height,
			(visible_height / content_height) * scrollbar_track_height
		);

	const float scroll_ratio = state.offset / max_scroll;
	const float scrollbar_travel = scrollbar_track_height - scrollbar_height;
	const float scrollbar_y = visible_rect.top() - scroll_ratio * scrollbar_travel;

	const ui_rect scrollbar_track_rect = ui_rect::from_position_size(
		{ visible_rect.right() - config.scrollbar_width, visible_rect.top() },
		{ config.scrollbar_width, visible_height }
	);

	const ui_rect scrollbar_rect = ui_rect::from_position_size(
		{ visible_rect.right() - config.scrollbar_width, scrollbar_y },
		{ config.scrollbar_width, scrollbar_height }
	);

	state.scrollbar_hovered = scrollbar_rect.contains(mouse_pos);

	if (ctx.mouse_pressed_for(scrollbar_rect)) {
		state.scrollbar_held = true;
		state.scrollbar_grab_offset = mouse_pos.y() - scrollbar_y;
	}

	if (state.scrollbar_held) {
		if (ctx.input.mouse_button_held(mouse_button::button_1)) {
			const float new_scrollbar_y = mouse_pos.y() - state.scrollbar_grab_offset;
			const float new_ratio = (visible_rect.top() - new_scrollbar_y) / scrollbar_travel;
			const float new_offset = std::clamp(new_ratio, 0.f, 1.f) * max_scroll;
			state.offset = new_offset;
			state.target_offset = new_offset;
		}
		else {
			state.scrollbar_held = false;
		}
	}

	if (!scrollbar_rect.contains(mouse_pos) && !state.scrollbar_held && ctx.mouse_pressed_for(scrollbar_track_rect)) {
		const float click_ratio = (visible_rect.top() - mouse_pos.y()) / scrollbar_track_height;
		const float new_offset = std::clamp(click_ratio, 0.f, 1.f) * max_scroll;
		state.target_offset = new_offset;
		if (!config.smooth_scrolling) {
			state.offset = new_offset;
		}
	}

	vec4f track_color = ctx.style.color_widget_background;
	track_color.w() *= 0.3f;

	ctx.queue_sprite({
		.rect = scrollbar_track_rect,
		.color = track_color,
		.texture = ctx.blank_texture,
		.layer = ctx.current_layer,
	});

	vec4f bar_color = ctx.style.color_widget_background;
	if (state.scrollbar_held) {
		bar_color = ctx.style.color_widget_active;
	}
	else if (state.scrollbar_hovered) {
		bar_color = ctx.style.color_widget_hovered;
	}

	ctx.queue_sprite({
		.rect = scrollbar_rect,
		.color = bar_color,
		.texture = ctx.blank_texture,
		.layer = ctx.current_layer,
	});
}

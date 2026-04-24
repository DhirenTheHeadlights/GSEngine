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
import :builder;

export namespace gse::gui {
	using ui_rect = rect_t<vec2f>;

	struct scroll_state {
		float offset = 0.f;
		float velocity = 0.f;
		float target_offset = 0.f;
		float content_height = 0.f;
		bool scrollbar_held = false;
		bool scrollbar_hovered = false;
		float scrollbar_grab_offset = 0.f;
	};

	struct scroll_context {
		ui_rect visible_rect;
		ui_rect content_rect;
		float scroll_offset = 0.f;
		float content_start_y = 0.f;
	};

	struct scroll_config {
		float scrollbar_width = 8.f;
		float scrollbar_min_height = 20.f;
		float scroll_speed = 40.f;
		float smooth_factor = 0.15f;
		bool auto_hide_scrollbar = true;
		bool smooth_scrolling = true;
	};
}

export namespace gse::gui {
	auto scroll_area(
		builder& b,
		std::string_view scroll_id,
		float height,
		const std::function<void(builder&)>& content,
		const scroll_config& cfg = {}
	) -> void;
}

export namespace gse::gui::scroll {
	auto begin(
		const scroll_state& state,
		const ui_rect& visible_rect,
		const style& sty,
		const input::state& input,
		const scroll_config& config = {}
	) -> scroll_context;

	auto end(
		scroll_state& state,
		const scroll_context& ctx,
		float content_cursor_y,
		const style& sty,
		const input::state& input, const resource::handle<texture>& blank_texture,
		std::vector<renderer::sprite_command>& sprites,
		render_layer layer,
		const scroll_config& config = {}
	) -> void;

	[[nodiscard]] auto needs_scroll(
		const scroll_state& state,
		const ui_rect& visible_rect
	) -> bool;

	[[nodiscard]] auto apply_offset(
		float y,
		const scroll_context& ctx
	) -> float;

	[[nodiscard]] auto is_visible(
		const ui_rect& item_rect,
		const scroll_context& ctx
	) -> bool;
}

auto gse::gui::scroll::begin(
	const scroll_state& state,
	const ui_rect& visible_rect,
	const style& sty,
	const input::state& input,
	const scroll_config& config
) -> scroll_context {
	scroll_context ctx;
	ctx.visible_rect = visible_rect;
	ctx.content_rect = visible_rect;
	ctx.scroll_offset = state.offset;
	ctx.content_start_y = visible_rect.top() + state.offset;

	return ctx;
}

auto gse::gui::scroll::end(
	scroll_state& state,
	const scroll_context& ctx,
	const float content_cursor_y,
	const style& sty,
	const input::state& input,
	const resource::handle<texture>& blank_texture,
	std::vector<renderer::sprite_command>& sprites,
	const render_layer layer,
	const scroll_config& config
) -> void {
	const float visible_height = ctx.visible_rect.height();
	const float content_height = ctx.content_start_y - content_cursor_y;
	state.content_height = content_height;

	const float max_scroll = std::max(0.f, content_height - visible_height);

	const vec2f mouse_pos = input.mouse_position();
	const bool mouse_in_region = ctx.visible_rect.contains(mouse_pos);

	if (mouse_in_region && max_scroll > 0.f) {
		const float scroll_delta = input.scroll_delta().y();
		if (std::abs(scroll_delta) > 0.001f) {
			if (config.smooth_scrolling) {
				state.target_offset -= scroll_delta * config.scroll_speed;
			} else {
				state.offset -= scroll_delta * config.scroll_speed;
			}
		}
	}

	if (config.smooth_scrolling) {
		state.target_offset = std::clamp(state.target_offset, 0.f, max_scroll);
		state.offset += (state.target_offset - state.offset) * config.smooth_factor;
	} else {
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

	const bool show_scrollbar = !config.auto_hide_scrollbar ||
		mouse_in_region ||
		state.scrollbar_held ||
		state.scrollbar_hovered;

	if (!show_scrollbar) {
		return;
	}

	const float scrollbar_track_height = visible_height;
	const float scrollbar_height = std::max(
		config.scrollbar_min_height,
		(visible_height / content_height) * scrollbar_track_height
	);

	const float scroll_ratio = state.offset / max_scroll;
	const float scrollbar_travel = scrollbar_track_height - scrollbar_height;
	const float scrollbar_y = ctx.visible_rect.top() - (scroll_ratio * scrollbar_travel);

	const ui_rect scrollbar_track_rect = ui_rect::from_position_size(
		{ ctx.visible_rect.right() - config.scrollbar_width, ctx.visible_rect.top() },
		{ config.scrollbar_width, visible_height }
	);

	const ui_rect scrollbar_rect = ui_rect::from_position_size(
		{ ctx.visible_rect.right() - config.scrollbar_width, scrollbar_y },
		{ config.scrollbar_width, scrollbar_height }
	);

	state.scrollbar_hovered = scrollbar_rect.contains(mouse_pos);

	if (state.scrollbar_hovered && input.mouse_button_pressed(mouse_button::button_1)) {
		state.scrollbar_held = true;
		state.scrollbar_grab_offset = mouse_pos.y() - scrollbar_y;
	}

	if (state.scrollbar_held) {
		if (input.mouse_button_held(mouse_button::button_1)) {
			const float new_scrollbar_y = mouse_pos.y() - state.scrollbar_grab_offset;
			const float new_ratio = (ctx.visible_rect.top() - new_scrollbar_y) / scrollbar_travel;
			const float new_offset = std::clamp(new_ratio, 0.f, 1.f) * max_scroll;
			state.offset = new_offset;
			state.target_offset = new_offset;
		} else {
			state.scrollbar_held = false;
		}
	}

	if (scrollbar_track_rect.contains(mouse_pos) &&
		!scrollbar_rect.contains(mouse_pos) &&
		input.mouse_button_pressed(mouse_button::button_1) &&
		!state.scrollbar_held) {
		const float click_ratio = (ctx.visible_rect.top() - mouse_pos.y()) / scrollbar_track_height;
		const float new_offset = std::clamp(click_ratio, 0.f, 1.f) * max_scroll;
		state.target_offset = new_offset;
		if (!config.smooth_scrolling) {
			state.offset = new_offset;
		}
	}

	vec4f track_color = sty.color_widget_background;
	track_color.w() *= 0.3f;

	sprites.push_back({
		.rect = scrollbar_track_rect,
		.color = track_color,
		.texture = blank_texture,
		.layer = layer
	});

	vec4f bar_color = sty.color_widget_background;
	if (state.scrollbar_held) {
		bar_color = sty.color_widget_active;
	} else if (state.scrollbar_hovered) {
		bar_color = sty.color_widget_hovered;
	}

	sprites.push_back({
		.rect = scrollbar_rect,
		.color = bar_color,
		.texture = blank_texture,
		.layer = layer
	});
}

auto gse::gui::scroll::needs_scroll(const scroll_state& state, const ui_rect& visible_rect) -> bool {
	return state.content_height > visible_rect.height();
}

auto gse::gui::scroll::apply_offset(const float y, const scroll_context& ctx) -> float {
	return y + ctx.scroll_offset;
}

auto gse::gui::scroll::is_visible(const ui_rect& item_rect, const scroll_context& ctx) -> bool {
	const float item_top = item_rect.top() + ctx.scroll_offset;
	const float item_bottom = item_rect.bottom() + ctx.scroll_offset;
	return item_top >= ctx.visible_rect.bottom() && item_bottom <= ctx.visible_rect.top();
}

namespace {
	std::unordered_map<std::uint64_t, gse::gui::scroll_state> g_scroll_states;
}

export auto gse::gui::scroll_area(
	builder& b,
	const std::string_view scroll_id,
	const float height,
	const std::function<void(builder&)>& content,
	const scroll_config& cfg
) -> void {
	if (!b.ctx.current_menu) {
		return;
	}

	const ui_rect content_rect = b.ctx.current_menu->rect.inset({ b.ctx.style.padding, b.ctx.style.padding });
	const float available_height = height > 0.f ? height : (b.ctx.layout_cursor.y() - content_rect.bottom());

	const ui_rect scroll_rect = ui_rect::from_position_size(
		{ content_rect.left(), b.ctx.layout_cursor.y() },
		{ content_rect.width(), available_height }
	);

	const auto key = ids::stable_key(std::string(scroll_id));
	scroll_state& state = g_scroll_states[key];

	const float saved_y = b.ctx.layout_cursor.y();
	b.ctx.layout_cursor.y() = scroll_rect.top() + state.offset;

	auto saved_clip = b.ctx.current_menu->rect;
	b.ctx.current_menu->rect = ui_rect::from_position_size(
		{ scroll_rect.left(), scroll_rect.top() },
		{ scroll_rect.width(), scroll_rect.height() }
	);

	content(b);

	const float content_end_y = b.ctx.layout_cursor.y();
	b.ctx.current_menu->rect = saved_clip;

	auto scroll_ctx = scroll::begin(state, scroll_rect, b.ctx.style, b.ctx.input, cfg);
	scroll::end(
		state, scroll_ctx,
		content_end_y + state.offset,
		b.ctx.style, b.ctx.input,
		b.ctx.blank_texture, b.ctx.sprites, b.ctx.current_layer,
		cfg
	);

	b.ctx.layout_cursor.y() = saved_y - available_height - b.ctx.style.padding;
}

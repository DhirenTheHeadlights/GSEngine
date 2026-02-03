export module gse.graphics:scroll_widget;

import std;

import gse.platform;
import gse.utility;
import gse.physics.math;

import :rendering_context;
import :font;
import :texture;
import :ui_renderer;
import :styles;

export namespace gse::gui {
	using ui_rect = rect_t<unitless::vec2>;

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

export namespace gse::gui::scroll {
	auto begin(
		scroll_state& state,
		const ui_rect& visible_rect,
		const style& sty,
		const input::state& input,
		const scroll_config& config = {}
	) -> scroll_context;

	auto end(
		scroll_state& state,
		scroll_context& ctx,
		float content_cursor_y,
		const style& sty,
		const input::state& input,
		resource::handle<texture> blank_texture,
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
	scroll_state& state,
	const ui_rect& visible_rect,
	const style& sty,
	const input::state& input,
	const scroll_config& config
) -> scroll_context {
	scroll_context ctx;
	ctx.visible_rect = visible_rect;
	ctx.content_rect = visible_rect;
	ctx.scroll_offset = state.offset;
	ctx.content_start_y = visible_rect.top();

	return ctx;
}

auto gse::gui::scroll::end(
	scroll_state& state,
	scroll_context& ctx,
	const float content_cursor_y,
	const style& sty,
	const input::state& input,
	const resource::handle<texture> blank_texture,
	std::vector<renderer::sprite_command>& sprites,
	const render_layer layer,
	const scroll_config& config
) -> void {
	const float visible_height = ctx.visible_rect.height();
	const float content_height = ctx.content_start_y - content_cursor_y;
	state.content_height = content_height;

	const float max_scroll = std::max(0.f, content_height - visible_height);

	const unitless::vec2 mouse_pos = input.mouse_position();
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

	unitless::vec4 track_color = sty.color_widget_background;
	track_color.w() *= 0.3f;

	sprites.push_back({
		.rect = scrollbar_track_rect,
		.color = track_color,
		.texture = blank_texture,
		.layer = layer
	});

	unitless::vec4 bar_color = sty.color_widget_background;
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

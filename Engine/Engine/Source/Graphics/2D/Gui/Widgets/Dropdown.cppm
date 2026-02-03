export module gse.graphics:dropdown_widget;

import std;

import gse.platform;
import gse.utility;

import :types;
import :ids;
import :styles;
import :scroll_widget;

export namespace gse::gui {
	struct dropdown_state {
		id open_dropdown_id;
		scroll_state scroll;
	};

	struct dropdown_result {
		bool changed = false;
		std::size_t new_index = 0;
	};

	struct dropdown_config {
		std::size_t max_visible_items = 8;
		float scrollbar_width = 6.f;
	};
}

export namespace gse::gui::draw {
	auto dropdown(
		const draw_context& ctx,
		const std::string& name,
		std::size_t current_index,
		std::span<const std::string_view> options,
		dropdown_state& state,
		id& hot_widget_id,
		id& active_widget_id,
		const dropdown_config& config = {}
	) -> dropdown_result;

	auto dropdown(
		const draw_context& ctx,
		const std::string& name,
		std::size_t current_index,
		std::span<const std::string> options,
		dropdown_state& state,
		id& hot_widget_id,
		id& active_widget_id,
		const dropdown_config& config = {}
	) -> dropdown_result;
}

namespace gse::gui::draw {
	auto dropdown_impl(
		const draw_context& ctx,
		const std::string& name,
		std::size_t current_index,
		const std::function<std::size_t()>& option_count,
		const std::function<std::string_view(std::size_t)>& get_option,
		dropdown_state& state,
		id& hot_widget_id,
		id& active_widget_id,
		const dropdown_config& config
	) -> dropdown_result;
}

auto gse::gui::draw::dropdown(const draw_context& ctx, const std::string& name, const std::size_t current_index, const std::span<const std::string_view> options, dropdown_state& state, id& hot_widget_id, id& active_widget_id, const dropdown_config& config) -> dropdown_result {
	return dropdown_impl(
		ctx, name, current_index,
		[&]() { return options.size(); },
		[&](const std::size_t i) { return options[i]; },
		state, hot_widget_id, active_widget_id, config
	);
}

auto gse::gui::draw::dropdown(const draw_context& ctx, const std::string& name, const std::size_t current_index, const std::span<const std::string> options, dropdown_state& state, id& hot_widget_id, id& active_widget_id, const dropdown_config& config) -> dropdown_result {
	return dropdown_impl(
		ctx, name, current_index,
		[&] { return options.size(); },
		[&](const std::size_t i) -> std::string_view { return options[i]; },
		state, hot_widget_id, active_widget_id, config
	);
}

auto gse::gui::draw::dropdown_impl(const draw_context& ctx, const std::string& name, const std::size_t current_index, const std::function<std::size_t()>& option_count, const std::function<std::string_view(std::size_t)>& get_option, dropdown_state& state, id& hot_widget_id, id& active_widget_id, const dropdown_config& config) -> dropdown_result {
	if (!ctx.current_menu) {
		return {};
	}

	const id dropdown_id = ids::make(name);
	const bool is_open = state.open_dropdown_id == dropdown_id;
	dropdown_result result;

	const float row_height = ctx.font->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
	const ui_rect content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });

	float max_option_width = 60.f;
	const std::size_t count = option_count();
	for (std::size_t i = 0; i < count; ++i) {
		const auto label = get_option(i);
		max_option_width = std::max(
			max_option_width,
			ctx.font->width(std::string(label), ctx.style.font_size) + ctx.style.padding * 4.f
		);
	}

	const ui_rect header_rect = ui_rect::from_position_size(
		{ content_rect.right() - max_option_width, ctx.layout_cursor.y() },
		{ max_option_width, row_height }
	);

	const bool header_hovered = header_rect.contains(ctx.input.mouse_position()) && ctx.input_available();

	if (header_hovered) {
		hot_widget_id = dropdown_id;
	}

	if (header_hovered && ctx.input.mouse_button_pressed(mouse_button::button_1)) {
		active_widget_id = dropdown_id;
	}

	if (header_hovered && active_widget_id == dropdown_id && ctx.input.mouse_button_released(mouse_button::button_1)) {
		if (is_open) {
			state.open_dropdown_id.reset();
		}
		else {
			state.open_dropdown_id = dropdown_id;
			state.scroll = {};
		}
		active_widget_id.reset();
	}

	unitless::vec4 header_bg = ctx.style.color_widget_background;
	if (is_open) {
		header_bg = ctx.style.color_widget_active;
	}
	else if (active_widget_id == dropdown_id) {
		header_bg = ctx.style.color_widget_active;
	}
	else if (header_hovered) {
		header_bg = ctx.style.color_widget_hovered;
	}

	ctx.queue_sprite({
		.rect = header_rect,
		.color = header_bg,
		.texture = ctx.blank_texture
	});

	const std::string current_label = current_index < count
		? std::string(get_option(current_index))
		: "";

	const float arrow_width = ctx.style.font_size * 0.6f;
	const float text_x = header_rect.left() + ctx.style.padding;

	ctx.queue_text({
		.font = ctx.font,
		.text = current_label,
		.position = {
			text_x,
			header_rect.center().y() + ctx.style.font_size * 0.35f
		},
		.scale = ctx.style.font_size,
		.color = ctx.style.color_text,
		.clip_rect = header_rect
	});

	const std::string arrow = is_open ? "^" : "v";
	const float arrow_x = header_rect.right() - arrow_width;
	ctx.queue_text({
		.font = ctx.font,
		.text = arrow,
		.position = {
			arrow_x,
			header_rect.center().y() + ctx.style.font_size * 0.35f
		},
		.scale = ctx.style.font_size,
		.color = ctx.style.color_text_secondary,
		.clip_rect = header_rect
	});

	ctx.layout_cursor.y() -= row_height + ctx.style.padding;

	if (is_open && count > 0) {
		const std::size_t visible_count = std::min(count, config.max_visible_items);
		const float visible_height = static_cast<float>(visible_count) * row_height;
		const float total_content_height = static_cast<float>(count) * row_height;
		const bool needs_scroll = count > config.max_visible_items;

		const float list_width = needs_scroll ? max_option_width + config.scrollbar_width : max_option_width;

		const ui_rect list_rect = ui_rect::from_position_size(
			{ header_rect.left(), header_rect.bottom() },
			{ list_width, visible_height }
		);

		const ui_rect content_area = needs_scroll
			? ui_rect::from_position_size(list_rect.top_left(), { max_option_width, visible_height })
			: list_rect;

		constexpr float border = 1.f;
		ctx.queue_sprite({
			.rect = list_rect.inset({ -border, -border }),
			.color = ctx.style.color_border,
			.texture = ctx.blank_texture,
			.layer = render_layer::modal
		});

		ctx.queue_sprite({
			.rect = list_rect,
			.color = ctx.style.color_menu_body,
			.texture = ctx.blank_texture,
			.layer = render_layer::modal
		});

		const float max_scroll = std::max(0.f, total_content_height - visible_height);
		const unitless::vec2 mouse_pos = ctx.input.mouse_position();
		const bool mouse_in_list = list_rect.contains(mouse_pos);

		if (mouse_in_list && needs_scroll) {
			const float scroll_delta = ctx.input.scroll_delta().y();
			if (std::abs(scroll_delta) > 0.001f) {
				state.scroll.target_offset -= scroll_delta * row_height * 2.f;
			}
		}

		state.scroll.target_offset = std::clamp(state.scroll.target_offset, 0.f, max_scroll);
		state.scroll.offset += (state.scroll.target_offset - state.scroll.offset) * 0.2f;
		if (std::abs(state.scroll.offset - state.scroll.target_offset) < 0.5f) {
			state.scroll.offset = state.scroll.target_offset;
		}
		state.scroll.content_height = total_content_height;

		const std::size_t first_visible = static_cast<std::size_t>(state.scroll.offset / row_height);
		const std::size_t last_visible = std::min(count, first_visible + visible_count + 2);

		for (std::size_t i = first_visible; i < last_visible; ++i) {
			const float item_y = list_rect.top() - static_cast<float>(i) * row_height + state.scroll.offset;

			if (item_y < list_rect.bottom() - row_height || item_y > list_rect.top() + row_height) {
				continue;
			}

			const ui_rect option_rect = ui_rect::from_position_size(
				{ content_area.left(), item_y },
				{ content_area.width(), row_height }
			);

			const float clipped_top = std::min(option_rect.top(), content_area.top());
			const float clipped_bottom = std::max(option_rect.bottom(), content_area.bottom());
			const float clipped_height = clipped_top - clipped_bottom;

			if (clipped_height <= 0.f) {
				continue;
			}

			const ui_rect clipped_option = ui_rect::from_position_size(
				{ option_rect.left(), clipped_top },
				{ content_area.width(), clipped_height }
			);

			const bool option_hovered = option_rect.contains(mouse_pos) && content_area.contains(mouse_pos);

			unitless::vec4 option_bg = ctx.style.color_menu_body;
			if (i == current_index) {
				option_bg = ctx.style.color_widget_selected;
			}
			else if (option_hovered) {
				option_bg = ctx.style.color_widget_hovered;
			}

			ctx.queue_sprite({
				.rect = clipped_option,
				.color = option_bg,
				.texture = ctx.blank_texture,
				.layer = render_layer::modal
			});

			ctx.queue_text({
				.font = ctx.font,
				.text = std::string(get_option(i)),
				.position = {
					option_rect.left() + ctx.style.padding,
					option_rect.center().y() + ctx.style.font_size * 0.35f
				},
				.scale = ctx.style.font_size,
				.color = ctx.style.color_text,
				.clip_rect = content_area,
				.layer = render_layer::modal
			});

			if (option_hovered && ctx.input.mouse_button_pressed(mouse_button::button_1)) {
				result.changed = true;
				result.new_index = i;
				state.open_dropdown_id.reset();
			}
		}

		if (needs_scroll) {
			const ui_rect scrollbar_track = ui_rect::from_position_size(
				{ list_rect.right() - config.scrollbar_width, list_rect.top() },
				{ config.scrollbar_width, visible_height }
			);

			unitless::vec4 track_color = ctx.style.color_widget_background;
			track_color.w() *= 0.3f;
			ctx.queue_sprite({
				.rect = scrollbar_track,
				.color = track_color,
				.texture = ctx.blank_texture,
				.layer = render_layer::modal
			});

			const float scrollbar_height = std::max(20.f, (visible_height / total_content_height) * visible_height);
			const float scroll_ratio = max_scroll > 0.f ? state.scroll.offset / max_scroll : 0.f;
			const float scrollbar_travel = visible_height - scrollbar_height;
			const float scrollbar_y = scrollbar_track.top() - scroll_ratio * scrollbar_travel;

			const ui_rect scrollbar_rect = ui_rect::from_position_size(
				{ scrollbar_track.left(), scrollbar_y },
				{ config.scrollbar_width, scrollbar_height }
			);

			state.scroll.scrollbar_hovered = scrollbar_rect.contains(mouse_pos);

			if (state.scroll.scrollbar_hovered && ctx.input.mouse_button_pressed(mouse_button::button_1)) {
				state.scroll.scrollbar_held = true;
				state.scroll.scrollbar_grab_offset = mouse_pos.y() - scrollbar_y;
			}

			if (state.scroll.scrollbar_held) {
				if (ctx.input.mouse_button_held(mouse_button::button_1)) {
					const float new_scrollbar_y = mouse_pos.y() - state.scroll.scrollbar_grab_offset;
					const float new_ratio = (scrollbar_track.top() - new_scrollbar_y) / scrollbar_travel;
					state.scroll.offset = std::clamp(new_ratio, 0.f, 1.f) * max_scroll;
					state.scroll.target_offset = state.scroll.offset;
				}
				else {
					state.scroll.scrollbar_held = false;
				}
			}

			unitless::vec4 bar_color = ctx.style.color_widget_background;
			if (state.scroll.scrollbar_held) {
				bar_color = ctx.style.color_widget_active;
			}
			else if (state.scroll.scrollbar_hovered) {
				bar_color = ctx.style.color_widget_hovered;
			}

			ctx.queue_sprite({
				.rect = scrollbar_rect,
				.color = bar_color,
				.texture = ctx.blank_texture,
				.layer = render_layer::modal
			});
		}

		if (!list_rect.contains(mouse_pos) &&
			!header_rect.contains(mouse_pos) &&
			ctx.input.mouse_button_pressed(mouse_button::button_1) &&
			!state.scroll.scrollbar_held) {
			state.open_dropdown_id.reset();
		}
	}

	return result;
}

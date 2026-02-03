export module gse.graphics:dropdown_widget;

import std;

import gse.platform;
import gse.utility;

import :types;
import :ids;
import :styles;

export namespace gse::gui {
	struct dropdown_state {
		id open_dropdown_id;
		float scroll_offset = 0.f;
	};

	struct dropdown_result {
		bool changed = false;
		std::size_t new_index = 0;
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
		id& active_widget_id
	) -> dropdown_result;

	auto dropdown(
		const draw_context& ctx,
		const std::string& name,
		std::size_t current_index,
		std::span<const std::string> options,
		dropdown_state& state,
		id& hot_widget_id,
		id& active_widget_id
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
		id& active_widget_id
	) -> dropdown_result;
}

auto gse::gui::draw::dropdown(const draw_context& ctx, const std::string& name, const std::size_t current_index, const std::span<const std::string_view> options, dropdown_state& state, id& hot_widget_id, id& active_widget_id) -> dropdown_result {
	return dropdown_impl(
		ctx, name, current_index,
		[&]() { return options.size(); },
		[&](const std::size_t i) { return options[i]; },
		state, hot_widget_id, active_widget_id
	);
}

auto gse::gui::draw::dropdown(const draw_context& ctx, const std::string& name, const std::size_t current_index, const std::span<const std::string> options, dropdown_state& state, id& hot_widget_id, id& active_widget_id) -> dropdown_result {
	return dropdown_impl(
		ctx, name, current_index,
		[&] { return options.size(); },
		[&](const std::size_t i) -> std::string_view { return options[i]; },
		state, hot_widget_id, active_widget_id
	);
}

auto gse::gui::draw::dropdown_impl(const draw_context& ctx, const std::string& name, const std::size_t current_index, const std::function<std::size_t()>& option_count, const std::function<std::string_view(std::size_t)>& get_option, dropdown_state& state, id& hot_widget_id, id& active_widget_id) -> dropdown_result {
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
			state.scroll_offset = 0.f;
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
	const float text_area_width = header_rect.width() - arrow_width - ctx.style.padding;
	const float text_width = ctx.font->width(current_label, ctx.style.font_size);
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
		constexpr std::size_t max_visible = 8;
		const std::size_t visible_count = std::min(count, max_visible);
		const float list_height = static_cast<float>(visible_count) * row_height;

		const ui_rect list_rect = ui_rect::from_position_size(
			{ header_rect.left(), header_rect.bottom() },
			{ max_option_width, list_height }
		);

		ctx.queue_sprite({
			.rect = list_rect,
			.color = ctx.style.color_menu_body,
			.texture = ctx.blank_texture,
			.layer = render_layer::modal
		});

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

		float option_y = list_rect.top();
		for (std::size_t i = 0; i < visible_count; ++i) {
			const ui_rect option_rect = ui_rect::from_position_size(
				{ list_rect.left(), option_y },
				{ list_rect.width(), row_height }
			);

			const bool option_hovered = option_rect.contains(ctx.input.mouse_position());
			const id option_id = ids::make(name + "_opt_" + std::to_string(i));

			unitless::vec4 option_bg = ctx.style.color_menu_body;
			if (i == current_index) {
				option_bg = ctx.style.color_widget_selected;
			}
			else if (option_hovered) {
				option_bg = ctx.style.color_widget_hovered;
			}

			ctx.queue_sprite({
				.rect = option_rect,
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
				.clip_rect = option_rect,
				.layer = render_layer::modal
			});

			if (option_hovered && ctx.input.mouse_button_pressed(mouse_button::button_1)) {
				result.changed = true;
				result.new_index = i;
				state.open_dropdown_id.reset();
			}

			option_y -= row_height;
		}

		if (!list_rect.contains(ctx.input.mouse_position()) &&
			!header_rect.contains(ctx.input.mouse_position()) &&
			ctx.input.mouse_button_pressed(mouse_button::button_1)) {
			state.open_dropdown_id.reset();
		}
	}

	return result;
}

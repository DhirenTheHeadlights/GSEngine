export module gse.graphics:button_widget;

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
import :types;
import :font;
import :ids;
import :styles;
import :builder;
import :interaction;

export namespace gse::gui::draw {
	auto button(
		const draw_context& ctx,
		const std::string& name,
		id& hot_widget_id,
		id& active_widget_id,
		resource::handle<font> font = {}
	) -> bool;

	auto button_in_rect(
		const draw_context& ctx,
		std::string_view label,
		std::string_view key,
		const rectf& button_rect,
		id& hot_widget_id,
		id& active_widget_id,
		bool enabled = true,
		resource::handle<font> font = {}
	) -> bool;
}

export namespace gse::gui {
	struct button {
		using result = bool;
		struct params {
			std::string_view text;
			bool enabled = true;
			resource::handle<font> font{};
		};
		static auto draw(const draw_context& ctx, const params p, id& hot, id& active, id&) -> bool;
	};
}

auto gse::gui::draw::button(const draw_context& ctx, const std::string& name, id& hot_widget_id, id& active_widget_id, const resource::handle<font> font) -> bool {
	const auto fnt = font.valid() ? font : ctx.fonts.text;
	if (!ctx.current_menu) {
		return false;
	}

	const id widget_id = ids::make(name);

	const float widget_height = fnt->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
	const rectf content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });

	const rectf button_rect = rectf::from_position_size(
		{ content_rect.left(), ctx.layout_cursor.y() },
		{ content_rect.width(), widget_height }
	);

	const bool activated_in_rect = button_in_rect(ctx, name, name, button_rect, hot_widget_id, active_widget_id, true, fnt);
	ctx.layout_cursor.y() -= widget_height + ctx.style.padding;
	return activated_in_rect;
}

auto gse::gui::draw::button_in_rect(const draw_context& ctx, const std::string_view label, const std::string_view key, const rectf& button_rect, id& hot_widget_id, id& active_widget_id, const bool enabled, const resource::handle<font> font) -> bool {
	const auto fnt = font.valid() ? font : ctx.fonts.text;
	const id widget_id = ids::make(std::string(key.empty() ? label : key));

	const bool hovered = enabled && button_rect.contains(ctx.input.mouse_position()) && ctx.input_available();
	const bool released = ctx.input.mouse_button_released(mouse_button::button_1);

	interaction::mark_hot(hot_widget_id, widget_id, hovered);
	const bool activated = enabled && interaction::activate_on_click(active_widget_id, widget_id, hovered, ctx.mouse_pressed_for(button_rect), released);

	vec4f target_color = enabled ? ctx.style.color_button_background : ctx.style.color_widget_background;
	if (enabled && active_widget_id == widget_id) {
		target_color = ctx.style.color_widget_active;
	}
	else if (enabled && hot_widget_id == widget_id) {
		target_color = ctx.style.color_button_hovered;
	}

	ctx.queue_sprite({
		.rect = button_rect,
		.color = ctx.animated_color(widget_id, target_color),
		.texture = ctx.blank_texture,
		.corner_radius = ctx.style.corner_radius
	});

	const std::string text(label);
	const float text_width = fnt->width(text, ctx.style.font_size);
	const vec2f text_pos = { button_rect.center().x() - text_width / 2.f,
							 button_rect.center().y() + fnt->vertical_center_offset(ctx.style.font_size) };

	ctx.queue_text({
		.font = fnt,
		.text = text,
		.position = text_pos,
		.scale = ctx.style.font_size,
		.color = enabled ? ctx.style.color_text : ctx.style.color_text_disabled,
		.clip_rect = button_rect
	});

	return activated;
}

auto gse::gui::button::draw(const draw_context& ctx, const params p, id& hot, id& active, id&) -> bool {
	const auto fnt = p.font.valid() ? p.font : ctx.fonts.text;
	if (!ctx.current_menu) {
		return false;
	}

	const float widget_height = fnt->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
	const rectf content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });
	const rectf button_rect = rectf::from_position_size(
		{ content_rect.left(), ctx.layout_cursor.y() },
		{ content_rect.width(), widget_height }
	);

	const bool activated = draw::button_in_rect(ctx, p.text, p.text, button_rect, hot, active, p.enabled, fnt);
	ctx.layout_cursor.y() -= widget_height + ctx.style.padding;
	return activated;
}

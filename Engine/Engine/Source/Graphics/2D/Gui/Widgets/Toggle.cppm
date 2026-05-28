export module gse.graphics:toggle_widget;

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
import :types;
import :ids;
import :styles;
import :builder;

export namespace gse::gui {
	struct toggle {
		using result = bool;
		struct params {
			std::string_view name;
			bool& value;
		};
		static auto draw(
			const draw_context& ctx,
			const params& p,
			id& hot,
			id& active,
			id& focus
		) -> bool;
	};
}

auto gse::gui::toggle::draw(const draw_context& ctx, const params& p, id& hot, id& active, id&) -> bool {
	if (!ctx.current_menu) {
		return false;
	}

	const std::uint64_t name_key = stable_id(p.name);
	const id widget_id = ids::make_from_key(name_key);

	const float widget_height =
		ctx.font->line_height(ctx.style.font_size) + ctx.style.padding * ctx.style.widget_height_padding;
	const ui_rect content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });

	const ui_rect row_rect = ui_rect::from_position_size(
		{ content_rect.left(), ctx.layout_cursor.y() },
		{ content_rect.width(), widget_height }
	);

	const bool hovered = row_rect.contains(ctx.input.mouse_position()) && ctx.input_available();

	if (hovered) {
		hot = widget_id;
	}

	bool toggled = false;
	if (ctx.mouse_pressed_for(row_rect)) {
		p.value = !p.value;
		toggled = true;
	}

	const float label_width = content_rect.width() * 0.4f;

	const ui_rect label_rect = ui_rect::from_position_size(
		row_rect.top_left(),
		{ label_width, widget_height }
	);

	ctx.queue_text({
		.font = ctx.font,
		.text = std::string(p.name),
		.position = { label_rect.left(), label_rect.center().y() + ctx.font->vertical_center_offset(ctx.style.font_size) },
		.scale = ctx.style.font_size,
		.color = ctx.style.color_text,
		.clip_rect = label_rect
	});

	const float track_width = 40.f * (ctx.style.font_size / 16.f);
	const float track_height = 20.f * (ctx.style.font_size / 16.f);
	const float knob_padding = 2.f * (ctx.style.font_size / 16.f);

	const float track_x = row_rect.left() + label_width;
	const float track_y = row_rect.center().y() + track_height / 2.f;

	const ui_rect track_rect = ui_rect::from_position_size(
		{ track_x, track_y },
		{ track_width, track_height }
	);

	const vec4f track_target = p.value ? ctx.style.color_toggle_on : ctx.style.color_toggle_off;
	constexpr std::uint64_t track_suffix_hash = stable_id("##track");
	const id track_anim_id = ids::make_from_key(hash_combine(name_key, track_suffix_hash));

	ctx.queue_sprite({
		.rect = track_rect,
		.color = ctx.animated_color(track_anim_id, track_target),
		.texture = ctx.blank_texture,
		.corner_radius = track_height / 2.f
	});

	const float knob_size = track_height - knob_padding * 2.f;
	const float knob_x = p.value ? track_x + track_width - knob_size - knob_padding : track_x + knob_padding;
	const float knob_y = track_y - knob_padding;

	const ui_rect knob_rect = ui_rect::from_position_size(
		{ knob_x, knob_y },
		{ knob_size, knob_size }
	);

	const vec4f knob_target = hovered ? ctx.style.color_handle_hovered : ctx.style.color_handle;

	ctx.queue_sprite({
		.rect = knob_rect,
		.color = ctx.animated_color(widget_id, knob_target),
		.texture = ctx.blank_texture,
		.corner_radius = knob_size / 2.f
	});

	ctx.layout_cursor.y() -= widget_height + ctx.style.padding + ctx.style.item_spacing;

	return toggled;
}

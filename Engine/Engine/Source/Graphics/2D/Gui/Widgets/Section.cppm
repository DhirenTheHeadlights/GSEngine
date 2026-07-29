export module gse.graphics:section_widget;

import std;

import gse.assets;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import :types;
import :font;
import :styles;
import :layout_ops;
import :builder;

export namespace gse::gui {
	struct section {
		using result = void;
		struct params {
			std::string_view title;
			std::string_view subtitle = {};
			std::string_view action_icon = {};
			std::function<void()> on_action = {};
			std::string_view secondary_action_icon = {};
			std::function<void()> on_secondary_action = {};
			resource::handle<font> font{};
		};
		static auto draw(
			const draw_context& ctx,
			params p,
			id&,
			id&,
			id&
		) -> void;
	};
}

auto gse::gui::section::draw(const draw_context& ctx, params p, id&, id&, id&) -> void {
	const auto fnt = p.font.valid() ? p.font : ctx.fonts.text;
	if (!ctx.current_menu) {
		return;
	}

	const auto& sty = ctx.style;
	namespace lo = layout;

	const rectf content_rect = ctx.current_menu->rect.inset({ sty.padding, sty.padding });

	lo::skip(ctx, sty.section_spacing_above);

	const float header_size = sty.font_size * sty.section_header_size_mult;
	const rectf header_row = lo::reserve_row(ctx, fnt->line_height(header_size));
	const float title_top = header_row.top();
	const float bar_height = fnt->line_height(header_size);

	const rectf bar_rect = rectf::from_position_size(
		{ content_rect.left(), title_top },
		{ sty.accent_bar_width, bar_height }
	);

	ctx.queue_sprite({
		.rect = bar_rect,
		.color = sty.color_accent,
		.texture = ctx.blank_texture,
		.corner_radius = sty.accent_bar_width * 0.5f,
	});

	const float text_left = content_rect.left() + sty.accent_bar_width + sty.padding * 0.6f;

	ctx.queue_text({
		.font = fnt,
		.text = std::string(p.title),
		.position = { text_left, title_top },
		.scale = header_size,
		.color = sty.color_section_header,
		.clip_rect = content_rect,
	});

	const float action_height = bar_height;
	float action_cursor_x = content_rect.right();

	auto draw_action = [&](const std::string_view icon, const std::function<void()>& on_click) {
		if (!on_click || icon.empty()) {
			return;
		}
		const float icon_w = fnt->width(icon, sty.font_size) + sty.padding;
		const rectf action_rect = rectf::from_position_size(
			{ action_cursor_x - icon_w, title_top },
			{ icon_w, action_height }
		);

		const bool hovered = action_rect.contains(ctx.input.mouse_position()) && ctx.input_available();
		const vec4f bg = hovered ? sty.color_widget_hovered : sty.color_widget_background;

		ctx.queue_sprite({
			.rect = action_rect,
			.color = bg,
			.texture = ctx.blank_texture,
			.corner_radius = sty.corner_radius,
		});

		const float icon_text_w = fnt->width(icon, sty.font_size);
		ctx.queue_text({
			.font = fnt,
			.text = std::string(icon),
			.position = { action_rect.center().x() - icon_text_w * 0.5f, action_rect.center().y() + fnt->vertical_center_offset(sty.font_size) },
			.scale = sty.font_size,
			.color = hovered ? sty.color_text : sty.color_text_secondary,
			.clip_rect = action_rect,
		});

		if (ctx.mouse_pressed_for(action_rect)) {
			on_click();
		}

		action_cursor_x -= icon_w + sty.padding * 0.5f;
	};

	draw_action(p.action_icon, p.on_action);
	draw_action(p.secondary_action_icon, p.on_secondary_action);

	if (!p.subtitle.empty()) {
		lo::skip(ctx, sty.item_spacing);
		const rectf subtitle_row = lo::reserve_row(ctx, fnt->line_height(sty.font_size));
		ctx.queue_text({
			.font = fnt,
			.text = std::string(p.subtitle),
			.position = { text_left, subtitle_row.top() },
			.scale = sty.font_size,
			.color = sty.color_text_secondary,
			.clip_rect = content_rect,
		});
	}

	lo::skip(ctx, sty.section_spacing_below);
}

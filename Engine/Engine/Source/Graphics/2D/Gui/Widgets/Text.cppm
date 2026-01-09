export module gse.graphics:text_widget;

import std;

import :types;

export namespace gse::gui::draw {
	auto text(
		const draw_context& ctx,
		const std::string& name,
		const std::string& text
	) -> void;
}

auto gse::gui::draw::text(const draw_context& ctx, const std::string& name, const std::string& text) -> void {
	if (!ctx.current_menu) {
		return;
	}

	const float widget_height = ctx.font->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
	const ui_rect content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });
	const ui_rect row_rect = ui_rect::from_position_size(
		{ content_rect.left(), ctx.layout_cursor.y() },
		{ content_rect.width(), widget_height }
	);

	const float label_width = name.empty() ? 0.f : content_rect.width() * 0.4f;

	if (!name.empty()) {
		const unitless::vec2 label_pos = {
			row_rect.left(),
			row_rect.center().y() + ctx.style.font_size / 2.f
		};

		ctx.queue_text({
			.font = ctx.font,
			.text = name,
			.position = label_pos,
			.scale = ctx.style.font_size,
			.clip_rect = row_rect
		});
	}

	const unitless::vec2 value_pos = {
		row_rect.left() + label_width,
		row_rect.center().y() + ctx.style.font_size / 2.f
	};

	ctx.queue_text({
		.font = ctx.font,
		.text = text,
		.position = value_pos,
		.scale = ctx.style.font_size,
		.clip_rect = row_rect
	});

	ctx.layout_cursor.y() -= widget_height + ctx.style.padding;
}
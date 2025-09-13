export module gse.graphics:text_widget;

import std;

import :types;
import :text_renderer;

export namespace gse::gui::draw {
	auto text(
		const widget_context& context,
		const std::string& name,
		const std::string& text
	) -> void;
}

namespace gse::gui::draw {
	auto text_row(
		const widget_context& context,
		const std::string& name,
		const std::string& text
	) -> void;
}

auto gse::gui::draw::text(const widget_context& context, const std::string& name, const std::string& text) -> void {
	text_row(context, name, text);
}

auto gse::gui::draw::text_row(const widget_context& context, const std::string& name, const std::string& text) -> void {
	if (!context.current_menu) return;

	const float widget_height = context.font->line_height(context.style.font_size) + context.style.padding * 0.5f;
	const auto content_rect = context.current_menu->rect.inset({ context.style.padding, context.style.padding });

	const ui_rect row_rect = ui_rect::from_position_size(
		{ content_rect.left(), context.layout_cursor.y() },
		{ content_rect.width(), widget_height }
	);

	const float label_width = content_rect.width() * 0.4f;
	const unitless::vec2 label_pos = {
		row_rect.left(),
		row_rect.center().y() + context.style.font_size / 2.f
	};
	context.text_renderer.draw_text({
		.font = context.font,
		.text = name,
		.position = label_pos,
		.scale = context.style.font_size,
		.clip_rect = row_rect
	});

	const unitless::vec2 value_pos = {
		row_rect.left() + label_width,
		row_rect.center().y() + context.style.font_size / 2.f
	};
	context.text_renderer.draw_text({
		.font = context.font,
		.text = text,
		.position = value_pos,
		.scale = context.style.font_size,
		.clip_rect = row_rect
	});

	context.layout_cursor.y() -= widget_height + context.style.padding;
}
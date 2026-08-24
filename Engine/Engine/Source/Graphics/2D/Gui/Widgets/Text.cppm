export module gse.graphics:text_widget;

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
import :builder;
import :layout_ops;

export namespace gse::gui::draw {
	struct text_style {
		std::optional<vec4f> color;
		std::optional<float> size;
		layout::halign align = layout::halign::start;
		bool strong = false;
		resource::handle<font> font{};
	};

	auto text_in_rect(
		const draw_context& ctx,
		const rectf& rect,
		std::string_view content,
		const text_style& styling = {}
	) -> void;

	auto text(
		const draw_context& ctx,
		std::string_view name,
		std::string_view text,
		resource::handle<font> font = {}
	) -> void;
}

export namespace gse::gui {
	struct text {
		using result = void;
		struct params {
			std::string_view content;
			resource::handle<font> font{};
		};
		static auto draw(const draw_context& ctx, const params p, id&, id&, id&) -> void {
			draw::text(ctx, "", p.content, p.font);
		}
	};
}

auto gse::gui::draw::text_in_rect(const draw_context& ctx, const rectf& rect, const std::string_view content, const text_style& styling) -> void {
	if (content.empty()) {
		return;
	}

	const auto preferred = styling.strong && ctx.fonts.text_strong.valid() ? ctx.fonts.text_strong : ctx.fonts.text;
	const auto fnt = styling.font.valid() ? styling.font : preferred;
	const auto fnt_view = fnt.resolve();
	if (!fnt_view) {
		return;
	}

	const float size = styling.size.value_or(ctx.style.font_size);
	const float width = fnt_view->width(content, size);

	float left = rect.left();
	if (styling.align == layout::halign::center) {
		left = rect.center().x() - width * 0.5f;
	}
	else if (styling.align == layout::halign::end) {
		left = rect.right() - width;
	}

	ctx.queue_text({
		.font = fnt,
		.text = content,
		.position = { left, rect.center().y() + fnt_view->vertical_center_offset(size) },
		.scale = size,
		.color = styling.color.value_or(ctx.style.color_text),
		.clip_rect = rect,
	});
}

auto gse::gui::draw::text(const draw_context& ctx, const std::string_view name, const std::string_view text, const resource::handle<font> font) -> void {
	const auto fnt = font.valid() ? font : ctx.fonts.text;
	const auto fnt_view = fnt.resolve();
	if (!ctx.current_menu) {
		return;
	}

	const float widget_height = fnt_view->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
	const rectf content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });

	const rectf row_rect = rectf::from_position_size(
		{ content_rect.left(), ctx.layout_cursor.y() },
		{ content_rect.width(), widget_height }
	);

	const float label_width = name.empty() ? 0.f : content_rect.width() * 0.4f;

	const rectf label_rect = rectf::from_position_size(
		row_rect.top_left(),
		{ label_width, widget_height }
	);

	const rectf value_rect = rectf::from_position_size(
		{ row_rect.left() + label_width, row_rect.top() },
		{ row_rect.width() - label_width, widget_height }
	);

	const text_style styling{
		.font = fnt,
	};

	text_in_rect(ctx, label_rect, name, styling);
	text_in_rect(ctx, value_rect, text, styling);

	ctx.layout_cursor.y() -= widget_height + ctx.style.padding;
}

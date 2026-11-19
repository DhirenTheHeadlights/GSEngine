export module gse.graphics:text_widget;

import gse.assets;
import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.time;
import std;

import :font;
import :layout_ops;
import :styles;
import :surface_paint;
import :text_buffer;
import :types;
import :widget_skin;

export namespace gse::gui {
	struct text_style {
		std::optional<vec4f> color;
		std::optional<float> size;
		layout::halign align = layout::halign::start;
		bool strong = false;
		bool emphasis = false;
		resource::handle<font> font{};
	};
}

namespace gse::gui::draw {
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
			std::optional<rectf> rect{};
			text_style style{};
			resource::handle<font> font{};
		};
		static auto draw(const draw_context& ctx, const params p, id&, id&, id&) -> void {
			if (p.rect) {
				text_style styling = p.style;
				if (!styling.font.valid()) {
					styling.font = p.font;
				}
				draw::text_in_rect(ctx, *p.rect, p.content, styling);
				return;
			}
			draw::text(ctx, "", p.content, p.font);
		}
	};
}

auto gse::gui::draw::text_in_rect(const draw_context& ctx, const rectf& rect, const std::string_view content, const text_style& styling) -> void {
	if (content.empty()) {
		return;
	}

	const label_spec spec{
		.face = styling.strong ? text_face::text_strong : (styling.emphasis ? text_face::text_emphasis : text_face::text),
		.h = styling.align,
		.size_mult = styling.size.value_or(ctx.style.font_size) / ctx.style.font_size,
	};

	label(ctx, rect, content, spec, {
		.color = styling.color.value_or(ctx.style.color_text),
		.font = styling.font,
		.clip_rect = rect,
	});
}

auto gse::gui::draw::text(const draw_context& ctx, const std::string_view name, const std::string_view text, const resource::handle<font> font) -> void {
	const auto fnt = ctx.fonts.face_or(font);
	if (!ctx.current_menu || !fnt.valid()) {
		return;
	}

	const auto fnt_view = fnt.resolve();

	const float widget_height = fnt_view->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
	const rectf content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });

	const rectf row_rect = rectf::from_position_size(
		{ content_rect.left(), ctx.layout_cursor.y() },
		{ content_rect.width(), widget_height }
	);

	const float label_width = name.empty() ? 0.f : content_rect.width() * ctx.style.label_column_ratio;

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
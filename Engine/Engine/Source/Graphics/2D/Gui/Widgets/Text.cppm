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

export namespace gse::gui::draw {
	auto text(
		const draw_context& ctx,
		const std::string& name,
		const std::string& text,
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
			draw::text(ctx, "", std::string(p.content), p.font);
		}
	};
}

auto gse::gui::draw::text(const draw_context& ctx, const std::string& name, const std::string& text, const resource::handle<font> font) -> void {
	const auto fnt = font.valid() ? font : ctx.fonts.text;
	if (!ctx.current_menu) {
		return;
	}

	const float widget_height = fnt->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
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

	if (!name.empty()) {
		ctx.queue_text({
			.font = fnt,
			.text = name,
			.position = { label_rect.left(), label_rect.center().y() + fnt->vertical_center_offset(ctx.style.font_size) },
			.scale = ctx.style.font_size,
			.color = ctx.style.color_text,
			.clip_rect = label_rect
		});
	}

	ctx.queue_text({
		.font = fnt,
		.text = text,
		.position = { value_rect.left(), value_rect.center().y() + fnt->vertical_center_offset(ctx.style.font_size) },
		.scale = ctx.style.font_size,
		.color = ctx.style.color_text,
		.clip_rect = value_rect
	});

	ctx.layout_cursor.y() -= widget_height + ctx.style.padding;
}

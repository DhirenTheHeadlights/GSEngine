export module gse.graphics:selectable_widget;

import gse.assets;
import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.gpu;
import gse.math;
import gse.os;
import gse.time;
import std;

import :font;
import :ids;
import :interaction;
import :layout_ops;
import :marquee_widget;
import :styles;
import :surface_paint;
import :types;
import :widget_skin;

export namespace gse::gui {
	enum class selectable_align : std::uint8_t {
		center,
		left
	};

	struct selectable_info {
		std::string_view text;
		std::string_view detail;
		std::string_view key;
		bool selected = false;
		selectable_align align = selectable_align::center;
		std::optional<vec4f> accent = std::nullopt;
		float detail_column = 0.f;
		resource::handle<font> font{};
	};

	struct selectable {
		using result = bool;
		using params = selectable_info;

		static auto draw(
			const draw_context& ctx,
			const params& p,
			id& hot,
			id& active,
			id& focus
		) -> bool;
	};
}

namespace gse::gui::draw {
	auto selectable(
		const draw_context& ctx,
		const selectable_info& info,
		id& hot_widget_id,
		id& active_widget_id
	) -> bool;
}

auto gse::gui::selectable::draw(const draw_context& ctx, const params& p, id& hot, id& active, id&) -> bool {
	return draw::selectable(ctx, p, hot, active);
}

auto gse::gui::draw::selectable(const draw_context& ctx, const selectable_info& info, id& hot_widget_id, id& active_widget_id) -> bool {
	const auto fnt = ctx.fonts.face_or(info.font);
	const auto fnt_view = fnt.resolve();
	if (!ctx.current_menu) {
		return false;
	}

	const id widget_id = ids::make(info.key.empty() ? info.text : info.key);

	const float widget_height = fnt_view->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
	const rectf content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });

	const rectf row_rect = rectf::from_position_size(
		{ content_rect.left(), ctx.layout_cursor.y() },
		{ content_rect.width(), widget_height }
	);

	const item_skin& skin = ctx.style.list_item;

	const interaction::press row = interaction::press_in_rect(ctx, hot_widget_id, active_widget_id, widget_id, {
		.rect = row_rect,
		.selected = info.selected,
	});
	const bool activated = row.activated;

	draw::fill_surface(ctx, row_rect, row.select(skin.surfaces), {
		.anim = widget_id,
	});

	const float pad = ctx.style.padding;
	float text_left = row_rect.left() + pad;

	if (info.accent) {
		const float swatch_w = ctx.style.accent_bar_width;
		surface swatch = skin.accent_bar;
		swatch.fill = *info.accent;
		draw::fill_surface(
			ctx,
			rectf::from_position_size(
				{ text_left, row_rect.center().y() + ctx.style.font_size * 0.5f },
				{ swatch_w, ctx.style.font_size }
			),
			swatch,
			{}
		);
		text_left += swatch_w + pad;
	}

	const float split = info.detail.empty()
		? row_rect.right() - pad
		: row_rect.left() + (info.detail_column > 0.f ? info.detail_column : row_rect.width() * 0.5f);

	const rectf text_area = rectf::from_position_size(
		{ text_left, row_rect.top() },
		{ std::max(0.f, split - pad - text_left), row_rect.height() }
	);

	label_spec text_spec = skin.label;
	if (info.align == selectable_align::center) {
		text_spec.h = layout::halign::center;
	}

	draw::label(ctx, info.align == selectable_align::center ? row_rect : text_area, info.text, text_spec, {
		.color = row.select(skin.label_colors),
		.font = info.font,
		.clip_rect = rectf::from_position_size(
			{ row_rect.left(), row_rect.top() },
			{ std::max(0.f, split - pad - row_rect.left()), row_rect.height() }
		),
	});

	if (!info.detail.empty()) {
		draw::fill_surface(
			ctx,
			rectf::from_position_size(
				{ split, row_rect.center().y() + ctx.style.font_size * 0.5f },
				{ std::max(1.f, ctx.style.scale_factor), ctx.style.font_size }
			),
			skin.divider,
			{}
		);

		marquee_text(ctx, {
			.text = info.detail,
			.area = rectf::from_position_size(
				{ split + pad, row_rect.top() },
				{ std::max(0.f, row_rect.right() - pad - split - pad), row_rect.height() }
			),
			.color = skin.detail_color,
			.scrolling = row.hovered,
			.font = info.font,
		});
	}

	ctx.layout_cursor.y() -= widget_height + ctx.style.padding;

	return activated;
}
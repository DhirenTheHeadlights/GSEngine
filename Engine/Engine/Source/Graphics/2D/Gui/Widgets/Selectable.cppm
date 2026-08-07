export module gse.graphics:selectable_widget;

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
import :marquee_widget;

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

export namespace gse::gui::draw {
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
	const auto fnt = info.font.valid() ? info.font : ctx.fonts.text;
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

	const bool hovered = ctx.hovers(row_rect);
	const bool released = ctx.mouse_released();

	interaction::mark_hot(hot_widget_id, widget_id, hovered);
	const bool activated = interaction::activate_on_click(active_widget_id, widget_id, hovered, ctx.mouse_pressed_for(row_rect), released);

	vec4f target_color = ctx.style.color_widget_background;
	if (info.selected) {
		target_color = ctx.style.color_widget_selected;
	}
	else if (active_widget_id == widget_id) {
		target_color = ctx.style.color_widget_active;
	}
	else if (hot_widget_id == widget_id) {
		target_color = ctx.style.color_widget_hovered;
	}

	ctx.queue_sprite({
		.rect = row_rect,
		.color = ctx.animated_color(widget_id, target_color),
		.texture = ctx.blank_texture,
		.corner_radius = ctx.style.corner_radius
	});

	const float pad = ctx.style.padding;
	const float baseline = row_rect.center().y() + fnt_view->vertical_center_offset(ctx.style.font_size);
	float text_left = row_rect.left() + pad;

	if (info.accent) {
		const float swatch_w = ctx.style.accent_bar_width;
		ctx.queue_sprite({
			.rect = rectf::from_position_size(
				{ text_left, row_rect.center().y() + ctx.style.font_size * 0.5f },
				{ swatch_w, ctx.style.font_size }
			),
			.color = *info.accent,
			.texture = ctx.blank_texture,
		});
		text_left += swatch_w + pad;
	}

	const float split = info.detail.empty()
		? row_rect.right() - pad
		: row_rect.left() + (info.detail_column > 0.f ? info.detail_column : row_rect.width() * 0.5f);

	const vec2f text_position = info.align == selectable_align::center
		? vec2f{ row_rect.center().x() - fnt_view->width(info.text, ctx.style.font_size) * 0.5f, baseline }
		: vec2f{ text_left, baseline };

	ctx.queue_text({
		.font = fnt,
		.text = info.text,
		.position = text_position,
		.scale = ctx.style.font_size,
		.color = ctx.style.color_text,
		.clip_rect = rectf::from_position_size(
			{ row_rect.left(), row_rect.top() },
			{ std::max(0.f, split - pad - row_rect.left()), row_rect.height() }
		)
	});

	if (!info.detail.empty()) {
		ctx.queue_sprite({
			.rect = rectf::from_position_size(
				{ split, row_rect.center().y() + ctx.style.font_size * 0.5f },
				{ std::max(1.f, ctx.style.scale_factor), ctx.style.font_size }
			),
			.color = ctx.style.color_text_secondary,
			.texture = ctx.blank_texture,
		});

		marquee_text(ctx, {
			.text = info.detail,
			.area = rectf::from_position_size(
				{ split + pad, row_rect.top() },
				{ std::max(0.f, row_rect.right() - pad - split - pad), row_rect.height() }
			),
			.color = ctx.style.color_text_secondary,
			.scrolling = hot_widget_id == widget_id,
			.font = fnt,
		});
	}

	ctx.layout_cursor.y() -= widget_height + ctx.style.padding;

	return activated;
}

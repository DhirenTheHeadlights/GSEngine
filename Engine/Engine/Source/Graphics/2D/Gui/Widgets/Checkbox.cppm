export module gse.graphics:checkbox_widget;

import std;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.meta;
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
import :render_layer;
import :symbols;
import :texture;
import :ui_renderer;

export namespace gse::gui {
	struct checkbox_paint {
		vec4f color_frame = { 1.f, 1.f, 1.f, 1.f };
		vec4f color_mark = { 1.f, 1.f, 1.f, 1.f };
		float extent = 0.f;
		float thickness = 0.f;
		render_layer layer = render_layer::content;
		std::uint32_t z_order = 0;
		std::optional<rectf> clip_rect = std::nullopt;
	};

	auto draw_checkbox(
		std::vector<renderer::sprite_command>& out,
		resource::handle<texture> blank,
		const rectf& box,
		bool checked,
		const checkbox_paint& p = {}
	) -> void;

	auto draw_checkbox(
		const draw_context& ctx,
		const rectf& box,
		bool checked,
		const checkbox_paint& p = {}
	) -> void;

	[[nodiscard]] auto checkbox_extent(
		const style& sty
	) -> float;

	struct checkbox {
		using result = bool;
		struct params {
			std::string_view name;
			bool& value;
			bool enabled = true;
			resource::handle<font> font{};
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

namespace gse::gui {
	auto checkbox_stroke_paint(
		vec4f color,
		const checkbox_paint& p
	) -> symbol::paint;
}

auto gse::gui::checkbox_stroke_paint(const vec4f color, const checkbox_paint& p) -> symbol::paint {
	return {
		.color = color,
		.thickness = p.thickness,
		.extent = p.extent,
		.layer = p.layer,
		.z_order = p.z_order,
		.clip_rect = p.clip_rect,
	};
}

auto gse::gui::draw_checkbox(std::vector<renderer::sprite_command>& out, const resource::handle<texture> blank, const rectf& box, const bool checked, const checkbox_paint& p) -> void {
	symbol::draw(out, blank, symbol::square(), box, checkbox_stroke_paint(p.color_frame, p));
	if (checked) {
		symbol::draw(out, blank, symbol::check(), box, checkbox_stroke_paint(p.color_mark, p));
	}
}

auto gse::gui::draw_checkbox(const draw_context& ctx, const rectf& box, const bool checked, const checkbox_paint& p) -> void {
	symbol::draw(ctx, symbol::square(), box, checkbox_stroke_paint(p.color_frame, p));
	if (checked) {
		symbol::draw(ctx, symbol::check(), box, checkbox_stroke_paint(p.color_mark, p));
	}
}

auto gse::gui::checkbox_extent(const style& sty) -> float {
	return sty.font_size;
}

auto gse::gui::checkbox::draw(const draw_context& ctx, const params& p, id& hot, id& active, id&) -> bool {
	const auto fnt = p.font.valid() ? p.font : ctx.fonts.text;
	const auto fnt_view = fnt.resolve();
	if (!ctx.current_menu) {
		return false;
	}

	const id widget_id = ids::make_from_key(stable_id(p.name));

	const float widget_height =
		fnt_view->line_height(ctx.style.font_size) + ctx.style.padding * ctx.style.widget_height_padding;
	const rectf content_rect = ctx.current_menu->rect.inset({ ctx.style.padding, ctx.style.padding });

	const rectf row_rect = rectf::from_position_size(
		{ content_rect.left(), ctx.layout_cursor.y() },
		{ content_rect.width(), widget_height }
	);

	const interaction::press pressed = interaction::press_in_rect(ctx, hot, active, widget_id, row_rect, p.enabled);
	if (pressed.activated) {
		p.value = !p.value;
	}

	const float label_width = content_rect.width() * 0.4f;

	const rectf label_rect = rectf::from_position_size(
		row_rect.top_left(),
		{ label_width, widget_height }
	);

	const vec4f text_color = p.enabled ? ctx.style.color_text : ctx.style.color_text_disabled;

	ctx.queue_text({
		.font = fnt,
		.text = p.name,
		.position = { label_rect.left(), label_rect.center().y() + fnt_view->vertical_center_offset(ctx.style.font_size) },
		.scale = ctx.style.font_size,
		.color = text_color,
		.clip_rect = label_rect
	});

	const float extent = checkbox_extent(ctx.style);
	const rectf box_rect = rectf::from_position_size(
		{ row_rect.left() + label_width, row_rect.center().y() + extent * 0.5f },
		{ extent, extent }
	);

	draw_checkbox(ctx, box_rect, p.value, {
		.color_frame = pressed.color({
			.idle = ctx.style.color_border,
			.hot = ctx.style.color_handle_hovered,
			.active = ctx.style.color_accent,
			.disabled = ctx.style.color_text_disabled,
		}),
		.color_mark = p.enabled ? ctx.style.color_accent : ctx.style.color_text_disabled,
		.extent = extent,
	});

	ctx.layout_cursor.y() -= widget_height + ctx.style.padding + ctx.style.item_spacing;

	return pressed.activated;
}

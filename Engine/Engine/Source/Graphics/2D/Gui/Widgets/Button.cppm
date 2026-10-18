export module gse.graphics:button_widget;

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

import :builder;
import :font;
import :ids;
import :interaction;
import :styles;
import :surface_paint;
import :symbols;
import :types;
import :widget_skin;

namespace gse::gui::draw {
	struct button_params {
		rectf rect;
		std::string_view label;
		std::span<const symbol::stroke> glyph;
		std::string_view key;
		id widget_id{};
		button_role role = button_role::standard;
		bool enabled = true;
		resource::handle<font> font{};
	};

	auto button_in_rect(
		const draw_context& ctx,
		const button_params& params,
		id& hot_widget_id,
		id& active_widget_id
	) -> bool;
}

namespace gse::gui {
	struct icon_button_visual {
		rectf rect;
		std::span<const symbol::stroke> glyph;
		interaction::press state{};
		id anim{};
		std::optional<rectf> clip_rect = std::nullopt;
	};

	auto draw_icon_button(
		const draw_context& ctx,
		const icon_button_skin& skin,
		const icon_button_visual& visual
	) -> void;
}

export namespace gse::gui {
	struct button {
		using result = bool;
		struct params {
			std::string_view text;
			std::optional<rectf> rect{};
			std::string_view key{};
			std::span<const symbol::stroke> glyph{};
			bool enabled = true;
			button_role role = button_role::standard;
			resource::handle<font> font{};
		};
		static auto draw(const draw_context& ctx, const params p, id& hot, id& active, id&) -> bool;
	};
}

auto gse::gui::draw::button_in_rect(const draw_context& ctx, const button_params& params, id& hot_widget_id, id& active_widget_id) -> bool {
	const button_skin skin = skin_for(ctx.style, params.role);
	const id widget_id = params.widget_id.exists()
		? params.widget_id
		: ids::make(params.key.empty() ? params.label : params.key);
	const auto btn = interaction::press_in_rect(ctx, hot_widget_id, active_widget_id, widget_id, {
		.rect = params.rect,
		.enabled = params.enabled,
	});

	const rectf content = fill_surface(ctx, params.rect, btn.select(skin.surfaces), {
		.anim = btn.widget,
	});

	label(ctx, content, params.label, skin.label, {
		.color = btn.select(skin.label_colors),
		.font = params.font,
		.glyph = params.glyph,
		.glyph_gap = skin.glyph_gap,
		.clip_rect = params.rect,
	});

	return btn.activated;
}

auto gse::gui::draw_icon_button(const draw_context& ctx, const icon_button_skin& skin, const icon_button_visual& visual) -> void {
	surface chip = visual.state.select(skin.surfaces);
	chip.radii = uniform_radii(std::min(visual.rect.width(), visual.rect.height()) * 0.5f);

	draw::fill_surface(ctx, visual.rect, chip, {
		.clip_rect = visual.clip_rect,
		.anim = visual.anim,
	});

	symbol::draw(ctx, visual.glyph, visual.rect, {
		.color = visual.state.select(skin.icon_colors),
		.extent = std::min(ctx.style.icon_extent, std::min(visual.rect.width(), visual.rect.height())),
		.clip_rect = visual.clip_rect,
	});
}

auto gse::gui::button::draw(const draw_context& ctx, const params p, id& hot, id& active, id&) -> bool {
	if (!p.rect && !ctx.current_menu) {
		return false;
	}

	const rectf rect = p.rect ? *p.rect : ctx.next_row(ctx.fonts.face_or(p.font));

	return draw::button_in_rect(ctx, {
		.rect = rect,
		.label = p.text,
		.glyph = p.glyph,
		.key = p.key.empty() ? p.text : p.key,
		.role = p.role,
		.enabled = p.enabled,
		.font = p.font,
	}, hot, active);
}

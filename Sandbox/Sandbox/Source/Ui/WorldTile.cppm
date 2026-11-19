export module sandbox:world_tile;

import std;
import gse;

export namespace sandbox::ui {
	struct world_tile {
		using result = bool;

		struct params {
			gse::rect_t<gse::vec2f> rect;
			std::string_view label;
			std::string_view key;
			gse::gui::symbol_glyph diagram = nullptr;
			bool selected = false;
			bool live = false;
		};

		static auto draw(
			const gse::gui::draw_context& ctx,
			params p,
			gse::id& hot,
			gse::id& active,
			gse::id&
		) -> bool;
	};
}

namespace sandbox::ui {
	constexpr float tile_label_band = 0.26f;
	constexpr float tile_diagram_inset = 0.18f;
	constexpr float diagram_stroke_weight = 2.f;
}

auto sandbox::ui::world_tile::draw(const gse::gui::draw_context& ctx, const params p, gse::id& hot, gse::id& active, gse::id&) -> bool {
	namespace gui = gse::gui;
	namespace lo = gse::gui::layout;

	const auto& sty = ctx.style;
	const gui::item_skin skin = sty.nav_item;

	const auto press = gui::interaction::press_in_rect(ctx, hot, active, gui::ids::make(p.key), {
		.rect = p.rect,
		.selected = p.selected,
	});

	gui::draw::fill_surface(ctx, p.rect, press.select(skin.surfaces), {
		.anim = press.widget,
	});

	gui::draw::fill_surface(
		ctx,
		gse::rect_t<gse::vec2f>::from_position_size(
			{ p.rect.left(), p.rect.bottom() + sty.separator_thickness },
			{ p.rect.width(), sty.separator_thickness }
		),
		sty.separator_line,
		{
			.clip_rect = p.rect,
		}
	);

	gui::draw::fill_surface(
		ctx,
		gse::rect_t<gse::vec2f>::from_position_size(
			{ p.rect.right() - sty.separator_thickness, p.rect.top() },
			{ sty.separator_thickness, p.rect.height() }
		),
		sty.separator_line,
		{
			.clip_rect = p.rect,
		}
	);

	if (p.selected) {
		gui::draw::fill_surface(
			ctx,
			gse::rect_t<gse::vec2f>::from_position_size(
				p.rect.top_left(),
				{ gui::accent_bar_extent(sty), p.rect.height() }
			),
			skin.accent_bar,
			{
				.clip_rect = p.rect,
			}
		);
	}

	if (p.live) {
		gui::draw::fill_surface(
			ctx,
			gse::rect_t<gse::vec2f>::from_position_size(
				p.rect.top_left(),
				{ p.rect.width(), gui::accent_bar_extent(sty) }
			),
			skin.accent_bar,
			{
				.clip_rect = p.rect,
			}
		);
	}

	const auto [art, caption] = lo::split_vertical<2>(
		p.rect,
		{
			lo::size_spec::flex(),
			lo::size_spec::ratio(tile_label_band),
		}
	);

	const gse::vec4f ink = press.select(skin.label_colors);

	if (p.diagram) {
		const float inset = std::min(art.width(), art.height()) * tile_diagram_inset;
		gui::symbol::draw(ctx, p.diagram(), art.inset({ inset, inset }), {
			.color = ink,
			.thickness = sty.separator_thickness * diagram_stroke_weight,
			.clip_rect = p.rect,
		});
	}

	gui::label_spec caption_spec = sty.screen.entry_path;
	caption_spec.h = lo::halign::center;

	gui::draw::label(ctx, caption, p.label, caption_spec, {
		.color = ink,
		.clip_rect = p.rect,
	});

	return press.activated;
}

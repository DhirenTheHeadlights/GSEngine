export module sandbox:card_backdrop;

import std;
import gse;

export namespace sandbox::ui {
	[[nodiscard]] auto card_rect(
		const gse::gui::style& sty,
		gse::vec2f viewport_size
	) -> gse::rect_t<gse::vec2f>;

	[[nodiscard]] auto rail_width(
		const gse::gui::style& sty
	) -> float;

	auto draw_card_backdrop(
		gse::gui::draw_context& ctx,
		gse::vec2f viewport_size,
		float scrim_scale = 1.f
	) -> void;
}

namespace sandbox::ui {
	constexpr float card_width_ratio = 0.78f;
	constexpr float card_height_ratio = 0.74f;
	constexpr float rail_scale = 2.4f;
}

auto sandbox::ui::card_rect(const gse::gui::style& sty, const gse::vec2f viewport_size) -> gse::rect_t<gse::vec2f> {
	return gse::gui::layout::fit_card(
		viewport_size,
		sty.card_min_size,
		{ viewport_size.x() * card_width_ratio, viewport_size.y() * card_height_ratio },
		sty.card_margin
	);
}

auto sandbox::ui::rail_width(const gse::gui::style& sty) -> float {
	return sty.accent_bar_width * rail_scale;
}

auto sandbox::ui::draw_card_backdrop(gse::gui::draw_context& ctx, const gse::vec2f viewport_size, const float scrim_scale) -> void {
	const gse::vec4f scrim = ctx.style.modal.scrim.fill;

	ctx.sprites.push_back({
		.rect = gse::rect_t<gse::vec2f>::from_position_size(
			{ 0.f, viewport_size.y() },
			viewport_size
		),
		.color = { gse::vec3f(scrim), scrim.w() * scrim_scale },
		.texture = ctx.blank_texture,
		.layer = gse::render_layer::popup,
	});

	const gse::rect_t<gse::vec2f> card = card_rect(ctx.style, viewport_size);

	gse::gui::draw::fill_surface(ctx, card, ctx.style.menu.floating, {});

	gse::gui::draw::fill_surface(
		ctx,
		gse::rect_t<gse::vec2f>::from_position_size(
			card.top_left(),
			{ rail_width(ctx.style), card.height() }
		),
		ctx.style.menu.accent_bar,
		{}
	);
}

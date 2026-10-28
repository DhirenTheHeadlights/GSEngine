export module gse.graphics:surface_paint;

import gse.assets;
import gse.core;
import gse.math;
import std;

import :font;
import :layout_ops;
import :render_layer;
import :symbols;
import :types;
import :widget_skin;

export namespace gse::gui {
	struct surface_paint_info {
		std::optional<rectf> clip_rect = std::nullopt;
		render_layer layer = render_layer::content;
		std::uint32_t z_order = 0;
		std::optional<id> anim = std::nullopt;
	};

	struct label_paint_info {
		vec4f color{};
		resource::handle<font> font{};
		std::span<const symbol::stroke> glyph{};
		float glyph_gap = 0.f;
		std::optional<rectf> clip_rect = std::nullopt;
		render_layer layer = render_layer::content;
	};
}

export namespace gse::gui::draw {
	auto fill_surface(
		const draw_context& ctx,
		const rectf& rect,
		const surface& skin,
		const surface_paint_info& paint
	) -> rectf;

	auto label(
		const draw_context& ctx,
		const rectf& rect,
		std::string_view text,
		const label_spec& spec,
		const label_paint_info& paint
	) -> void;
}

namespace gse::gui::draw {
	[[nodiscard]] auto apply_case(
		std::string_view text,
		text_case transform,
		std::string& storage
	) -> std::string_view;
}

auto gse::gui::draw::fill_surface(const draw_context& ctx, const rectf& rect, const surface& skin, const surface_paint_info& paint) -> rectf {
	if (rect.width() <= 0.f || rect.height() <= 0.f) {
		return rect;
	}

	const vec4f fill = paint.anim ? ctx.animated_color(*paint.anim, skin.fill) : skin.fill;

	ctx.queue_sprite({
		.rect = rect,
		.color = fill,
		.texture = ctx.blank_texture,
		.clip_rect = paint.clip_rect,
		.layer = paint.layer,
		.z_order = paint.z_order,
		.corner_radius = skin.radii,
	});

	const float width = std::min(skin.stroke_width, std::min(rect.width(), rect.height()) * 0.5f);
	if (width <= 0.f) {
		return rect;
	}

	ctx.queue_sprite({
		.rect = rect,
		.color = skin.stroke,
		.texture = ctx.blank_texture,
		.clip_rect = paint.clip_rect,
		.layer = paint.layer,
		.z_order = paint.z_order,
		.corner_radius = skin.radii,
		.shape = renderer::sprite_shape::ring,
		.stroke_thickness = width * 0.5f,
	});

	return rect.inset({ width, width });
}

auto gse::gui::draw::apply_case(const std::string_view text, const text_case transform, std::string& storage) -> std::string_view {
	if (transform == text_case::as_written) {
		return text;
	}

	storage.assign(text);
	for (char& c : storage) {
		c = transform == text_case::upper
			? static_cast<char>(std::toupper(static_cast<unsigned char>(c)))
			: static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return storage;
}

auto gse::gui::draw::label(const draw_context& ctx, const rectf& rect, const std::string_view text, const label_spec& spec, const label_paint_info& paint) -> void {
	const auto fnt = ctx.fonts.face_or(paint.font, spec.face);
	if (!fnt.valid()) {
		return;
	}

	const auto fnt_view = fnt.resolve();
	std::string case_storage;
	const std::string_view shown = apply_case(text, spec.transform, case_storage);

	const float scale = ctx.style.font_size * spec.size_mult;
	const float glyph_extent = paint.glyph.empty() ? 0.f : std::min(ctx.style.icon_extent, std::min(rect.width(), rect.height()));
	const float glyph_span = paint.glyph.empty() ? 0.f : glyph_extent + paint.glyph_gap;
	const float text_width = shown.empty() ? 0.f : fnt_view->width(shown, scale);

	const rectf content = layout::align_in(
		rect,
		{ std::min(glyph_span + text_width, rect.width()), rect.height() },
		spec.h,
		layout::valign::center
	);

	if (!paint.glyph.empty()) {
		const rectf glyph_rect = rectf::from_position_size(
			{ content.left(), content.top() },
			{ glyph_extent, content.height() }
		);
		symbol::draw(ctx, paint.glyph, glyph_rect, {
			.color = paint.color,
			.extent = glyph_extent,
			.layer = paint.layer,
			.clip_rect = paint.clip_rect.value_or(rect),
		});
	}

	if (shown.empty()) {
		return;
	}

	ctx.queue_text({
		.font = fnt,
		.text = shown,
		.position = { content.left() + glyph_span, content.center().y() + fnt_view->vertical_center_offset(scale) },
		.scale = scale,
		.color = paint.color,
		.clip_rect = paint.clip_rect.value_or(rect),
		.layer = paint.layer,
	});
}

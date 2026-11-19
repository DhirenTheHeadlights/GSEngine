export module gse.ide.app:brand;

import gse;
import std;

import :brand_shape;

export namespace gse::ide::brand {
	auto draw(
		const gui::draw_context& ctx,
		const rectf& box
	) -> void;

	auto window_icon(
		const gui::style& style
	) -> window_icon_request;
}

namespace gse::ide::brand {
	struct palette {
		vec4f accent;
		vec4f foreground;
		vec4f secondary;
		vec4f background;
	};

	auto colors(
		const gui::style& style
	) -> palette;

	template <std::size_t extent>
	auto icon(
		const palette& colors
	) -> std::array<std::uint32_t, extent * extent>;

	struct painted_blade {
		brand_shape::blade shape;
		vec4f color;
	};

	auto blades(
		const palette& paints
	) -> std::array<painted_blade, 3>;

	auto sample(
		std::span<const painted_blade> shapes,
		vec4f background,
		vec2f point
	) -> vec4f;

	auto packed_bgra(
		vec4f color
	) -> std::uint32_t;
}

auto gse::ide::brand::colors(const gui::style& style) -> palette {
	return {
		.accent = style.color_accent,
		.foreground = style.color_text,
		.secondary = style.color_icon,
		.background = style.menu.title_bar.fill,
	};
}

auto gse::ide::brand::blades(const palette& paints) -> std::array<painted_blade, 3> {
	return { {
		{
			.shape = brand_shape::accent(),
			.color = paints.accent,
		},
		{
			.shape = brand_shape::foreground(),
			.color = paints.foreground,
		},
		{
			.shape = brand_shape::secondary(),
			.color = paints.secondary,
		},
	} };
}

auto gse::ide::brand::draw(const gui::draw_context& ctx, const rectf& box) -> void {
	const float extent = std::min(box.width(), box.height());
	const vec2f origin{ box.center().x() - extent * 0.5f, box.center().y() + extent * 0.5f };
	for (const auto& blade : blades(colors(ctx.style))) {
		for (const auto& triangle : blade.shape.triangles) {
			std::array<vec2f, 4> corners;
			for (std::size_t i = 0; i < corners.size(); ++i) {
				const vec2f point = blade.shape.outline[triangle[std::min(i, std::size_t{ 2 })]];
				corners[i] = origin + point * vec2f{ extent * 0.01f, -extent * 0.01f };
			}
			ctx.queue_sprite({
				.rect = box,
				.color = blade.color,
				.texture = ctx.blank_texture,
				.corners = corners,
			});
		}
	}
}

auto gse::ide::brand::sample(const std::span<const painted_blade> shapes, const vec4f background, const vec2f point) -> vec4f {
	const vec2f corner = max(abs(point - vec2f{ 50, 50 }) - vec2f{ 33, 33 }, vec2f{});
	if (dot(corner, corner) > 225.f) {
		return {};
	}
	for (const auto& blade : shapes) {
		const auto outline = blade.shape.outline;
		bool inside = false;
		for (std::size_t i = 0, j = outline.size() - 1; i < outline.size(); j = i++) {
			const vec2f a = outline[i];
			const vec2f b = outline[j];
			if ((a.y() > point.y()) != (b.y() > point.y()) && point.x() < (b.x() - a.x()) * (point.y() - a.y()) / (b.y() - a.y()) + a.x()) {
				inside = !inside;
			}
		}
		if (inside) {
			return { blade.color.x(), blade.color.y(), blade.color.z(), 1.f };
		}
	}
	return { background.x(), background.y(), background.z(), 1.f };
}

auto gse::ide::brand::packed_bgra(const vec4f color) -> std::uint32_t {
	const auto byte = [](const float value) {
		return static_cast<std::uint32_t>(std::clamp(value * 255.f + 0.5f, 0.f, 255.f));
	};
	return byte(color.z()) | (byte(color.y()) << 8) | (byte(color.x()) << 16) | (byte(color.w()) << 24);
}

template <std::size_t extent>
auto gse::ide::brand::icon(const palette& colors) -> std::array<std::uint32_t, extent * extent> {
	const auto shapes = blades(colors);
	std::array<std::uint32_t, extent * extent> pixels{};
	for (std::size_t y = 0; y < extent; ++y) {
		for (std::size_t x = 0; x < extent; ++x) {
			vec4f sum{};
			for (int sy = 0; sy < 4; ++sy) {
				for (int sx = 0; sx < 4; ++sx) {
					const vec2f point{ x + (sx + 0.5f) * 0.25f, y + (sy + 0.5f) * 0.25f };
					sum += sample(shapes, colors.background, point * (100.f / extent));
				}
			}
			const float coverage = sum.w();
			const vec4f color = coverage > 0.f
				? vec4f{ sum.x() / coverage, sum.y() / coverage, sum.z() / coverage, coverage / 16.f }
				: vec4f{};
			pixels[y * extent + x] = packed_bgra(color);
		}
	}
	return pixels;
}

auto gse::ide::brand::window_icon(const gui::style& style) -> window_icon_request {
	const palette paints = colors(style);
	return {
		.small = icon<32>(paints),
		.large = icon<64>(paints),
	};
}

export module gse.graphics:symbols;

import std;

import gse.math;
import gse.assets;

import :ui_renderer;
import :texture;
import :render_layer;
import :types;

export namespace gse::gui::symbol {
	struct paint {
		vec4f color = { 1.f, 1.f, 1.f, 1.f };
		float thickness = 0.f;
		float extent = 0.f;
		render_layer layer = render_layer::content;
		std::uint32_t z_order = 0;
		std::optional<rect_t<vec2f>> clip_rect = std::nullopt;
	};

	auto close() -> std::span<const stroke>;
	auto minimize() -> std::span<const stroke>;
	auto maximize() -> std::span<const stroke>;
	auto chevron_up() -> std::span<const stroke>;
	auto chevron_down() -> std::span<const stroke>;
	auto chevron_left() -> std::span<const stroke>;
	auto chevron_right() -> std::span<const stroke>;
	auto gear() -> std::span<const stroke>;
	auto file() -> std::span<const stroke>;
	auto folder() -> std::span<const stroke>;
	auto project() -> std::span<const stroke>;
	auto trash() -> std::span<const stroke>;
	auto hammer() -> std::span<const stroke>;
	auto play() -> std::span<const stroke>;

	auto draw(
		const draw_context& ctx,
		std::span<const stroke> strokes,
		const rect_t<vec2f>& box,
		const paint& p = {}
	) -> void;

	auto draw(
		std::vector<renderer::sprite_command>& out,
		resource::handle<texture> blank,
		std::span<const stroke> strokes,
		const rect_t<vec2f>& box,
		const paint& p = {}
	) -> void;
}

namespace gse::gui::symbol {
	constexpr float default_weight = 0.13f;
	constexpr float min_half_thickness = 0.75f;

	auto to_command(
		const stroke& st,
		vec2f center,
		float extent,
		float half_thickness,
		const paint& p
	) -> renderer::sprite_command;

	auto snap_to_pixel_center(
		vec2f point
	) -> vec2f;

	auto glyph_extent(
		const rect_t<vec2f>& box,
		const paint& p
	) -> float;
}

auto gse::gui::symbol::close() -> std::span<const stroke> {
	static constexpr std::array<stroke, 2> data{
		stroke{ { 0.22f, 0.22f }, { 0.78f, 0.78f } },
		stroke{ { 0.22f, 0.78f }, { 0.78f, 0.22f } },
	};
	return data;
}

auto gse::gui::symbol::minimize() -> std::span<const stroke> {
	static constexpr std::array<stroke, 1> data{
		stroke{ { 0.24f, 0.30f }, { 0.76f, 0.30f } },
	};
	return data;
}

auto gse::gui::symbol::maximize() -> std::span<const stroke> {
	static constexpr std::array<stroke, 4> data{
		stroke{ { 0.26f, 0.26f }, { 0.74f, 0.26f } },
		stroke{ { 0.74f, 0.26f }, { 0.74f, 0.74f } },
		stroke{ { 0.74f, 0.74f }, { 0.26f, 0.74f } },
		stroke{ { 0.26f, 0.74f }, { 0.26f, 0.26f } },
	};
	return data;
}

auto gse::gui::symbol::chevron_up() -> std::span<const stroke> {
	static constexpr std::array<stroke, 2> data{
		stroke{ { 0.25f, 0.40f }, { 0.50f, 0.62f } },
		stroke{ { 0.50f, 0.62f }, { 0.75f, 0.40f } },
	};
	return data;
}

auto gse::gui::symbol::chevron_down() -> std::span<const stroke> {
	static constexpr std::array<stroke, 2> data{
		stroke{ { 0.25f, 0.60f }, { 0.50f, 0.38f } },
		stroke{ { 0.50f, 0.38f }, { 0.75f, 0.60f } },
	};
	return data;
}

auto gse::gui::symbol::chevron_left() -> std::span<const stroke> {
	static constexpr std::array<stroke, 2> data{
		stroke{ { 0.60f, 0.72f }, { 0.38f, 0.50f } },
		stroke{ { 0.38f, 0.50f }, { 0.60f, 0.28f } },
	};
	return data;
}

auto gse::gui::symbol::chevron_right() -> std::span<const stroke> {
	static constexpr std::array<stroke, 2> data{
		stroke{ { 0.40f, 0.72f }, { 0.62f, 0.50f } },
		stroke{ { 0.62f, 0.50f }, { 0.40f, 0.28f } },
	};
	return data;
}

auto gse::gui::symbol::gear() -> std::span<const stroke> {
	static const std::array<stroke, 24> data = [] {
		std::array<stroke, 24> s{};
		constexpr int teeth = 8;
		constexpr float pi = std::numbers::pi_v<float>;
		constexpr float r_hub = 0.10f;
		constexpr float r_body = 0.22f;
		constexpr float r_tooth = 0.33f;
		const vec2f c{ 0.5f, 0.5f };
		std::size_t k = 0;
		for (int i = 0; i < teeth; ++i) {
			const float a0 = static_cast<float>(i) / teeth * 2.f * pi;
			const float a1 = static_cast<float>(i + 1) / teeth * 2.f * pi;
			const vec2f dir0{ std::cos(a0), std::sin(a0) };
			const vec2f dir1{ std::cos(a1), std::sin(a1) };
			s[k++] = { c + dir0 * r_hub, c + dir1 * r_hub };
			s[k++] = { c + dir0 * r_body, c + dir1 * r_body };
			s[k++] = { c + dir0 * r_body, c + dir0 * r_tooth };
		}
		return s;
	}();
	return data;
}

auto gse::gui::symbol::file() -> std::span<const stroke> {
	static constexpr std::array<stroke, 7> data{
		stroke{ { 0.28f, 0.14f }, { 0.28f, 0.86f } },
		stroke{ { 0.28f, 0.86f }, { 0.58f, 0.86f } },
		stroke{ { 0.58f, 0.86f }, { 0.72f, 0.72f } },
		stroke{ { 0.72f, 0.72f }, { 0.72f, 0.14f } },
		stroke{ { 0.72f, 0.14f }, { 0.28f, 0.14f } },
		stroke{ { 0.58f, 0.86f }, { 0.58f, 0.72f } },
		stroke{ { 0.58f, 0.72f }, { 0.72f, 0.72f } },
	};
	return data;
}

auto gse::gui::symbol::folder() -> std::span<const stroke> {
	static constexpr std::array<stroke, 6> data{
		stroke{ { 0.18f, 0.30f }, { 0.18f, 0.70f } },
		stroke{ { 0.18f, 0.70f }, { 0.42f, 0.70f } },
		stroke{ { 0.42f, 0.70f }, { 0.50f, 0.62f } },
		stroke{ { 0.50f, 0.62f }, { 0.82f, 0.62f } },
		stroke{ { 0.82f, 0.62f }, { 0.82f, 0.30f } },
		stroke{ { 0.82f, 0.30f }, { 0.18f, 0.30f } },
	};
	return data;
}

auto gse::gui::symbol::project() -> std::span<const stroke> {
	static constexpr std::array<stroke, 6> data{
		stroke{ { 0.20f, 0.24f }, { 0.80f, 0.24f } },
		stroke{ { 0.80f, 0.24f }, { 0.80f, 0.68f } },
		stroke{ { 0.80f, 0.68f }, { 0.20f, 0.68f } },
		stroke{ { 0.20f, 0.68f }, { 0.20f, 0.24f } },
		stroke{ { 0.20f, 0.55f }, { 0.80f, 0.55f } },
		stroke{ { 0.50f, 0.68f }, { 0.50f, 0.55f } },
	};
	return data;
}

auto gse::gui::symbol::trash() -> std::span<const stroke> {
	static constexpr std::array<stroke, 9> data{
		stroke{ { 0.24f, 0.70f }, { 0.76f, 0.70f } },
		stroke{ { 0.42f, 0.70f }, { 0.44f, 0.78f } },
		stroke{ { 0.44f, 0.78f }, { 0.56f, 0.78f } },
		stroke{ { 0.56f, 0.78f }, { 0.58f, 0.70f } },
		stroke{ { 0.30f, 0.66f }, { 0.35f, 0.24f } },
		stroke{ { 0.70f, 0.66f }, { 0.65f, 0.24f } },
		stroke{ { 0.35f, 0.24f }, { 0.65f, 0.24f } },
		stroke{ { 0.44f, 0.60f }, { 0.46f, 0.32f } },
		stroke{ { 0.56f, 0.60f }, { 0.54f, 0.32f } },
	};
	return data;
}

auto gse::gui::symbol::hammer() -> std::span<const stroke> {
	static constexpr std::array<stroke, 6> data{
		stroke{ { 0.24f, 0.74f }, { 0.76f, 0.74f } },
		stroke{ { 0.24f, 0.74f }, { 0.24f, 0.62f } },
		stroke{ { 0.76f, 0.74f }, { 0.76f, 0.62f } },
		stroke{ { 0.24f, 0.62f }, { 0.76f, 0.62f } },
		stroke{ { 0.50f, 0.62f }, { 0.50f, 0.22f } },
		stroke{ { 0.42f, 0.22f }, { 0.58f, 0.22f } },
	};
	return data;
}

auto gse::gui::symbol::play() -> std::span<const stroke> {
	static constexpr std::array<stroke, 3> data{
		stroke{ { 0.36f, 0.74f }, { 0.72f, 0.50f } },
		stroke{ { 0.72f, 0.50f }, { 0.36f, 0.26f } },
		stroke{ { 0.36f, 0.26f }, { 0.36f, 0.74f } },
	};
	return data;
}

auto gse::gui::symbol::to_command(const stroke& st, const vec2f center, const float extent, const float half_thickness, const paint& p) -> renderer::sprite_command {
	const vec2f a{ center.x() + (st.from.x() - 0.5f) * extent, center.y() + (st.from.y() - 0.5f) * extent };
	const vec2f b{ center.x() + (st.to.x() - 0.5f) * extent, center.y() + (st.to.y() - 0.5f) * extent };
	const segment_t<vec2f> seg{ a, b };
	const vec2f mid = seg.midpoint();
	const vec2f half_size{ seg.length() * 0.5f + half_thickness, half_thickness };
	return {
		.rect = rect_t<vec2f>({ .min = mid - half_size, .max = mid + half_size }),
		.color = p.color,
		.clip_rect = p.clip_rect,
		.rotation = seg.angle(),
		.layer = p.layer,
		.z_order = p.z_order,
		.corner_radius = half_thickness,
	};
}

auto gse::gui::symbol::snap_to_pixel_center(const vec2f point) -> vec2f {
	return { std::floor(point.x()) + 0.5f, std::floor(point.y()) + 0.5f };
}

auto gse::gui::symbol::glyph_extent(const rect_t<vec2f>& box, const paint& p) -> float {
	return p.extent > 0.f ? p.extent : std::min(box.width(), box.height());
}

auto gse::gui::symbol::draw(const draw_context& ctx, std::span<const stroke> strokes, const rect_t<vec2f>& box, const paint& p) -> void {
	const float extent = glyph_extent(box, p);
	if (extent <= 0.f) {
		return;
	}
	const vec2f center = snap_to_pixel_center(box.center());
	const float half_thickness = std::max(min_half_thickness, (p.thickness > 0.f ? p.thickness : extent * default_weight) * 0.5f);
	for (const stroke& st : strokes) {
		renderer::sprite_command cmd = to_command(st, center, extent, half_thickness, p);
		cmd.texture = ctx.blank_texture;
		ctx.queue_sprite(cmd);
	}
}

auto gse::gui::symbol::draw(std::vector<renderer::sprite_command>& out, resource::handle<texture> blank, std::span<const stroke> strokes, const rect_t<vec2f>& box, const paint& p) -> void {
	const float extent = glyph_extent(box, p);
	if (extent <= 0.f) {
		return;
	}
	const vec2f center = snap_to_pixel_center(box.center());
	const float half_thickness = std::max(min_half_thickness, (p.thickness > 0.f ? p.thickness : extent * default_weight) * 0.5f);
	for (const stroke& st : strokes) {
		renderer::sprite_command cmd = to_command(st, center, extent, half_thickness, p);
		cmd.texture = blank;
		out.push_back(cmd);
	}
}

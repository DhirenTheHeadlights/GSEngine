export module gse.graphics:symbols;

import std;

import gse.math;
import gse.assets;
import gse.time;

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

	constexpr auto segment(
		vec2f from,
		vec2f to
	) -> stroke;

	constexpr auto arc(
		vec2f center,
		float radius,
		angle begin,
		angle sweep
	) -> stroke;

	auto close() -> std::span<const stroke>;
	auto minimize() -> std::span<const stroke>;
	auto maximize() -> std::span<const stroke>;
	auto chevron_up() -> std::span<const stroke>;
	auto chevron_down() -> std::span<const stroke>;
	auto chevron_left() -> std::span<const stroke>;
	auto chevron_right() -> std::span<const stroke>;
	auto gear() -> std::span<const stroke>;
	auto info() -> std::span<const stroke>;
	auto plus() -> std::span<const stroke>;
	auto file() -> std::span<const stroke>;
	auto folder() -> std::span<const stroke>;
	auto project() -> std::span<const stroke>;
	auto trash() -> std::span<const stroke>;
	auto hammer() -> std::span<const stroke>;
	auto play() -> std::span<const stroke>;
	auto stop() -> std::span<const stroke>;

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

	auto spinner_rotation() -> angle;

	auto spinner(
		const draw_context& ctx,
		const rect_t<vec2f>& box,
		angle rotation,
		const paint& p = {}
	) -> void;

	auto snap_to_pixel_center(
		vec2f point
	) -> vec2f;
}

namespace gse::gui::symbol {
	constexpr float default_weight = 0.13f;
	constexpr float min_half_thickness = 0.75f;

	auto to_segment_command(
		const stroke& st,
		vec2f center,
		float extent,
		float half_thickness,
		const paint& p
	) -> renderer::sprite_command;

	auto to_arc_command(
		const stroke& st,
		vec2f center,
		float extent,
		float half_thickness,
		const paint& p
	) -> renderer::sprite_command;

	auto to_command(
		const stroke& st,
		vec2f center,
		float extent,
		float half_thickness,
		const paint& p
	) -> renderer::sprite_command;

	auto glyph_extent(
		const rect_t<vec2f>& box,
		const paint& p
	) -> float;
}

constexpr auto gse::gui::symbol::segment(const vec2f from, const vec2f to) -> stroke {
	return {
		.kind = shape::segment,
		.from = from,
		.to = to,
	};
}

constexpr auto gse::gui::symbol::arc(const vec2f center, const float radius, const angle begin, const angle sweep) -> stroke {
	return {
		.kind = shape::arc,
		.center = center,
		.radius = radius,
		.begin = begin,
		.sweep = sweep,
	};
}

auto gse::gui::symbol::close() -> std::span<const stroke> {
	static constexpr std::array<stroke, 2> data{
		segment({ 0.22f, 0.22f }, { 0.78f, 0.78f }),
		segment({ 0.22f, 0.78f }, { 0.78f, 0.22f }),
	};
	return data;
}

auto gse::gui::symbol::minimize() -> std::span<const stroke> {
	static constexpr std::array<stroke, 1> data{
		segment({ 0.24f, 0.30f }, { 0.76f, 0.30f }),
	};
	return data;
}

auto gse::gui::symbol::maximize() -> std::span<const stroke> {
	static constexpr std::array<stroke, 4> data{
		segment({ 0.26f, 0.26f }, { 0.74f, 0.26f }),
		segment({ 0.74f, 0.26f }, { 0.74f, 0.74f }),
		segment({ 0.74f, 0.74f }, { 0.26f, 0.74f }),
		segment({ 0.26f, 0.74f }, { 0.26f, 0.26f }),
	};
	return data;
}

auto gse::gui::symbol::chevron_up() -> std::span<const stroke> {
	static constexpr std::array<stroke, 2> data{
		segment({ 0.25f, 0.40f }, { 0.50f, 0.62f }),
		segment({ 0.50f, 0.62f }, { 0.75f, 0.40f }),
	};
	return data;
}

auto gse::gui::symbol::chevron_down() -> std::span<const stroke> {
	static constexpr std::array<stroke, 2> data{
		segment({ 0.25f, 0.60f }, { 0.50f, 0.38f }),
		segment({ 0.50f, 0.38f }, { 0.75f, 0.60f }),
	};
	return data;
}

auto gse::gui::symbol::chevron_left() -> std::span<const stroke> {
	static constexpr std::array<stroke, 2> data{
		segment({ 0.60f, 0.72f }, { 0.38f, 0.50f }),
		segment({ 0.38f, 0.50f }, { 0.60f, 0.28f }),
	};
	return data;
}

auto gse::gui::symbol::chevron_right() -> std::span<const stroke> {
	static constexpr std::array<stroke, 2> data{
		segment({ 0.40f, 0.72f }, { 0.62f, 0.50f }),
		segment({ 0.62f, 0.50f }, { 0.40f, 0.28f }),
	};
	return data;
}

auto gse::gui::symbol::gear() -> std::span<const stroke> {
	static constexpr std::array<stroke, 10> data{
		arc({ 0.5f, 0.5f }, 0.10f, degrees(0.f), degrees(360.f)),
		arc({ 0.5f, 0.5f }, 0.22f, degrees(0.f), degrees(360.f)),
		segment({ 0.7200f, 0.5000f }, { 0.8300f, 0.5000f }),
		segment({ 0.6556f, 0.6556f }, { 0.7333f, 0.7333f }),
		segment({ 0.5000f, 0.7200f }, { 0.5000f, 0.8300f }),
		segment({ 0.3444f, 0.6556f }, { 0.2667f, 0.7333f }),
		segment({ 0.2800f, 0.5000f }, { 0.1700f, 0.5000f }),
		segment({ 0.3444f, 0.3444f }, { 0.2667f, 0.2667f }),
		segment({ 0.5000f, 0.2800f }, { 0.5000f, 0.1700f }),
		segment({ 0.6556f, 0.3444f }, { 0.7333f, 0.2667f }),
	};
	return data;
}

auto gse::gui::symbol::info() -> std::span<const stroke> {
	static constexpr std::array<stroke, 3> data{
		arc({ 0.5f, 0.5f }, 0.38f, degrees(0.f), degrees(360.f)),
		segment({ 0.50f, 0.28f }, { 0.50f, 0.56f }),
		segment({ 0.50f, 0.66f }, { 0.50f, 0.70f }),
	};
	return data;
}

auto gse::gui::symbol::plus() -> std::span<const stroke> {
	static constexpr std::array<stroke, 2> data{
		segment({ 0.50f, 0.24f }, { 0.50f, 0.76f }),
		segment({ 0.24f, 0.50f }, { 0.76f, 0.50f }),
	};
	return data;
}

auto gse::gui::symbol::file() -> std::span<const stroke> {
	static constexpr std::array<stroke, 7> data{
		segment({ 0.28f, 0.14f }, { 0.28f, 0.86f }),
		segment({ 0.28f, 0.86f }, { 0.58f, 0.86f }),
		segment({ 0.58f, 0.86f }, { 0.72f, 0.72f }),
		segment({ 0.72f, 0.72f }, { 0.72f, 0.14f }),
		segment({ 0.72f, 0.14f }, { 0.28f, 0.14f }),
		segment({ 0.58f, 0.86f }, { 0.58f, 0.72f }),
		segment({ 0.58f, 0.72f }, { 0.72f, 0.72f }),
	};
	return data;
}

auto gse::gui::symbol::folder() -> std::span<const stroke> {
	static constexpr std::array<stroke, 6> data{
		segment({ 0.18f, 0.30f }, { 0.18f, 0.70f }),
		segment({ 0.18f, 0.70f }, { 0.42f, 0.70f }),
		segment({ 0.42f, 0.70f }, { 0.50f, 0.62f }),
		segment({ 0.50f, 0.62f }, { 0.82f, 0.62f }),
		segment({ 0.82f, 0.62f }, { 0.82f, 0.30f }),
		segment({ 0.82f, 0.30f }, { 0.18f, 0.30f }),
	};
	return data;
}

auto gse::gui::symbol::project() -> std::span<const stroke> {
	static constexpr std::array<stroke, 6> data{
		segment({ 0.20f, 0.24f }, { 0.80f, 0.24f }),
		segment({ 0.80f, 0.24f }, { 0.80f, 0.68f }),
		segment({ 0.80f, 0.68f }, { 0.20f, 0.68f }),
		segment({ 0.20f, 0.68f }, { 0.20f, 0.24f }),
		segment({ 0.20f, 0.55f }, { 0.80f, 0.55f }),
		segment({ 0.50f, 0.68f }, { 0.50f, 0.55f }),
	};
	return data;
}

auto gse::gui::symbol::trash() -> std::span<const stroke> {
	static constexpr std::array<stroke, 9> data{
		segment({ 0.24f, 0.70f }, { 0.76f, 0.70f }),
		segment({ 0.42f, 0.70f }, { 0.44f, 0.78f }),
		segment({ 0.44f, 0.78f }, { 0.56f, 0.78f }),
		segment({ 0.56f, 0.78f }, { 0.58f, 0.70f }),
		segment({ 0.30f, 0.66f }, { 0.35f, 0.24f }),
		segment({ 0.70f, 0.66f }, { 0.65f, 0.24f }),
		segment({ 0.35f, 0.24f }, { 0.65f, 0.24f }),
		segment({ 0.44f, 0.60f }, { 0.46f, 0.32f }),
		segment({ 0.56f, 0.60f }, { 0.54f, 0.32f }),
	};
	return data;
}

auto gse::gui::symbol::hammer() -> std::span<const stroke> {
	static constexpr std::array<stroke, 6> data{
		segment({ 0.24f, 0.74f }, { 0.76f, 0.74f }),
		segment({ 0.24f, 0.74f }, { 0.24f, 0.62f }),
		segment({ 0.76f, 0.74f }, { 0.76f, 0.62f }),
		segment({ 0.24f, 0.62f }, { 0.76f, 0.62f }),
		segment({ 0.50f, 0.62f }, { 0.50f, 0.22f }),
		segment({ 0.42f, 0.22f }, { 0.58f, 0.22f }),
	};
	return data;
}

auto gse::gui::symbol::play() -> std::span<const stroke> {
	static constexpr std::array<stroke, 3> data{
		segment({ 0.36f, 0.74f }, { 0.72f, 0.50f }),
		segment({ 0.72f, 0.50f }, { 0.36f, 0.26f }),
		segment({ 0.36f, 0.26f }, { 0.36f, 0.74f }),
	};
	return data;
}

auto gse::gui::symbol::stop() -> std::span<const stroke> {
	static constexpr std::array<stroke, 4> data{
		segment({ 0.32f, 0.32f }, { 0.68f, 0.32f }),
		segment({ 0.68f, 0.32f }, { 0.68f, 0.68f }),
		segment({ 0.68f, 0.68f }, { 0.32f, 0.68f }),
		segment({ 0.32f, 0.68f }, { 0.32f, 0.32f }),
	};
	return data;
}

auto gse::gui::symbol::to_segment_command(const stroke& st, const vec2f center, const float extent, const float half_thickness, const paint& p) -> renderer::sprite_command {
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

auto gse::gui::symbol::to_arc_command(const stroke& st, const vec2f center, const float extent, const float half_thickness, const paint& p) -> renderer::sprite_command {
	const angle quarter_turn = degrees(90.f);
	const vec2f pivot{ center.x() + (st.center.x() - 0.5f) * extent, center.y() + (st.center.y() - 0.5f) * extent };
	const float radius = st.radius * extent;
	const float reach = radius + half_thickness;
	const vec2f half_size{ reach, reach };
	const angle half_sweep = st.sweep * 0.5f;
	return {
		.rect = rect_t<vec2f>({ .min = pivot - half_size, .max = pivot + half_size }),
		.color = p.color,
		.clip_rect = p.clip_rect,
		.rotation = st.begin + half_sweep - quarter_turn,
		.layer = p.layer,
		.z_order = p.z_order,
		.shape = renderer::sprite_shape::arc,
		.arc_radius = radius,
		.arc_half_sweep = half_sweep,
		.arc_thickness = half_thickness,
	};
}

auto gse::gui::symbol::to_command(const stroke& st, const vec2f center, const float extent, const float half_thickness, const paint& p) -> renderer::sprite_command {
	return st.kind == shape::arc
		? to_arc_command(st, center, extent, half_thickness, p)
		: to_segment_command(st, center, extent, half_thickness, p);
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

auto gse::gui::symbol::spinner_rotation() -> angle {
	constexpr angular_velocity spin_rate = radians_per_second(9.6f);
	constexpr angle full_rotation = degrees(360.f);
	return fmod(spin_rate * system_clock::now(), full_rotation);
}

auto gse::gui::symbol::spinner(const draw_context& ctx, const rect_t<vec2f>& box, const angle rotation, const paint& p) -> void {
	constexpr float radius = 0.34f;
	const angle sweep = degrees(270.f);
	const std::array<stroke, 1> strokes{ arc({ 0.5f, 0.5f }, radius, rotation, sweep) };
	draw(ctx, strokes, box, p);
}

export module gse.graphics:graph_canvas_widget;

import std;

import gse.os;
import gse.assets;
import gse.core;
import gse.math;

import :font;
import :ui_renderer;
import :styles;
import :types;
import :ids;
import :interaction;

export namespace gse::gui {
	struct graph_canvas {
		struct node {
			std::uint64_t key = 0;
			std::string_view label;
			rectf rect;
			vec4f color = { 0.20f, 0.22f, 0.26f, 1.f };
		};

		struct edge {
			vec2f from;
			vec2f to;
			vec4f color = { 0.45f, 0.48f, 0.55f, 1.f };
		};

		struct result {
			std::optional<std::uint64_t> clicked;
			std::optional<std::uint64_t> hovered;
			bool background_clicked = false;
		};

		struct params {
			rectf area;
			std::span<const node> nodes;
			std::span<const edge> edges;
			vec4f background = { 0.09f, 0.10f, 0.13f, 1.f };
			float label_scale = 1.f;
			resource::handle<font> font{};
		};

		static auto draw(const draw_context& ctx, params p, id& hot, id& active, id& focus) -> result;
	};
}

auto gse::gui::graph_canvas::draw(const draw_context& ctx, params p, id& hot, id& active, id&) -> result {
	const auto fnt = p.font.valid() ? p.font : ctx.fonts.text;
	const rectf clip = p.area;

	ctx.queue_sprite({
		.rect = p.area,
		.color = p.background,
		.texture = ctx.blank_texture,
		.clip_rect = clip,
	});

	const auto draw_seg = [&](const vec2f a, const vec2f b, const vec4f color, const float thick) {
		const segment_t<vec2f> s{ a, b };
		const vec2f mid = s.midpoint();
		const vec2f hs{ s.length() * 0.5f + thick, thick };
		ctx.queue_sprite({
			.rect = rectf({
				.min = mid - hs,
				.max = mid + hs,
			}),
			.color = color,
			.texture = ctx.blank_texture,
			.clip_rect = clip,
			.rotation = s.angle(),
			.corner_radius = thick,
		});
	};

	for (const edge& e : p.edges) {
		draw_seg(e.from, e.to, e.color, 1.0f);
		const vec2f delta = e.to - e.from;
		const float len = segment_t<vec2f>{ e.from, e.to }.length();
		if (len > 3.f) {
			const vec2f u = delta * (1.f / len);
			const vec2f perp{ -u.y(), u.x() };
			const float barb = 8.f;
			const vec2f base = e.to - u * barb;
			draw_seg(e.to, base + perp * (barb * 0.55f), e.color, 1.2f);
			draw_seg(e.to, base - perp * (barb * 0.55f), e.color, 1.2f);
		}
	}

	result out;
	const bool released = ctx.input.mouse_button_released(mouse_button::button_1);

	for (const node& n : p.nodes) {
		const id nid = ids::make(std::format("graph_node##{}", n.key));
		const bool hovered = n.rect.contains(ctx.input.mouse_position()) && ctx.input_available();
		interaction::mark_hot(hot, nid, hovered);
		if (hovered) {
			out.hovered = n.key;
		}
		if (interaction::activate_on_click(active, nid, hovered, ctx.mouse_pressed_for(n.rect), released)) {
			out.clicked = n.key;
		}

		vec4f fill = n.color;
		if (active == nid) {
			fill = { fill.x() + 0.14f, fill.y() + 0.14f, fill.z() + 0.14f, fill.w() };
		}
		else if (hot == nid) {
			fill = { fill.x() + 0.07f, fill.y() + 0.07f, fill.z() + 0.07f, fill.w() };
		}

		ctx.queue_sprite({
			.rect = n.rect,
			.color = fill,
			.texture = ctx.blank_texture,
			.clip_rect = clip,
			.corner_radius = ctx.style.corner_radius,
		});

		const float label_font = ctx.style.font_size * p.label_scale;
		const rectf label_clip = rectf({
			.min = { std::max(n.rect.left(), clip.left()), std::max(n.rect.bottom(), clip.bottom()) },
			.max = { std::min(n.rect.right(), clip.right()), std::min(n.rect.top(), clip.top()) },
		});
		ctx.queue_text({
			.font = fnt,
			.text = n.label,
			.position = { n.rect.left() + 6.f * p.label_scale, n.rect.center().y() + fnt->vertical_center_offset(label_font) },
			.scale = label_font,
			.color = ctx.style.color_text,
			.clip_rect = label_clip,
		});
	}

	if (released && p.area.contains(ctx.input.mouse_position()) && ctx.input_available() && !out.clicked) {
		out.background_clicked = true;
	}

	return out;
}

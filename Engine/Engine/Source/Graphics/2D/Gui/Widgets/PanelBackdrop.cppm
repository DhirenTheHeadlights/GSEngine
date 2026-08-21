export module gse.graphics:panel_backdrop;

import std;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;

import :types;
import :styles;
import :builder;
import :render_layer;

export namespace gse::gui {
	struct panel_accent {
		panel_edge edge = panel_edge::left;
		float width = 0.f;
		vec4f color{};
	};

	struct panel_backdrop_params {
		std::optional<rectf> rect{};
		vec4f background{};
		std::optional<panel_accent> accent{};
		std::optional<rectf> clip{};
		render_layer layer = render_layer::content;
	};
}

export namespace gse::gui::draw {
	auto panel_backdrop(
		const draw_context& ctx,
		const panel_backdrop_params& p
	) -> rectf;
}

auto gse::gui::draw::panel_backdrop(const draw_context& ctx, const panel_backdrop_params& p) -> rectf {
	const rectf body = p.rect.value_or(ctx.clip_stack.empty() ? rectf{} : ctx.clip_stack.back());
	if (body.width() <= 0.f || body.height() <= 0.f) {
		return body;
	}

	ctx.queue_sprite({
		.rect = body,
		.color = p.background,
		.texture = ctx.blank_texture,
		.clip_rect = p.clip,
		.layer = p.layer,
	});

	if (!p.accent) {
		return body;
	}

	const panel_accent& accent = *p.accent;
	const float width = std::min(accent.width, std::min(body.width(), body.height()));

	rectf bar = body;
	rectf content = body;

	switch (accent.edge) {
		case panel_edge::left:
			bar = rectf::from_position_size({ body.left(), body.top() }, { width, body.height() });
			content = rectf(rectf::min_max_params{ .min = { body.left() + width, body.bottom() }, .max = body.max() });
			break;
		case panel_edge::right:
			bar = rectf::from_position_size({ body.right() - width, body.top() }, { width, body.height() });
			content = rectf(rectf::min_max_params{ .min = body.min(), .max = { body.right() - width, body.top() } });
			break;
		case panel_edge::top:
			bar = rectf::from_position_size({ body.left(), body.top() }, { body.width(), width });
			content = rectf(rectf::min_max_params{ .min = body.min(), .max = { body.right(), body.top() - width } });
			break;
		case panel_edge::bottom:
			bar = rectf::from_position_size({ body.left(), body.bottom() + width }, { body.width(), width });
			content = rectf(rectf::min_max_params{ .min = { body.left(), body.bottom() + width }, .max = body.max() });
			break;
	}

	ctx.queue_sprite({
		.rect = bar,
		.color = accent.color,
		.texture = ctx.blank_texture,
		.clip_rect = p.clip,
		.layer = p.layer,
	});

	return content;
}

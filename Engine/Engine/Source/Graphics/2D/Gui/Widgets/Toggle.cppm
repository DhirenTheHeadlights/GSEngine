export module gse.graphics:toggle_widget;

import gse.assets;
import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.gpu;
import gse.math;
import gse.meta;
import gse.os;
import gse.time;
import std;

import :font;
import :ids;
import :layout_ops;
import :styles;
import :surface_paint;
import :types;
import :widget_skin;

export namespace gse::gui {
	struct toggle {
		using result = bool;
		struct params {
			std::string_view name;
			bool& value;
			std::optional<rectf> rect{};
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
	auto toggle_in_rect(
		const draw_context& ctx,
		const rectf& row_rect,
		const toggle::params& p,
		id& hot
	) -> bool;
}

auto gse::gui::toggle::draw(const draw_context& ctx, const params& p, id& hot, id&, id&) -> bool {
	if (p.rect) {
		return toggle_in_rect(ctx, *p.rect, p, hot);
	}

	if (!ctx.current_menu) {
		return false;
	}

	return toggle_in_rect(ctx, ctx.next_row(ctx.fonts.face_or(p.font)), p, hot);
}

auto gse::gui::toggle_in_rect(const draw_context& ctx, const rectf& row_rect, const toggle::params& p, id& hot) -> bool {
	const toggle_skin& skin = ctx.style.toggle;

	const std::uint64_t name_key = stable_id(p.name);
	const id widget_id = ids::make_from_key(name_key);

	const bool hovered = ctx.hovers(row_rect);

	if (hovered) {
		hot = widget_id;
	}

	bool toggled = false;
	if (ctx.clicked_in_rect(row_rect)) {
		p.value = !p.value;
		toggled = true;
	}

	const auto [label_rect, control_rect] = layout::split_horizontal<2>(
		row_rect,
		{
			layout::size_spec::ratio(ctx.style.label_column_ratio),
			layout::size_spec::flex()
		}
	);

	draw::label(ctx, label_rect, p.name, skin.label, {
		.color = skin.label_color,
		.font = p.font,
		.clip_rect = label_rect,
	});

	const rectf track_rect = layout::align_in(
		control_rect,
		{ skin.track_width, skin.track_height },
		layout::halign::start,
		layout::valign::center
	);

	const id track_anim_id = ids::make_from_key(hash_of(name_key, "##track"));

	draw::fill_surface(ctx, track_rect, p.value ? skin.track_on : skin.track_off, {
		.anim = track_anim_id,
	});

	const float knob_size = std::max(0.f, skin.track_height - skin.knob_inset * 2.f);
	const float knob_left = p.value
		? track_rect.right() - knob_size - skin.knob_inset
		: track_rect.left() + skin.knob_inset;

	surface knob = skin.knob;
	if (hovered) {
		knob.fill = skin.knob_hovered;
	}

	draw::fill_surface(ctx, rectf::from_position_size({ knob_left, track_rect.top() - skin.knob_inset }, { knob_size, knob_size }), knob, {
		.anim = widget_id,
	});

	return toggled;
}

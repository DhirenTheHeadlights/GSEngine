export module gse.graphics:text_area_widget;

import std;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;

import :types;
import :ids;
import :styles;
import :builder;
import :text_buffer;
import :font;

export namespace gse::gui {
	struct text_area_state {
		buffer_position caret;
		buffer_position anchor;
		vec2f scroll{ 0.f, 0.f };
		double blink_ms = 0.0;
		bool blink_on = true;
		bool rpt_active = false;
		double rpt_next_ms = 0.0;
	};

	struct text_area {
		using result = void;

		struct params {
			text_buffer& buffer;
			text_area_state& state;
			std::span<const text_span> spans{};
			std::optional<ui_rect> rect{};
			bool read_only = false;
			bool show_line_numbers = false;
		};

		static auto draw(
			const draw_context& ctx,
			const params& p,
			id& hot,
			id& active,
			id& focus
		) -> void;
	};
}

export namespace gse::gui::draw {
	auto text_area_in_rect(
		const draw_context& ctx,
		id widget_id,
		text_buffer& buffer,
		text_area_state& state,
		std::span<const text_span> spans,
		const ui_rect& rect,
		bool read_only,
		bool show_line_numbers,
		id& hot_widget_id,
		id& focus_widget_id
	) -> void;
}

auto gse::gui::text_area::draw(
	const draw_context& ctx,
	const params& p,
	id& hot,
	id& active,
	id& focus
) -> void {
	(void)active;
	const ui_rect rect = p.rect.value_or(ctx.next_row(8.f));
	draw::text_area_in_rect(
		ctx,
		ids::make_from_key(stable_id("##TextArea")),
		p.buffer,
		p.state,
		p.spans,
		rect,
		p.read_only,
		p.show_line_numbers,
		hot,
		focus
	);
}

auto gse::gui::draw::text_area_in_rect(
	const draw_context& ctx,
	id widget_id,
	text_buffer& buffer,
	text_area_state& state,
	std::span<const text_span> spans,
	const ui_rect& rect,
	bool read_only,
	bool show_line_numbers,
	id& hot_widget_id,
	id& focus_widget_id
) -> void {
	(void)widget_id;
	(void)buffer;
	(void)state;
	(void)spans;
	(void)read_only;
	(void)show_line_numbers;
	(void)hot_widget_id;
	(void)focus_widget_id;

	ctx.queue_sprite({
		.rect = rect,
		.color = ctx.style.color_input_background,
		.texture = ctx.blank_texture,
		.corner_radius = ctx.style.corner_radius,
	});
}

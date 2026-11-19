export module gse.complete.view;

import std;

import gse.analysis;
import gse.complete;
import gse.complete.widgets;
import gse.graphics;
import gse.math;
import gse.time;

export namespace gse::complete {
	using token_color = vec4f (*)(const analysis::highlight_token& token);

	struct hover_view {
		std::optional<position> at;
		std::uint64_t generation = 0;
		std::optional<hover_info> info;
		std::vector<gui::text_span> title_spans;
		deadline_timer reveal;
	};

	struct view_state {
		session state;
		gui::completion_state popup;
		std::uint64_t shown = 0;
		hover_view hover;
	};

	struct view_params {
		gui::text_area_geometry geometry;
		gui::text_buffer& buffer;
		gui::text_area_state& area;
		rectf bounds;
	};

	auto draw_popup(
		gui::builder& ui,
		view_state& view,
		const view_params& p
	) -> bool;

	auto draw_hints(
		gui::builder& ui,
		const view_state& view,
		const view_params& p
	) -> void;

	struct hover_input {
		const sources& from;
		bool edited = false;
		token_color color;
	};

	auto draw_hover(
		gui::builder& ui,
		view_state& view,
		const view_params& p,
		const hover_input& in
	) -> void;

	auto buffer_point(
		position at
	) -> gui::buffer_position;

	auto caret_of(
		const gui::text_area_state& area
	) -> position;
}

namespace gse::complete {
	auto accent_of(
		std::uint32_t rgb
	) -> vec4f;

	auto forget(
		hover_view& hover
	) -> void;

	auto spans_of(
		std::string_view text,
		token_color color
	) -> std::vector<gui::text_span>;
}

auto gse::complete::accent_of(const std::uint32_t rgb) -> vec4f {
	return {
		static_cast<float>((rgb >> 16) & 0xff) / 255.f,
		static_cast<float>((rgb >> 8) & 0xff) / 255.f,
		static_cast<float>(rgb & 0xff) / 255.f,
		1.f,
	};
}

auto gse::complete::buffer_point(const position at) -> gui::buffer_position {
	return {
		.line = at.line,
		.column = at.column,
	};
}

auto gse::complete::caret_of(const gui::text_area_state& area) -> position {
	return {
		.line = area.caret.line,
		.column = area.caret.column,
	};
}

auto gse::complete::draw_popup(gui::builder& ui, view_state& view, const view_params& p) -> bool {
	session& s = view.state;
	if (!s.open || s.current.items.empty()) {
		return false;
	}
	if (view.shown != s.serial) {
		view.shown = s.serial;
		view.popup.selected = 0;
		view.popup.reveal = true;
	}

	const std::vector<entity_info> infos = s.current.items
		| std::views::transform([](const candidate& c) { return info_of(c.kind); })
		| std::ranges::to<std::vector>();
	std::vector<gui::completion_item> items;
	items.reserve(s.current.items.size());
	for (std::size_t i = 0; i < s.current.items.size(); ++i) {
		const candidate& c = s.current.items[i];
		items.push_back({
			.label = c.label,
			.detail = c.detail,
			.badge = infos[i].badge,
			.accent = accent_of(infos[i].color),
			.matched = c.matched,
		});
	}

	const gui::completion_result picked = ui.draw<gui::completion_popup>({
		.items = items,
		.state = view.popup,
		.anchor = gui::text_area_point_of(ui.ctx, p.geometry, buffer_point(s.current.where.replace_from)),
		.line_height = gui::text_area_line_height(ui.ctx, p.geometry.font),
		.bounds = p.bounds,
		.font = p.geometry.font,
	});
	if (picked.action == gui::completion_action::dismiss) {
		dismiss(s);
		return false;
	}
	if (picked.action != gui::completion_action::accept) {
		return false;
	}
	const std::optional<accept_edit> edit = accept(s, picked.index);
	if (!edit) {
		return false;
	}
	gui::text_area_replace(p.buffer, p.area, buffer_point(edit->from), buffer_point(edit->to), edit->text);
	return true;
}

auto gse::complete::draw_hints(gui::builder& ui, const view_state& view, const view_params& p) -> void {
	const session& s = view.state;
	const float line_height = gui::text_area_line_height(ui.ctx, p.geometry.font);
	const gui::buffer_position caret = p.area.caret;

	if (const std::string_view ghost = ghost_of(s, view.popup.selected); !ghost.empty()) {
		ui.draw<gui::ghost_text>({
			.text = ghost,
			.anchor = gui::text_area_point_of(ui.ctx, p.geometry, caret),
			.line_height = line_height,
			.clip = p.geometry.rect,
			.font = p.geometry.font,
		});
	}

	if (!s.signature || s.signature->overloads.empty()) {
		return;
	}
	const signature_help& help = *s.signature;
	const signature& shown = help.overloads[std::min<std::size_t>(help.active, help.overloads.size() - 1)];
	const parameter_span active = help.argument < shown.parameters.size() ? shown.parameters[help.argument] : parameter_span{};
	const std::string counter = signature_counter(help);
	ui.draw<gui::signature_strip>({
		.label = shown.label,
		.active_start = active.start,
		.active_length = active.length,
		.counter = counter,
		.anchor = gui::text_area_point_of(ui.ctx, p.geometry, buffer_point(help.open)),
		.line_height = line_height,
		.bounds = p.bounds,
		.font = p.geometry.font,
	});
}

auto gse::complete::forget(hover_view& hover) -> void {
	hover.at.reset();
	hover.info.reset();
	hover.title_spans.clear();
	hover.reveal.disarm();
}

auto gse::complete::spans_of(const std::string_view text, const token_color color) -> std::vector<gui::text_span> {
	return analysis::highlight(text, nullptr)
		| std::views::transform([color](const analysis::highlight_token& t) {
			return gui::text_span{
				.line = t.line,
				.start_col = t.start,
				.end_col = t.end,
				.color = color(t),
			};
		})
		| std::ranges::to<std::vector>();
}

auto gse::complete::draw_hover(gui::builder& ui, view_state& view, const view_params& p, const hover_input& in) -> void {
	const gui::draw_context& ctx = ui.ctx;
	hover_view& h = view.hover;
	if (in.edited || view.state.open || ctx.mouse_held() || !ctx.hovers(p.bounds)) {
		forget(h);
		return;
	}

	const vec2f mouse = ctx.mouse_position();
	const gui::buffer_position picked = gui::text_area_position_at(ctx, p.geometry, mouse);
	const position at{
		.line = picked.line,
		.column = picked.column,
	};
	if (h.at != at || h.generation != view.state.generation) {
		std::optional<hover_info> info = describe_at(in.from, at);
		const bool same = info && h.info && info->from == h.info->from && info->title == h.info->title;
		if (!same) {
			h.reveal.disarm();
			h.title_spans.clear();
			if (info) {
				h.reveal.arm(gui::tooltip_state::show_delay);
				h.title_spans = spans_of(info->title, in.color);
			}
		}
		h.at = at;
		h.generation = view.state.generation;
		h.info = std::move(info);
	}
	if (!h.info) {
		return;
	}

	const float line_height = gui::text_area_line_height(ctx, p.geometry.font);
	const vec2f start = gui::text_area_point_of(ctx, p.geometry, buffer_point(h.info->from));
	const vec2f end = gui::text_area_point_of(ctx, p.geometry, buffer_point(h.info->to));
	const rectf word = rectf::from_position_size(start, { end.x() - start.x(), line_height });
	if (!ctx.hovers(word) || !h.reveal.due()) {
		return;
	}

	const entity_info kind = info_of(h.info->kind);
	ui.draw<gui::hover_card>({
		.title = h.info->title,
		.title_spans = h.title_spans,
		.detail = h.info->detail,
		.badge = kind.badge,
		.accent = accent_of(kind.color),
		.anchor = start,
		.line_height = line_height,
		.bounds = p.bounds,
		.font = p.geometry.font,
	});
}

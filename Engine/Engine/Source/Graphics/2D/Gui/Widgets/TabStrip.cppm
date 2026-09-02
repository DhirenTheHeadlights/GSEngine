export module gse.graphics:tab_strip;

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
import gse.assert;

import :types;
import :styles;
import :ids;
import :input_layers;
import :font;
import :symbols;
import :scroll_widget;
import :ui_renderer;

export namespace gse::gui {
	enum class tab_orientation : std::uint8_t {
		horizontal,
		vertical,
	};

	enum class tab_overflow : std::uint8_t {
		scroll,
		wrap,
	};

	struct tab_desc {
		id tab_id;
		std::string_view caption;
		bool dirty = false;
		bool busy = false;
		bool closeable = true;
		bool pinned = false;
		bool warning = false;
		bool error = false;
		bool dimmed = false;
	};

	struct tab_strip_params {
		rectf area;
		std::span<const tab_desc> tabs;
		id active;
		tab_orientation orientation = tab_orientation::horizontal;
		tab_overflow overflow = tab_overflow::scroll;
		bool allow_reorder = false;
		bool show_add = false;
		id renaming;
		float min_tab_extent = 64.f;
		float max_tab_extent = 220.f;
		resource::handle<font> font{};
	};

	struct tab_strip_result {
		id activated;
		id close_requested;
		id context_requested;
		vec2f context_position{};
		id reorder_id;
		std::size_t reorder_to = 0;
		bool add_requested = false;
		rectf renaming_rect;
	};

	struct tab_strip_measure_params {
		resource::handle<font> font{};
		std::span<const tab_desc> tabs;
		float available_extent = 0.f;
		bool show_add = false;
		float min_tab_extent = 64.f;
		float max_tab_extent = 220.f;
	};

	struct tab_strip_metrics {
		float content_extent = 0.f;
		std::uint32_t required_rows = 1;
		float row_extent = 0.f;
		float row_gap = 0.f;
		float scroll_bar_extent = 0.f;
		bool overflow = false;
	};

	struct tab_strip_placement {
		std::size_t index = 0;
		rectf rect;
		rectf close_rect;
	};

	auto tab_strip(
		const draw_context& ctx,
		const tab_strip_params& params,
		tab_strip_state& state
	) -> tab_strip_result;

	auto tab_strip_row_extent(
		const resource::handle<font>& fnt,
		const style& sty
	) -> float;

	auto tab_strip_measure(
		const style& sty,
		const tab_strip_measure_params& params
	) -> tab_strip_metrics;

	auto tab_strip_extent(
		const tab_strip_metrics& metrics,
		std::uint32_t visible_rows
	) -> float;

	auto tab_strip_layout(
		const resource::handle<font>& fnt,
		const style& sty,
		const rectf& area,
		std::span<const tab_desc> tabs,
		const tab_strip_state& state,
		tab_overflow overflow,
		float min_tab_extent = 64.f,
		float max_tab_extent = 220.f
	) -> std::vector<tab_strip_placement>;
}

namespace gse::gui {
	constexpr float base_tab_gap = 2.f;

	auto warning_extent(
		const style& sty
	) -> float;

	auto tab_extent(
		const resource::handle<font>& fnt,
		const style& sty,
		const tab_desc& tab,
		float min_extent,
		float max_extent,
		float close_extent,
		float pad
	) -> float;
}

auto gse::gui::warning_extent(const style& sty) -> float {
	return std::floor(sty.font_size * 0.35f);
}

auto gse::gui::tab_extent(const resource::handle<font>& fnt, const style& sty, const tab_desc& tab, const float min_extent, const float max_extent, const float close_extent, const float pad) -> float {
	const auto fnt_view = fnt.resolve();
	const float fs = sty.font_size;
	const float caption_w = fnt_view->width(tab.caption, fs);
	const float dirty_w = tab.dirty ? fnt_view->width("*", fs) + pad * 0.5f : 0.f;
	const float warning_w = tab.warning || tab.error ? warning_extent(sty) + pad * 0.5f : 0.f;
	return std::clamp(caption_w + dirty_w + warning_w + pad * 3.f + close_extent, min_extent * sty.scale_factor, max_extent * sty.scale_factor);
}

auto gse::gui::tab_strip_row_extent(const resource::handle<font>& fnt, const style& sty) -> float {
	return fnt.resolve()->line_height(sty.font_size) + sty.padding;
}

auto gse::gui::tab_strip_measure(const style& sty, const tab_strip_measure_params& params) -> tab_strip_metrics {
	const float pad = sty.padding;
	const float close_extent = sty.icon_extent;
	const float tab_gap = base_tab_gap * sty.scale_factor;
	const float row_extent = tab_strip_row_extent(params.font, sty);
	const float available = std::max(0.f, params.available_extent - (params.show_add ? row_extent + tab_gap : 0.f));

	tab_strip_metrics metrics{
		.row_extent = row_extent,
		.row_gap = tab_gap,
		.scroll_bar_extent = 6.f * sty.scale_factor,
	};
	float row_width = 0.f;
	for (const tab_desc& tab : params.tabs) {
		const float w = tab_extent(params.font, sty, tab, params.min_tab_extent, params.max_tab_extent, close_extent, pad);
		metrics.content_extent += (metrics.content_extent > 0.f ? tab_gap : 0.f) + w;
		if (row_width > 0.f && row_width + tab_gap + w > available) {
			++metrics.required_rows;
			row_width = w;
		}
		else {
			row_width += (row_width > 0.f ? tab_gap : 0.f) + w;
		}
	}
	metrics.overflow = metrics.content_extent > available;
	return metrics;
}

auto gse::gui::tab_strip_extent(const tab_strip_metrics& metrics, const std::uint32_t visible_rows) -> float {
	const auto rows = static_cast<float>(std::max(1u, visible_rows));
	const float rows_extent = rows * metrics.row_extent + (rows - 1.f) * metrics.row_gap;
	const bool scrolling = visible_rows <= 1 && metrics.overflow;
	return rows_extent + (scrolling ? metrics.row_gap + metrics.scroll_bar_extent : 0.f);
}

auto gse::gui::tab_strip_layout(const resource::handle<font>& fnt, const style& sty, const rectf& area, const std::span<const tab_desc> tabs, const tab_strip_state& state, const tab_overflow overflow, const float min_tab_extent, const float max_tab_extent) -> std::vector<tab_strip_placement> {
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const float close_extent = sty.icon_extent;
	const auto fnt_view = fnt.resolve();
	const float row_h = std::min(fnt_view->line_height(fs) + pad, area.height());
	const float tab_gap = base_tab_gap * sty.scale_factor;

	std::vector<float> widths;
	widths.reserve(tabs.size());
	for (const tab_desc& tab : tabs) {
		widths.push_back(tab_extent(fnt, sty, tab, min_tab_extent, max_tab_extent, close_extent, pad));
	}

	const auto make_close = [&](const tab_desc& tab, const rectf& rect) -> rectf {
		if (!tab.closeable && !tab.busy) {
			return {};
		}
		return rectf::from_position_size({ rect.right() - close_extent - pad, rect.center().y() + close_extent * 0.5f }, { close_extent, close_extent });
	};

	std::vector<tab_strip_placement> out;
	out.reserve(tabs.size());

	const bool single_row = overflow == tab_overflow::scroll || state.visible_rows == 1;
	if (single_row) {
		float x = area.left() - state.scroll.offset;
		for (std::size_t i = 0; i < tabs.size(); ++i) {
			const rectf rect = rectf::from_position_size({ x, area.top() }, { widths[i], row_h });
			out.push_back({
				.index = i,
				.rect = rect,
				.close_rect = make_close(tabs[i], rect),
			});
			x += widths[i] + tab_gap;
		}
	}
	else {
		float x = area.left();
		float y = area.top();
		std::uint32_t row = 0;
		for (std::size_t i = 0; i < tabs.size(); ++i) {
			if (x > area.left() && x + widths[i] > area.right()) {
				++row;
				x = area.left();
				y -= row_h + tab_gap;
			}
			if (row >= state.visible_rows) {
				break;
			}
			const rectf rect = rectf::from_position_size({ x, y }, { widths[i], row_h });
			out.push_back({
				.index = i,
				.rect = rect,
				.close_rect = make_close(tabs[i], rect),
			});
			x += widths[i] + tab_gap;
		}
	}
	return out;
}

auto gse::gui::tab_strip(const draw_context& ctx, const tab_strip_params& params, tab_strip_state& state) -> tab_strip_result {
	tab_strip_result result{};
	if (params.tabs.empty() && !params.show_add) {
		return result;
	}
	assert(
		std::ranges::all_of(params.tabs, [](const tab_desc& tab) {
			return tab.tab_id.exists();
		}),
		"tab strip requires a valid ID for every tab"
	);

	const resource::handle<font>& fnt = params.font.valid() ? params.font : ctx.fonts.text;
	const auto fnt_view = fnt.resolve();
	if (!fnt.valid()) {
		return result;
	}
	const style& sty = ctx.style;
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const rectf area = params.area;
	const vec2f mouse = ctx.mouse_position();
	const bool available = ctx.input_available();
	const bool pressed = ctx.mouse_pressed() && available;
	const bool held = ctx.mouse_held();

	if (ctx.hit_regions) {
		ctx.hit_regions->block_text_selection(area);
	}

	const angle spin = symbol::spinner_rotation();
	const float close_extent = sty.icon_extent;
	const float dirty_extent = fnt_view->width("*", fs);
	const float dot_extent = warning_extent(sty);
	rectf tab_area = params.area;

	const auto draw_cell = [&](const rectf& rect, const rectf& visible, const rectf& close_rect, const tab_desc& tab, const bool is_active, const bool hovered) {
		ctx.queue_sprite({
			.rect = rect,
			.color = is_active ? sty.color_tab_active : (hovered ? sty.color_tab_hovered : sty.color_tab_background),
			.texture = ctx.blank_texture,
			.clip_rect = visible,
		});

		const bool show_close = tab.busy || tab.closeable;
		const float close_zone = show_close ? close_rect.width() + pad : pad;
		const float marker_zone = tab.dirty ? dirty_extent + pad * 0.5f : 0.f;
		const float warning_zone = tab.warning || tab.error ? dot_extent + pad * 0.5f : 0.f;
		const float caption_right = rect.right() - close_zone - marker_zone;
		const rectf caption_clip = visible.intersection(rectf::from_position_size(
			{ rect.left(), rect.top() },
			{ std::max(0.f, caption_right - rect.left()), rect.height() }
		));

		if (tab.warning || tab.error) {
			const vec2f dot_center = symbol::snap_to_pixel_center({ rect.left() + pad + dot_extent * 0.5f, rect.center().y() });
			ctx.queue_sprite({
				.rect = rectf::from_position_size(
					{ dot_center.x() - dot_extent * 0.5f, dot_center.y() + dot_extent * 0.5f },
					{ dot_extent, dot_extent }
				),
				.color = tab.error ? sty.color_error : sty.color_warning,
				.texture = ctx.blank_texture,
				.clip_rect = caption_clip,
				.corner_radius = dot_extent * 0.5f,
			});
		}

		ctx.queue_text({
			.font = fnt,
			.text = tab.caption,
			.position = { rect.left() + pad + warning_zone, rect.center().y() + fnt_view->vertical_center_offset(fs) },
			.scale = fs,
			.color = tab.dimmed ? sty.color_text_disabled : (is_active ? sty.color_text : sty.color_text_secondary),
			.clip_rect = caption_clip,
		});
		if (tab.dirty) {
			ctx.queue_text({
				.font = fnt,
				.text = "*",
				.position = { caption_right + pad * 0.25f, rect.center().y() + fnt_view->vertical_center_offset(fs) },
				.scale = fs,
				.color = is_active ? sty.color_text : sty.color_text_secondary,
				.clip_rect = visible,
			});
		}
		const bool close_hovered = tab_area.contains(mouse) && ctx.hovers(close_rect);
		if (tab.busy && !(tab.closeable && close_hovered)) {
			symbol::spinner(ctx, close_rect, spin, {
				.color = sty.color_text_secondary,
				.extent = sty.icon_extent,
				.clip_rect = visible,
			});
		}
		else if (tab.closeable) {
			symbol::draw(ctx, symbol::close(), close_rect, {
				.color = close_hovered ? sty.color_text : sty.color_text_secondary,
				.extent = sty.icon_extent,
				.clip_rect = visible,
			});
		}
	};

	const auto draw_add_cell = [&](const rectf& cell) -> bool {
		const bool hovered = ctx.hovers(cell);
		ctx.queue_sprite({
			.rect = cell,
			.color = hovered ? sty.color_tab_hovered : sty.color_tab_background,
			.texture = ctx.blank_texture,
		});
		symbol::draw(ctx, symbol::plus(), cell, {
			.color = hovered ? sty.color_text : sty.color_text_secondary,
			.extent = sty.icon_extent,
			.clip_rect = cell,
		});
		return hovered && ctx.mouse_pressed_for(cell);
	};

	if (params.orientation == tab_orientation::vertical) {
		const float cell_h = fnt_view->line_height(fs) + pad;
		const std::size_t add_cells = params.show_add ? 1 : 0;
		const float content_h = cell_h * static_cast<float>(params.tabs.size() + add_cells);
		const float max_scroll = std::max(0.f, content_h - area.height());

		if (ctx.hovers(area) && !ctx.is_scroll_consumed()) {
			const vec2f wheel = ctx.scroll_delta();
			if (std::abs(wheel.y()) > 0.001f) {
				state.scroll.offset = std::clamp(state.scroll.offset - wheel.y() * cell_h, 0.f, max_scroll);
				ctx.consume_scroll();
			}
		}
		state.scroll.offset = std::clamp(state.scroll.offset, 0.f, max_scroll);

		if (state.scroll_active != params.active) {
			const auto active_it = std::ranges::find(params.tabs, params.active, &tab_desc::tab_id);
			if (active_it != params.tabs.end()) {
				const float active_top = static_cast<float>(std::distance(params.tabs.begin(), active_it)) * cell_h;
				if (active_top < state.scroll.offset) {
					state.scroll.offset = active_top;
				}
				else if (active_top + cell_h > state.scroll.offset + area.height()) {
					state.scroll.offset = active_top + cell_h - area.height();
				}
			}
			state.scroll_active = params.active;
		}
		state.scroll.offset = std::clamp(state.scroll.offset, 0.f, max_scroll);

		rectf active_bar{};
		bool has_active = false;
		float y = area.top() + state.scroll.offset;
		for (std::size_t i = 0; i < params.tabs.size(); ++i) {
			const tab_desc& tab = params.tabs[i];
			const rectf cell = rectf::from_position_size({ area.left(), y }, { area.width(), cell_h });
			const rectf visible = cell.intersection(area);
			y -= cell_h;
			if (visible.height() <= 0.f) {
				continue;
			}
			const bool is_active = tab.tab_id == params.active;
			const bool hovered = ctx.hovers(visible);
			const rectf close_rect = rectf::from_position_size({ cell.right() - close_extent - pad, cell.center().y() + close_extent * 0.5f }, { close_extent, close_extent });

			if (is_active) {
				active_bar = rectf::from_position_size({ area.left(), visible.top() }, { sty.accent_bar_width, visible.height() });
				has_active = true;
			}
			draw_cell(cell, visible, close_rect, tab, is_active, hovered);

			if (hovered && ctx.mouse_pressed_for(visible)) {
				if (tab.closeable && close_rect.contains(mouse)) {
					result.close_requested = tab.tab_id;
				}
				else {
					result.activated = tab.tab_id;
					if (params.allow_reorder && !tab.pinned) {
						state.dragging = tab.tab_id;
					}
				}
			}
			if (hovered && ctx.mouse_pressed_for(visible, mouse_button::button_2)) {
				result.context_requested = tab.tab_id;
				result.context_position = mouse;
			}
		}

		if (params.allow_reorder && state.dragging.exists() && held) {
			const auto cur = std::ranges::find(params.tabs, state.dragging, &tab_desc::tab_id);
			if (cur != params.tabs.end()) {
				const float row = std::floor((area.top() + state.scroll.offset - mouse.y()) / cell_h);
				const auto target = static_cast<std::size_t>(std::clamp(row, 0.f, static_cast<float>(params.tabs.size() - 1)));
				if (target != static_cast<std::size_t>(std::distance(params.tabs.begin(), cur))) {
					result.reorder_id = state.dragging;
					result.reorder_to = target;
				}
			}
		}
		else {
			state.dragging.reset();
		}

		if (params.show_add) {
			const rectf add_cell = rectf::from_position_size({ area.left(), y }, { area.width(), cell_h });
			if (const rectf visible = add_cell.intersection(area); visible.height() > 0.f) {
				result.add_requested = draw_add_cell(visible);
			}
		}

		if (has_active) {
			ctx.queue_sprite({
				.rect = active_bar,
				.color = sty.color_accent,
				.texture = ctx.blank_texture,
			});
		}
		return result;
	}

	const tab_strip_metrics metrics = tab_strip_measure(sty, {
		.font = fnt,
		.tabs = params.tabs,
		.available_extent = area.width(),
		.show_add = params.show_add,
		.min_tab_extent = params.min_tab_extent,
		.max_tab_extent = params.max_tab_extent,
	});
	const float tab_gap = metrics.row_gap;
	const float row_h = std::min(metrics.row_extent, area.height());
	const float add_zone = params.show_add ? metrics.row_extent + tab_gap : 0.f;
	tab_area = rectf::from_position_size(area.top_left(), { std::max(0.f, area.width() - add_zone), area.height() });

	const float total = metrics.content_extent;
	const bool overflow = metrics.overflow;

	if (overflow && ctx.hovers(tab_area) && !ctx.is_scroll_consumed()) {
		const vec2f wheel = ctx.scroll_delta();
		const bool shift = ctx.key_held(key::left_shift) || ctx.key_held(key::right_shift);
		if (params.overflow == tab_overflow::wrap && std::abs(wheel.y()) > 0.001f && !shift) {
			if (wheel.y() > 0.f) {
				state.visible_rows = state.visible_rows > 1 ? state.visible_rows - 1 : 1;
			}
			else {
				++state.visible_rows;
			}
			ctx.consume_scroll();
		}
		else if (std::abs(wheel.x()) > 0.001f || (shift && std::abs(wheel.y()) > 0.001f)) {
			state.scroll.offset -= (wheel.x() + wheel.y()) * 80.f * sty.scale_factor;
			state.scroll.target = state.scroll.offset;
			ctx.consume_scroll();
		}
	}

	const bool single_row = params.overflow == tab_overflow::scroll || state.visible_rows == 1;
	const float max_scroll = std::max(0.f, total - tab_area.width());
	if (single_row) {
		state.scroll.offset = std::clamp(state.scroll.offset, 0.f, max_scroll);
		state.scroll.target = std::clamp(state.scroll.target, 0.f, max_scroll);
		if (state.scroll_active != params.active) {
			float acc = 0.f;
			for (const tab_desc& tab : params.tabs) {
				const float w = tab_extent(fnt, sty, tab, params.min_tab_extent, params.max_tab_extent, close_extent, pad);
				if (tab.tab_id == params.active) {
					if (acc < state.scroll.offset) {
						state.scroll.offset = acc;
					}
					else if (acc + w > state.scroll.offset + tab_area.width()) {
						state.scroll.offset = acc + w - tab_area.width();
					}
					break;
				}
				acc += w + tab_gap;
			}
			state.scroll.offset = std::clamp(state.scroll.offset, 0.f, max_scroll);
			state.scroll.target = state.scroll.offset;
			state.scroll_active = params.active;
		}
	}
	else {
		state.scroll.offset = 0.f;
		state.scroll.target = 0.f;
	}

	const std::vector<tab_strip_placement> placements = tab_strip_layout(fnt, sty, tab_area, params.tabs, state, params.overflow, params.min_tab_extent, params.max_tab_extent);

	for (const tab_strip_placement& p : placements) {
		const rectf visible = p.rect.intersection(tab_area);
		if (visible.width() <= 0.f || !ctx.hovers(visible)) {
			continue;
		}
		const tab_desc& tab = params.tabs[p.index];
		if (tab.tab_id == params.renaming) {
			break;
		}
		if (ctx.mouse_pressed_for(visible)) {
			if (tab.closeable && p.close_rect.contains(mouse)) {
				result.close_requested = tab.tab_id;
			}
			else {
				result.activated = tab.tab_id;
				if (params.allow_reorder && !tab.pinned) {
					state.dragging = tab.tab_id;
				}
			}
			break;
		}
		if (ctx.mouse_pressed_for(visible, mouse_button::button_2)) {
			result.context_requested = tab.tab_id;
			result.context_position = mouse;
			break;
		}
	}

	if (params.allow_reorder && state.dragging.exists() && held) {
		const auto cur = std::ranges::find(params.tabs, state.dragging, &tab_desc::tab_id);
		if (cur != params.tabs.end()) {
			const auto cur_idx = static_cast<std::size_t>(std::distance(params.tabs.begin(), cur));
			std::size_t target = 0;
			for (const tab_strip_placement& p : placements) {
				if (params.tabs[p.index].tab_id != state.dragging && mouse.x() > p.rect.center().x()) {
					++target;
				}
			}
			target = std::min(target, params.tabs.size() - 1);
			if (target != cur_idx) {
				result.reorder_id = state.dragging;
				result.reorder_to = target;
			}
		}
	}
	else {
		state.dragging.reset();
	}

	for (const tab_strip_placement& p : placements) {
		const rectf visible = p.rect.intersection(tab_area);
		if (visible.width() <= 0.f || visible.height() <= 0.f) {
			continue;
		}
		const tab_desc& tab = params.tabs[p.index];
		if (tab.tab_id == params.renaming) {
			result.renaming_rect = visible;
			continue;
		}
		const bool is_active = tab.tab_id == params.active;
		const bool hovered = ctx.hovers(visible);
		draw_cell(p.rect, visible, p.close_rect, tab, is_active, hovered);
	}

	if (single_row && overflow) {
		const float scrollbar_h = metrics.scroll_bar_extent;
		const float track_top = std::max(tab_area.bottom() + scrollbar_h, tab_area.top() - row_h - tab_gap);
		const rectf track_rect = rectf::from_position_size({ tab_area.left(), track_top }, { tab_area.width(), scrollbar_h });
		const scroll_bar_result bar = update_scroll_bar(state.scroll, {
			.track_rect = track_rect,
			.visible_extent = tab_area.width(),
			.content_extent = total,
			.horizontal = true,
			.mouse = mouse,
			.mouse_pressed = pressed && !ctx.is_press_consumed(mouse_button::button_1),
			.mouse_held = held,
			.min_thumb = 24.f * sty.scale_factor,
		});
		if (bar.used_press) {
			ctx.consume_press(mouse_button::button_1);
		}
		vec4f track_color = sty.color_widget_background;
		track_color.w() *= 0.4f;
		ctx.queue_sprite({
			.rect = bar.track_rect,
			.color = track_color,
			.texture = ctx.blank_texture,
		});
		ctx.queue_sprite({
			.rect = bar.thumb_rect,
			.color = bar.held || bar.hovered ? sty.color_widget_hovered : sty.color_widget_background,
			.texture = ctx.blank_texture,
		});
	}

	if (params.show_add) {
		const rectf add_cell = rectf::from_position_size({ tab_area.right() + tab_gap, area.top() }, { row_h, row_h });
		result.add_requested = draw_add_cell(add_cell);
	}

	return result;
}

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

import :types;
import :styles;
import :ids;
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
		std::uint64_t id = 0;
		std::string_view caption;
		bool dirty = false;
		bool busy = false;
		bool closeable = true;
		bool pinned = false;
	};

	struct tab_strip_params {
		rectf area;
		std::span<const tab_desc> tabs;
		std::uint64_t active = 0;
		tab_orientation orientation = tab_orientation::horizontal;
		tab_overflow overflow = tab_overflow::scroll;
		bool allow_reorder = false;
		bool show_add = false;
		float min_tab_extent = 64.f;
		float max_tab_extent = 220.f;
		resource::handle<font> font{};
	};

	struct tab_strip_result {
		std::uint64_t activated = 0;
		std::uint64_t close_requested = 0;
		std::uint64_t context_requested = 0;
		vec2f context_position{};
		std::uint64_t reorder_id = 0;
		std::size_t reorder_to = 0;
		bool add_requested = false;
	};

	struct tab_strip_metrics {
		float content_extent = 0.f;
		std::uint32_t required_rows = 1;
	};

	struct tab_strip_placement {
		std::size_t index = 0;
		std::uint64_t id = 0;
		rectf rect;
		rectf close_rect;
	};

	auto tab_strip(
		const draw_context& ctx,
		const tab_strip_params& params,
		tab_strip_state& state
	) -> tab_strip_result;

	auto tab_strip_measure(
		const resource::handle<font>& fnt,
		const style& sty,
		std::span<const tab_desc> tabs,
		float available_width,
		float min_tab_extent = 64.f,
		float max_tab_extent = 220.f
	) -> tab_strip_metrics;

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

	auto draw_tab_spinner(const draw_context& ctx, const rectf& rect, vec4f color, angle rotation) -> void {
		constexpr int segments = 9;
		constexpr float radius = 0.34f;
		std::array<symbol::stroke, segments> strokes{};
		const angle sweep = degrees(270.f);
		vec2f previous{};
		for (int i = 0; i <= segments; ++i) {
			const float t = static_cast<float>(i) / static_cast<float>(segments);
			const angle a = rotation + sweep * t;
			const vec2f point{ 0.5f + cos(a) * radius, 0.5f + sin(a) * radius };
			if (i > 0) {
				strokes[static_cast<std::size_t>(i - 1)] = { previous, point };
			}
			previous = point;
		}
		symbol::draw(ctx, strokes, rect, {
			.color = color,
			.extent = ctx.style.icon_extent,
			.clip_rect = rect,
		});
	}

	auto tab_extent(const resource::handle<font>& fnt, const style& sty, const tab_desc& tab, const float min_extent, const float max_extent, const float close_extent, const float pad) -> float {
		const float fs = sty.font_size;
		const float caption_w = fnt->width(tab.caption, fs);
		const float dirty_w = tab.dirty ? fnt->width("*", fs) + pad * 0.5f : 0.f;
		return std::clamp(caption_w + dirty_w + pad * 3.f + close_extent, min_extent * sty.scale_factor, max_extent * sty.scale_factor);
	}
}

auto gse::gui::tab_strip_measure(const resource::handle<font>& fnt, const style& sty, const std::span<const tab_desc> tabs, const float available_width, const float min_tab_extent, const float max_tab_extent) -> tab_strip_metrics {
	const float pad = sty.padding;
	const float close_extent = sty.icon_extent;
	const float tab_gap = base_tab_gap * sty.scale_factor;

	tab_strip_metrics metrics{};
	float row_width = 0.f;
	for (const tab_desc& tab : tabs) {
		const float w = tab_extent(fnt, sty, tab, min_tab_extent, max_tab_extent, close_extent, pad);
		metrics.content_extent += (metrics.content_extent > 0.f ? tab_gap : 0.f) + w;
		if (row_width > 0.f && row_width + tab_gap + w > available_width) {
			++metrics.required_rows;
			row_width = w;
		}
		else {
			row_width += (row_width > 0.f ? tab_gap : 0.f) + w;
		}
	}
	return metrics;
}

auto gse::gui::tab_strip_layout(const resource::handle<font>& fnt, const style& sty, const rectf& area, const std::span<const tab_desc> tabs, const tab_strip_state& state, const tab_overflow overflow, const float min_tab_extent, const float max_tab_extent) -> std::vector<tab_strip_placement> {
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const float close_extent = sty.icon_extent;
	const float row_h = std::min(fnt->line_height(fs) + pad, area.height());
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
			out.push_back({ .index = i, .id = tabs[i].id, .rect = rect, .close_rect = make_close(tabs[i], rect) });
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
			out.push_back({ .index = i, .id = tabs[i].id, .rect = rect, .close_rect = make_close(tabs[i], rect) });
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

	const resource::handle<font>& fnt = params.font.valid() ? params.font : ctx.fonts.text;
	if (!fnt.valid()) {
		return result;
	}
	const style& sty = ctx.style;
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const rectf area = params.area;
	const vec2f mouse = ctx.input.mouse_position();
	const bool available = ctx.input_available();
	const bool pressed = ctx.input.mouse_button_pressed(mouse_button::button_1) && available;
	const bool held = ctx.input.mouse_button_held(mouse_button::button_1);
	const bool right_pressed = ctx.input.mouse_button_pressed(mouse_button::button_2) && available;

	state.spinner_phase += 0.16f;
	const angle spin = radians(state.spinner_phase);
	const float close_extent = sty.icon_extent;
	const float dirty_extent = fnt->width("*", fs);

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
		const float caption_right = rect.right() - close_zone - marker_zone;
		const rectf caption_clip = visible.intersection(rectf::from_position_size(
			{ rect.left(), rect.top() },
			{ std::max(0.f, caption_right - rect.left()), rect.height() }
		));

		ctx.queue_text({
			.font = fnt,
			.text = tab.caption,
			.position = { rect.left() + pad, rect.center().y() + fnt->vertical_center_offset(fs) },
			.scale = fs,
			.color = is_active ? sty.color_text : sty.color_text_secondary,
			.clip_rect = caption_clip,
		});
		if (tab.dirty) {
			ctx.queue_text({
				.font = fnt,
				.text = "*",
				.position = { caption_right + pad * 0.25f, rect.center().y() + fnt->vertical_center_offset(fs) },
				.scale = fs,
				.color = is_active ? sty.color_text : sty.color_text_secondary,
				.clip_rect = visible,
			});
		}
		const bool close_hovered = close_rect.contains(mouse) && area.contains(mouse) && available;
		if (tab.busy && !(tab.closeable && close_hovered)) {
			draw_tab_spinner(ctx, close_rect, sty.color_text_secondary, spin);
		}
		else if (tab.closeable) {
			symbol::draw(ctx, symbol::close(), close_rect, {
				.color = close_hovered ? sty.color_text : sty.color_text_secondary,
				.extent = sty.icon_extent,
				.clip_rect = visible,
			});
		}
	};

	if (params.orientation == tab_orientation::vertical) {
		const float cell_h = fnt->line_height(fs) + pad;
		const std::size_t add_cells = params.show_add ? 1 : 0;
		const float content_h = cell_h * static_cast<float>(params.tabs.size() + add_cells);
		const float max_scroll = std::max(0.f, content_h - area.height());

		if (area.contains(mouse) && !ctx.is_scroll_consumed()) {
			const vec2f wheel = ctx.input.scroll_delta();
			if (std::abs(wheel.y()) > 0.001f) {
				state.scroll.offset = std::clamp(state.scroll.offset - wheel.y() * cell_h, 0.f, max_scroll);
				ctx.consume_scroll();
			}
		}
		state.scroll.offset = std::clamp(state.scroll.offset, 0.f, max_scroll);

		if (state.scroll_active != params.active) {
			const auto active_it = std::ranges::find(params.tabs, params.active, &tab_desc::id);
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
			const bool is_active = tab.id == params.active;
			const bool hovered = visible.contains(mouse) && available;
			const rectf close_rect = rectf::from_position_size({ cell.right() - close_extent - pad, cell.center().y() + close_extent * 0.5f }, { close_extent, close_extent });

			if (is_active) {
				active_bar = rectf::from_position_size({ area.left(), visible.top() }, { sty.accent_bar_width, visible.height() });
				has_active = true;
			}
			draw_cell(cell, visible, close_rect, tab, is_active, hovered);

			if (pressed && visible.contains(mouse)) {
				if (tab.closeable && close_rect.contains(mouse)) {
					result.close_requested = tab.id;
				}
				else {
					result.activated = tab.id;
				}
			}
			if (right_pressed && visible.contains(mouse)) {
				result.context_requested = tab.id;
				result.context_position = mouse;
			}
		}

		if (params.show_add) {
			const rectf add_cell = rectf::from_position_size({ area.left(), y }, { area.width(), cell_h });
			const rectf visible = add_cell.intersection(area);
			if (visible.height() > 0.f) {
				const bool hovered = visible.contains(mouse) && available;
				ctx.queue_sprite({
					.rect = visible,
					.color = hovered ? sty.color_tab_hovered : sty.color_tab_background,
					.texture = ctx.blank_texture,
				});
				ctx.queue_text({
					.font = fnt,
					.text = "+",
					.position = { add_cell.center().x() - fnt->width("+", fs) * 0.5f, add_cell.center().y() + fnt->vertical_center_offset(fs) },
					.scale = fs,
					.color = hovered ? sty.color_text : sty.color_text_secondary,
					.clip_rect = visible,
				});
				if (pressed && visible.contains(mouse)) {
					result.add_requested = true;
				}
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

	const float tab_gap = base_tab_gap * sty.scale_factor;
	const float total = tab_strip_measure(fnt, sty, params.tabs, area.width(), params.min_tab_extent, params.max_tab_extent).content_extent;
	const bool overflow = total > area.width();

	if (overflow && area.contains(mouse) && !ctx.is_scroll_consumed()) {
		const vec2f wheel = ctx.input.scroll_delta();
		const bool shift = ctx.input.key_held(key::left_shift) || ctx.input.key_held(key::right_shift);
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
	const float max_scroll = std::max(0.f, total - area.width());
	if (single_row) {
		state.scroll.offset = std::clamp(state.scroll.offset, 0.f, max_scroll);
		state.scroll.target = std::clamp(state.scroll.target, 0.f, max_scroll);
		if (state.scroll_active != params.active) {
			float acc = 0.f;
			for (const tab_desc& tab : params.tabs) {
				const float w = tab_extent(fnt, sty, tab, params.min_tab_extent, params.max_tab_extent, close_extent, pad);
				if (tab.id == params.active) {
					if (acc < state.scroll.offset) {
						state.scroll.offset = acc;
					}
					else if (acc + w > state.scroll.offset + area.width()) {
						state.scroll.offset = acc + w - area.width();
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

	const std::vector<tab_strip_placement> placements = tab_strip_layout(fnt, sty, area, params.tabs, state, params.overflow, params.min_tab_extent, params.max_tab_extent);

	if (pressed) {
		for (const tab_strip_placement& p : placements) {
			const rectf visible = p.rect.intersection(area);
			if (visible.width() <= 0.f) {
				continue;
			}
			const tab_desc& tab = params.tabs[p.index];
			if (tab.closeable && p.close_rect.contains(mouse) && area.contains(mouse)) {
				result.close_requested = tab.id;
				break;
			}
			if (visible.contains(mouse)) {
				result.activated = tab.id;
				if (params.allow_reorder && !tab.pinned) {
					state.dragging = tab.id;
				}
				break;
			}
		}
	}

	if (right_pressed) {
		for (const tab_strip_placement& p : placements) {
			const rectf visible = p.rect.intersection(area);
			if (visible.contains(mouse)) {
				result.context_requested = params.tabs[p.index].id;
				result.context_position = mouse;
				break;
			}
		}
	}

	if (params.allow_reorder && state.dragging != 0 && held) {
		const auto cur = std::ranges::find(params.tabs, state.dragging, &tab_desc::id);
		if (cur != params.tabs.end()) {
			const auto cur_idx = static_cast<std::size_t>(std::distance(params.tabs.begin(), cur));
			std::size_t target = 0;
			for (const tab_strip_placement& p : placements) {
				if (params.tabs[p.index].id != state.dragging && mouse.x() > p.rect.center().x()) {
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
		state.dragging = 0;
	}

	for (const tab_strip_placement& p : placements) {
		const rectf visible = p.rect.intersection(area);
		if (visible.width() <= 0.f || visible.height() <= 0.f) {
			continue;
		}
		const tab_desc& tab = params.tabs[p.index];
		const bool is_active = tab.id == params.active;
		const bool hovered = visible.contains(mouse) && available;
		draw_cell(p.rect, visible, p.close_rect, tab, is_active, hovered);
	}

	if (single_row && overflow) {
		const float scrollbar_h = 6.f * sty.scale_factor;
		const rectf track_rect = rectf::from_position_size({ area.left(), area.bottom() + scrollbar_h + 1.f }, { area.width(), scrollbar_h });
		const scroll_bar_result bar = update_scroll_bar(state.scroll, {
			.track_rect = track_rect,
			.visible_extent = area.width(),
			.content_extent = total,
			.horizontal = true,
			.mouse = mouse,
			.mouse_pressed = pressed,
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

	return result;
}

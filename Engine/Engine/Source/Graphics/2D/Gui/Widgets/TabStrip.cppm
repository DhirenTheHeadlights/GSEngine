export module gse.graphics:tab_strip;

import gse.assert;
import gse.assets;
import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.gpu;
import gse.math;
import gse.os;
import gse.time;
import std;

import :font;
import :input_layers;
import :interaction;
import :scroll_widget;
import :styles;
import :surface_paint;
import :symbols;
import :types;
import :ui_renderer;
import :widget_skin;

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
		std::span<const symbol::stroke> icon{};
		std::optional<vec4f> icon_color = std::nullopt;
		bool dirty = false;
		bool busy = false;
		bool closeable = true;
		bool pinned = false;
		bool warning = false;
		bool error = false;
		bool dimmed = false;
	};

	struct tab_slots {
		rectf icon;
		rectf dot;
		rectf caption;
		rectf marker;
		rectf close;
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
		tab_slots slots;
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
	struct tab_slot_extents {
		float icon = 0.f;
		float dot = 0.f;
		float caption = 0.f;
		float marker = 0.f;
		float close = 0.f;
	};

	auto warning_extent(
		const style& sty
	) -> float;

	auto slot_extents(
		const resource::handle<font>& fnt,
		const style& sty,
		const tab_desc& tab
	) -> tab_slot_extents;

	auto reserved_extent(
		const tab_slot_extents& slots,
		float pad
	) -> float;

	auto tab_extent(
		const resource::handle<font>& fnt,
		const style& sty,
		const tab_desc& tab,
		float min_extent,
		float max_extent
	) -> float;

	auto tab_cell_slots(
		const resource::handle<font>& fnt,
		const style& sty,
		const tab_desc& tab,
		const rectf& cell
	) -> tab_slots;
}

auto gse::gui::warning_extent(const style& sty) -> float {
	return std::floor(sty.font_size * 0.35f);
}

auto gse::gui::slot_extents(const resource::handle<font>& fnt, const style& sty, const tab_desc& tab) -> tab_slot_extents {
	const auto fnt_view = fnt.resolve();
	const float fs = sty.font_size;
	return {
		.icon = tab.icon.empty() ? 0.f : sty.icon_extent,
		.dot = tab.warning || tab.error ? warning_extent(sty) : 0.f,
		.caption = fnt_view->width(tab.caption, fs),
		.marker = tab.dirty ? fnt_view->width("*", fs) : 0.f,
		.close = sty.icon_extent,
	};
}

auto gse::gui::reserved_extent(const tab_slot_extents& slots, const float pad) -> float {
	const float leading = (slots.icon > 0.f ? slots.icon + pad * 0.5f : 0.f) + (slots.dot > 0.f ? slots.dot + pad * 0.5f : 0.f);
	const float trailing = (slots.marker > 0.f ? slots.marker + pad * 0.5f : 0.f) + slots.close;
	return leading + trailing + pad * 3.f;
}

auto gse::gui::tab_extent(const resource::handle<font>& fnt, const style& sty, const tab_desc& tab, const float min_extent, const float max_extent) -> float {
	const float pad = sty.padding;
	const tab_slot_extents slots = slot_extents(fnt, sty, tab);
	const float reserved = reserved_extent(slots, pad);
	const float floor_extent = std::max(min_extent * sty.scale_factor, reserved + sty.font_size);
	return std::max(floor_extent, std::min(reserved + slots.caption, max_extent * sty.scale_factor));
}

auto gse::gui::tab_cell_slots(const resource::handle<font>& fnt, const style& sty, const tab_desc& tab, const rectf& cell) -> tab_slots {
	const float pad = sty.padding;
	const tab_slot_extents slots = slot_extents(fnt, sty, tab);
	const float center_y = cell.center().y();

	const auto leading_box = [&](float& x, const float extent) -> rectf {
		if (extent <= 0.f) {
			return {};
		}
		const rectf box = rectf::from_position_size({ x, center_y + extent * 0.5f }, { extent, extent });
		x += extent + pad * 0.5f;
		return box;
	};

	float x = cell.left() + pad;
	tab_slots out;
	out.icon = leading_box(x, slots.icon);
	out.dot = leading_box(x, slots.dot);

	const bool show_close = tab.busy || tab.closeable;
	out.close = show_close
		? rectf::from_position_size({ cell.right() - slots.close - pad, center_y + slots.close * 0.5f }, { slots.close, slots.close })
		: rectf{};

	const float close_zone = show_close ? slots.close + pad : pad;
	const float marker_zone = slots.marker > 0.f ? slots.marker + pad * 0.5f : 0.f;
	const float caption_right = cell.right() - close_zone - marker_zone;

	out.marker = slots.marker > 0.f
		? rectf::from_position_size({ caption_right + pad * 0.25f, center_y + slots.marker * 0.5f }, { slots.marker, slots.marker })
		: rectf{};
	out.caption = rectf::from_position_size({ x, cell.top() }, { std::max(0.f, caption_right - x), cell.height() });
	return out;
}

auto gse::gui::tab_strip_row_extent(const resource::handle<font>& fnt, const style& sty) -> float {
	return fnt.resolve()->line_height(sty.font_size) + sty.padding;
}

auto gse::gui::tab_strip_measure(const style& sty, const tab_strip_measure_params& params) -> tab_strip_metrics {
	const float tab_gap = sty.tab.gap;
	const float row_extent = tab_strip_row_extent(params.font, sty);
	const float available = std::max(0.f, params.available_extent - (params.show_add ? row_extent + tab_gap : 0.f));

	tab_strip_metrics metrics{
		.row_extent = row_extent,
		.row_gap = tab_gap,
		.scroll_bar_extent = sty.tab.scroll_bar_extent,
	};
	float row_width = 0.f;
	for (const tab_desc& tab : params.tabs) {
		const float w = tab_extent(params.font, sty, tab, params.min_tab_extent, params.max_tab_extent);
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
	const auto fnt_view = fnt.resolve();
	const float row_h = std::min(fnt_view->line_height(fs) + pad, area.height());
	const float tab_gap = sty.tab.gap;

	std::vector<float> widths;
	widths.reserve(tabs.size());
	for (const tab_desc& tab : tabs) {
		widths.push_back(tab_extent(fnt, sty, tab, min_tab_extent, max_tab_extent));
	}

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
				.slots = tab_cell_slots(fnt, sty, tabs[i], rect),
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
				.slots = tab_cell_slots(fnt, sty, tabs[i], rect),
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

	const resource::handle<font> fnt = ctx.fonts.face_or(params.font);
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

	rectf tab_area = params.area;

	const tab_skin& skin = sty.tab;

	const auto draw_cell = [&](const rectf& rect, const rectf& visible, const tab_slots& slots, const tab_desc& tab, const bool is_active, const bool hovered) {
		const interaction::press cell{
			.hovered = hovered,
			.selected = is_active,
		};
		const interaction::press caption{
			.enabled = !tab.dimmed,
			.hovered = hovered,
			.selected = is_active,
		};

		draw::fill_surface(ctx, rect, cell.select(skin.cell), {
			.clip_rect = visible,
		});

		const rectf content_clip = visible.intersection(rectf::from_position_size(
			{ rect.left(), rect.top() },
			{ std::max(0.f, slots.caption.right() - rect.left()), rect.height() }
		));
		const vec4f caption_color = caption.select(skin.caption_colors);

		if (slots.icon.width() > 0.f) {
			symbol::draw(ctx, tab.icon, slots.icon, {
				.color = tab.icon_color.value_or(caption_color),
				.extent = sty.icon_extent,
				.clip_rect = content_clip,
			});
		}

		if (slots.dot.width() > 0.f) {
			const vec2f dot_center = symbol::snap_to_pixel_center(slots.dot.center());
			ctx.queue_sprite({
				.rect = rectf::from_position_size(
					{ dot_center.x() - slots.dot.width() * 0.5f, dot_center.y() + slots.dot.height() * 0.5f },
					slots.dot.size()
				),
				.color = tab.error ? sty.color_error : sty.color_warning,
				.texture = ctx.blank_texture,
				.clip_rect = content_clip,
				.corner_radius = slots.dot.width() * 0.5f,
			});
		}

		ctx.queue_text({
			.font = fnt,
			.text = tab.caption,
			.position = { slots.caption.left(), rect.center().y() + fnt_view->vertical_center_offset(fs) },
			.scale = fs,
			.color = caption_color,
			.clip_rect = content_clip,
		});
		if (slots.marker.width() > 0.f) {
			ctx.queue_text({
				.font = fnt,
				.text = "*",
				.position = { slots.marker.left(), rect.center().y() + fnt_view->vertical_center_offset(fs) },
				.scale = fs,
				.color = cell.select(skin.caption_colors),
				.clip_rect = visible,
			});
		}
		const bool close_hovered = tab_area.contains(mouse) && ctx.hovers(slots.close);
		const interaction::press close_glyph{
			.hovered = close_hovered,
		};
		if (tab.busy && !(tab.closeable && close_hovered)) {
			symbol::spinner(ctx, slots.close, {
				.color = skin.glyph_colors.idle,
				.extent = sty.icon_extent,
				.clip_rect = visible,
			});
		}
		else if (tab.closeable) {
			symbol::draw(ctx, symbol::close(), slots.close, {
				.color = close_glyph.select(skin.glyph_colors),
				.extent = sty.icon_extent,
				.clip_rect = visible,
			});
		}
	};

	const auto draw_add_cell = [&](const rectf& cell) -> bool {
		const interaction::press add{
			.hovered = ctx.hovers(cell),
		};

		draw::fill_surface(ctx, cell, add.select(skin.cell), {});

		symbol::draw(ctx, symbol::plus(), cell, {
			.color = add.select(skin.glyph_colors),
			.extent = sty.icon_extent,
			.clip_rect = cell,
		});
		return add.hovered && ctx.mouse_pressed_for(cell);
	};

	if (params.orientation == tab_orientation::vertical) {
		const float cell_h = fnt_view->line_height(fs) + pad;
		const std::size_t add_cells = params.show_add ? 1 : 0;
		const float content_h = cell_h * static_cast<float>(params.tabs.size() + add_cells);
		const float max_scroll = std::max(0.f, content_h - area.height());

		if (ctx.hovers(area)) {
			const vec2f wheel = ctx.scroll_delta_for(area, scroll_axis_kind::vertical);
			if (std::abs(wheel.y()) > 0.001f) {
				state.scroll.offset = std::clamp(state.scroll.offset - wheel.y() * cell_h, 0.f, max_scroll);
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
			const tab_slots slots = tab_cell_slots(fnt, sty, tab, cell);

			if (is_active) {
				active_bar = rectf::from_position_size({ area.left(), visible.top() }, { sty.accent_bar_width, visible.height() });
				has_active = true;
			}
			draw_cell(cell, visible, slots, tab, is_active, hovered);

			if (hovered && ctx.mouse_pressed_for(visible)) {
				if (tab.closeable && slots.close.contains(mouse)) {
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
			draw::fill_surface(ctx, active_bar, skin.accent_bar, {});
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

	if (overflow && ctx.hovers(tab_area)) {
		const bool shift = ctx.key_held(key::left_shift) || ctx.key_held(key::right_shift);
		const bool wraps = params.overflow == tab_overflow::wrap;
		scroll_axes axes{ scroll_axis_kind::horizontal };
		if (shift || wraps) {
			axes.set(scroll_axis_kind::vertical);
		}

		const vec2f wheel = ctx.scroll_delta_for(tab_area, axes);
		if (wraps && std::abs(wheel.y()) > 0.001f && !shift) {
			if (wheel.y() > 0.f) {
				state.visible_rows = state.visible_rows > 1 ? state.visible_rows - 1 : 1;
			}
			else {
				++state.visible_rows;
			}
		}
		else if (std::abs(wheel.x()) > 0.001f || (shift && std::abs(wheel.y()) > 0.001f)) {
			state.scroll.offset -= (wheel.x() + wheel.y()) * skin.wheel_step;
			state.scroll.target = state.scroll.offset;
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
				const float w = tab_extent(fnt, sty, tab, params.min_tab_extent, params.max_tab_extent);
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
			if (tab.closeable && p.slots.close.contains(mouse)) {
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
		draw_cell(p.rect, visible, p.slots, tab, is_active, hovered);
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
			.min_thumb = sty.scrollbar.min_thumb,
		});
		if (bar.used_press) {
			ctx.consume_press(mouse_button::button_1);
		}

		const interaction::press scroll_state{
			.hovered = bar.hovered,
			.held = bar.held,
		};

		draw::fill_surface(ctx, bar.track_rect, sty.scrollbar.track, {});
		draw::fill_surface(ctx, bar.thumb_rect, scroll_state.select(sty.scrollbar.thumb), {});
	}

	if (params.show_add) {
		const rectf add_cell = rectf::from_position_size({ tab_area.right() + tab_gap, area.top() }, { row_h, row_h });
		result.add_requested = draw_add_cell(add_cell);
	}

	return result;
}
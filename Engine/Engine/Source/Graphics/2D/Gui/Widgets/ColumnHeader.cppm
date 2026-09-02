export module gse.graphics:column_header_widget;

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
import :font;
import :input_layers;
import :styles;
import :ui_renderer;
import :layout_ops;

export namespace gse::gui {
	struct column_state {
		std::vector<float> widths;
		std::vector<layout::halign> alignments;
		int resizing = -1;
	};

	struct column_header_params {
		rectf area;
		std::span<const std::string_view> captions;
		std::size_t active = 0;
		float min_width = 32.f;
		resource::handle<font> font{};
	};

	struct column_header_result {
		bool activated = false;
		std::size_t index = 0;
		bool resize_cursor = false;
	};

	[[nodiscard]] auto needs_seed(
		const column_state& state,
		std::size_t column_count
	) -> bool;

	auto seed_columns(
		const draw_context& ctx,
		std::span<const float> content_widths,
		column_state& state
	) -> void;

	[[nodiscard]] auto column_cell(
		const column_state& state,
		const rectf& row,
		std::size_t index
	) -> rectf;

	[[nodiscard]] auto column_align(
		const column_state& state,
		std::size_t index
	) -> layout::halign;

	[[nodiscard]] auto column_text_left(
		const draw_context& ctx,
		const column_state& state,
		const rectf& cell,
		std::size_t index,
		float text_width
	) -> float;

	auto column_header(
		const draw_context& ctx,
		const column_header_params& params,
		column_state& state
	) -> column_header_result;
}

namespace gse::gui {
	constexpr float column_grip_width = 6.f;
}

auto gse::gui::needs_seed(const column_state& state, const std::size_t column_count) -> bool {
	return state.widths.size() != column_count;
}

auto gse::gui::seed_columns(const draw_context& ctx, const std::span<const float> content_widths, column_state& state) -> void {
	if (!needs_seed(state, content_widths.size())) {
		return;
	}
	state.widths.assign(content_widths.begin(), content_widths.end());
	for (float& width : state.widths) {
		width += ctx.style.padding;
	}
}

auto gse::gui::column_cell(const column_state& state, const rectf& row, const std::size_t index) -> rectf {
	if (index >= state.widths.size()) {
		return {};
	}
	float left = row.right();
	for (std::size_t i = state.widths.size(); i > index; --i) {
		left -= state.widths[i - 1];
	}
	return rectf::from_position_size({ left, row.top() }, { state.widths[index], row.height() });
}

auto gse::gui::column_align(const column_state& state, const std::size_t index) -> layout::halign {
	return index < state.alignments.size() ? state.alignments[index] : layout::halign::end;
}

auto gse::gui::column_text_left(const draw_context& ctx, const column_state& state, const rectf& cell, const std::size_t index, const float text_width) -> float {
	const float pad = ctx.style.padding * 0.5f;
	switch (column_align(state, index)) {
		case layout::halign::center:
			return cell.center().x() - text_width * 0.5f;
		case layout::halign::end:
			return cell.right() - text_width - pad;
		case layout::halign::start:
			break;
	}
	return cell.left() + pad;
}

auto gse::gui::column_header(const draw_context& ctx, const column_header_params& params, column_state& state) -> column_header_result {
	const resource::handle<font>& fnt = params.font.valid() ? params.font : ctx.fonts.code;
	const auto fnt_view = fnt.resolve();
	if (!fnt.valid() || state.widths.size() != params.captions.size()) {
		return {};
	}

	const style& sty = ctx.style;
	const float fs = sty.font_size;
	const float pad = sty.padding;
	const vec2f mouse = ctx.mouse_position();
	const bool held = ctx.mouse_held();

	if (!held) {
		state.resizing = -1;
	}

	column_header_result result{};

	for (std::size_t i = 0; i < params.captions.size(); ++i) {
		const rectf cell = column_cell(state, params.area, i);
		if (cell.width() <= 0.f) {
			continue;
		}

		const rectf grip = rectf::from_position_size(
			{ cell.left() - column_grip_width * 0.5f, cell.top() },
			{ column_grip_width, cell.height() }
		);
		const auto index = static_cast<int>(i);

		if (state.resizing == index) {
			state.widths[i] = std::max(params.min_width * sty.scale_factor, cell.right() - mouse.x());
			result.resize_cursor = true;
		}
		else if (state.resizing < 0 && ctx.hovers(grip)) {
			result.resize_cursor = true;
			if (ctx.mouse_pressed_for(grip)) {
				state.resizing = index;
			}
		}

		if (ctx.hit_regions) {
			ctx.hit_regions->block_text_selection(grip);
		}

		const std::string_view caption = params.captions[i];
		const bool active = params.active == i;
		ctx.queue_text({
			.font = fnt,
			.text = caption,
			.position = {
				column_text_left(ctx, state, cell, i, fnt_view->width(caption, fs)),
				cell.center().y() + fnt_view->vertical_center_offset(fs),
			},
			.scale = fs,
			.color = active ? sty.color_accent : sty.color_text_secondary,
			.clip_rect = params.area,
		});

		if (state.resizing < 0 && !grip.contains(mouse) && ctx.clicked_in_rect(cell)) {
			result.activated = true;
			result.index = i;
		}
	}

	return result;
}

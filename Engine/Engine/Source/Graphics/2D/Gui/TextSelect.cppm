export module gse.graphics:text_select;

import std;

import gse.core;
import gse.math;
import gse.os;
import gse.assets;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;

import :types;
import :font;
import :ids;
import :input_layers;
import :render_layer;
import :styles;
import :texture;
import :ui_renderer;

export namespace gse::gui {
	struct selected_run {
		rectf rect;
		std::string_view text;
		render_layer layer = render_layer::content;
		std::uint32_t z_order = 0;
		std::optional<rectf> clip_rect;
		bool row_break = false;
	};

	struct text_selection_state {
		bool dragging = false;
		bool present = false;
		vec2f anchor;
		vec2f focus;
		std::vector<selected_run> runs;
		std::string menu_text;
	};

	struct text_selection_frame {
		text_selection_state& selection;
		const input::state& input;
		input_layer& layers;
		context_menu_state& context_menu;
		const font_set& fonts;
		const style& style;
		resource::handle<texture> blank_texture;
		std::span<const renderer::text_command> texts;
		std::vector<renderer::sprite_command>& sprites;
		rectf bounds;
		const state& gesture;
	};

	auto update_text_selection(
		const text_selection_frame& frame
	) -> void;

	[[nodiscard]] auto text_copy_menu_tag() -> id;

	[[nodiscard]] auto text_edit_menu_tag() -> id;

	[[nodiscard]] auto selection_text(
		const text_selection_state& state
	) -> std::string;
}

namespace gse::gui {
	struct laid_run {
		std::size_t index = 0;
		rectf rect;
		std::shared_ptr<const font> face;
	};

	struct run_position {
		std::size_t run = 0;
		std::size_t column = 0;
	};

	auto lay_out_runs(
		std::span<const renderer::text_command> texts,
		const font_set& fonts,
		render_layer layer,
		std::vector<laid_run>& out
	) -> void;

	[[nodiscard]] auto layer_for_point(
		std::span<const renderer::text_command> texts,
		const font_set& fonts,
		vec2f point
	) -> std::optional<render_layer>;

	[[nodiscard]] auto face_for(
		const renderer::text_command& cmd,
		const font_set& fonts
	) -> std::shared_ptr<const font>;

	[[nodiscard]] auto position_at(
		std::span<const laid_run> runs,
		std::span<const renderer::text_command> texts,
		vec2f point
	) -> run_position;

	[[nodiscard]] auto column_at(
		const laid_run& run,
		const renderer::text_command& cmd,
		float x
	) -> std::size_t;

	[[nodiscard]] auto same_row(
		const rectf& a,
		const rectf& b
	) -> bool;

	[[nodiscard]] auto row_contains(
		const rectf& rect,
		float y
	) -> bool;
}

auto gse::gui::text_copy_menu_tag() -> id {
	return ids::make_from_key(stable_id("gse_gui_text_copy_menu"));
}

auto gse::gui::text_edit_menu_tag() -> id {
	return ids::make_from_key(stable_id("gse_gui_text_edit_menu"));
}

auto gse::gui::same_row(const rectf& a, const rectf& b) -> bool {
	const float overlap = std::min(a.top(), b.top()) - std::max(a.bottom(), b.bottom());
	return overlap > std::min(a.height(), b.height()) * 0.5f;
}

auto gse::gui::row_contains(const rectf& rect, const float y) -> bool {
	return y <= rect.top() && y >= rect.bottom();
}

auto gse::gui::face_for(const renderer::text_command& cmd, const font_set& fonts) -> std::shared_ptr<const font> {
	const resource::handle<font> handle = cmd.font.valid() ? cmd.font : fonts.text;
	return handle.resolve();
}

auto gse::gui::lay_out_runs(const std::span<const renderer::text_command> texts, const font_set& fonts, const render_layer layer, std::vector<laid_run>& out) -> void {
	out.clear();
	for (std::size_t i = 0; i < texts.size(); ++i) {
		const renderer::text_command& cmd = texts[i];
		if (cmd.text.empty() || cmd.layer != layer) {
			continue;
		}
		const std::shared_ptr<const font> face = face_for(cmd, fonts);
		if (!face) {
			continue;
		}
		const rectf rect = rectf::from_position_size(
			cmd.position,
			{ face->width(cmd.text, cmd.scale), face->line_height(cmd.scale) }
		);
		if (cmd.clip_rect && !cmd.clip_rect->intersects(rect)) {
			continue;
		}
		out.push_back({
			.index = i,
			.rect = rect,
			.face = face,
		});
	}

	std::ranges::sort(out, [](const laid_run& a, const laid_run& b) {
		if (a.rect.top() != b.rect.top()) {
			return a.rect.top() > b.rect.top();
		}
		return a.rect.left() < b.rect.left();
	});
}

auto gse::gui::layer_for_point(const std::span<const renderer::text_command> texts, const font_set& fonts, const vec2f point) -> std::optional<render_layer> {
	auto raise = [](std::optional<render_layer>& slot, const render_layer layer) {
		if (!slot || static_cast<std::uint8_t>(layer) > static_cast<std::uint8_t>(*slot)) {
			slot = layer;
		}
	};

	std::optional<render_layer> on_row;
	std::optional<render_layer> in_panel;
	for (const renderer::text_command& cmd : texts) {
		if (cmd.text.empty()) {
			continue;
		}
		if (cmd.clip_rect && !cmd.clip_rect->contains(point)) {
			continue;
		}
		const std::shared_ptr<const font> face = face_for(cmd, fonts);
		if (!face) {
			continue;
		}
		const rectf band = rectf::from_position_size(cmd.position, { 0.f, face->line_height(cmd.scale) });
		if (row_contains(band, point.y())) {
			raise(on_row, cmd.layer);
		}
		else if (cmd.clip_rect) {
			raise(in_panel, cmd.layer);
		}
	}
	return on_row ? on_row : in_panel;
}

auto gse::gui::column_at(const laid_run& run, const renderer::text_command& cmd, const float x) -> std::size_t {
	const std::vector<float> offsets = run.face->caret_offsets(cmd.text, cmd.scale);
	const float local = x - run.rect.left();
	std::size_t best = 0;
	float best_dx = std::numeric_limits<float>::max();
	for (std::size_t k = 0; k < offsets.size(); ++k) {
		if (const float dx = std::abs(offsets[k] - local); dx < best_dx) {
			best_dx = dx;
			best = k;
		}
	}
	return best;
}

auto gse::gui::position_at(const std::span<const laid_run> runs, const std::span<const renderer::text_command> texts, const vec2f point) -> run_position {
	if (runs.empty()) {
		return {};
	}

	std::size_t row_first = runs.size();
	std::size_t row_last = runs.size();
	for (std::size_t i = 0; i < runs.size(); ++i) {
		if (!row_contains(runs[i].rect, point.y())) {
			continue;
		}
		row_first = std::min(row_first, i);
		row_last = i;
	}

	if (row_first == runs.size()) {
		std::size_t above = runs.size();
		for (std::size_t i = 0; i < runs.size(); ++i) {
			if (runs[i].rect.bottom() > point.y()) {
				above = i;
			}
		}
		if (above == runs.size()) {
			return { 0, 0 };
		}
		return { above, texts[runs[above].index].text.size() };
	}

	if (point.x() <= runs[row_first].rect.left()) {
		return { row_first, 0 };
	}
	if (point.x() >= runs[row_last].rect.right()) {
		return { row_last, texts[runs[row_last].index].text.size() };
	}

	for (std::size_t i = row_first; i <= row_last; ++i) {
		if (!row_contains(runs[i].rect, point.y())) {
			continue;
		}
		if (point.x() <= runs[i].rect.right()) {
			return { i, column_at(runs[i], texts[runs[i].index], point.x()) };
		}
	}

	return { row_last, texts[runs[row_last].index].text.size() };
}

auto gse::gui::selection_text(const text_selection_state& state) -> std::string {
	std::string out;
	for (const selected_run& run : state.runs) {
		if (!out.empty()) {
			out += run.row_break ? '\n' : ' ';
		}
		out += run.text;
	}
	return out;
}

auto gse::gui::update_text_selection(const text_selection_frame& frame) -> void {
	text_selection_state& state = frame.selection;
	constexpr float drag_slop = 3.f;

	const vec2f mouse = frame.input.mouse_position();
	const bool held = frame.input.mouse_button_held(mouse_button::button_1);

	auto discard = [&state] {
		state.dragging = false;
		state.present = false;
		state.runs.clear();
	};

	if (frame.input.mouse_button_pressed(mouse_button::button_1)) {
		const bool eligible = frame.bounds.contains(mouse)
			&& std::holds_alternative<states::idle>(frame.gesture.v)
			&& !frame.layers.text_selection_blocked_at(mouse)
			&& !frame.context_menu.open;
		discard();
		state.dragging = eligible;
		state.anchor = mouse;
		state.focus = mouse;
	}

	if (state.dragging) {
		state.focus = mouse;
		state.dragging = held;
	}

	if (!state.dragging && !state.present) {
		return;
	}

	if (const vec2f scroll = frame.input.scroll_delta(); scroll.x() != 0.f || scroll.y() != 0.f) {
		discard();
		return;
	}

	if (frame.input.key_pressed(key::escape)) {
		discard();
		return;
	}

	const bool right_pressed = state.present && frame.input.mouse_button_pressed(mouse_button::button_2);
	if (right_pressed && !std::ranges::any_of(state.runs, [mouse](const selected_run& run) { return run.rect.contains(mouse); })) {
		discard();
		return;
	}

	const vec2f travel = state.focus - state.anchor;
	if (state.dragging && std::abs(travel.x()) < drag_slop && std::abs(travel.y()) < drag_slop) {
		state.present = false;
		state.runs.clear();
		return;
	}

	const std::optional<render_layer> layer = layer_for_point(frame.texts, frame.fonts, state.anchor);
	if (!layer) {
		discard();
		return;
	}

	std::vector<laid_run> laid;
	lay_out_runs(frame.texts, frame.fonts, *layer, laid);
	if (laid.empty()) {
		discard();
		return;
	}

	run_position from = position_at(laid, frame.texts, state.anchor);
	run_position to = position_at(laid, frame.texts, state.focus);
	if (to.run < from.run || (to.run == from.run && to.column < from.column)) {
		std::swap(from, to);
	}

	state.runs.clear();
	for (std::size_t i = from.run; i <= to.run && i < laid.size(); ++i) {
		const renderer::text_command& cmd = frame.texts[laid[i].index];
		const std::size_t begin = i == from.run ? from.column : 0;
		const std::size_t end = i == to.run ? to.column : cmd.text.size();
		if (end <= begin) {
			continue;
		}
		const std::vector<float> offsets = laid[i].face->caret_offsets(cmd.text, cmd.scale);
		const float left = laid[i].rect.left() + offsets[begin];
		const float right = laid[i].rect.left() + offsets[end];
		state.runs.push_back({
			.rect = rectf::from_position_size({ left, laid[i].rect.top() }, { right - left, laid[i].rect.height() }),
			.text = cmd.text.substr(begin, end - begin),
			.layer = cmd.layer,
			.z_order = cmd.z_order,
			.clip_rect = cmd.clip_rect,
			.row_break = !state.runs.empty() && !same_row(laid[i - 1].rect, laid[i].rect),
		});
	}

	state.present = !state.runs.empty();

	for (const selected_run& run : state.runs) {
		frame.sprites.push_back({
			.rect = run.rect,
			.color = frame.style.color_selection,
			.texture = frame.blank_texture,
			.clip_rect = run.clip_rect,
			.layer = run.layer,
			.z_order = run.z_order,
		});
	}

	if (!state.present) {
		return;
	}

	const bool ctrl = frame.input.key_held(key::left_control) || frame.input.key_held(key::right_control);
	if (ctrl && frame.input.key_pressed(key::c) && !frame.layers.is_key_press_consumed(key::c)) {
		frame.layers.consume_key_press(key::c);
		window::set_clipboard_text(selection_text(state));
	}

	if (right_pressed && !frame.context_menu.open) {
		state.menu_text = selection_text(state);
		frame.context_menu.open = true;
		frame.context_menu.just_opened = true;
		frame.context_menu.position = mouse;
		frame.context_menu.items = {
			{
				.label = "Copy",
				.action_id = static_cast<std::uint32_t>(text_edit_action::copy),
			},
		};
		frame.context_menu.target = {};
		frame.context_menu.tag = text_copy_menu_tag();
	}
}

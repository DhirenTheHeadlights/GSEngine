module gse.graphics;

import std;

import :types;
import :font;
import :texture;
import :ui_renderer;
import :styles;
import :scroll_widget;
import :render_layer;

import gse.math;
import gse.core;
import gse.time;

gse::gui::menu::menu(std::string_view tag, const menu_data& data)
	: identifiable(tag),
	  identifiable_owned(data.parent_id),
	  rect(data.rect),
	  dock_split_ratio(data.dock_split_ratio),
	  docked_to(data.docked_to) {
	tab_contents.emplace_back(tag);
}

auto gse::gui::draw_context::queue_sprite(renderer::sprite_command cmd) const -> void {
	if (static_cast<std::uint8_t>(cmd.layer) < static_cast<std::uint8_t>(current_layer)) {
		cmd.layer = current_layer;
	}
	if (cmd.z_order == 0) {
		cmd.z_order = current_z_order;
	}
	if (!clip_stack.empty() && static_cast<std::uint8_t>(cmd.layer) <= static_cast<std::uint8_t>(render_layer::popup)) {
		const ui_rect& clip = clip_stack.back();
		cmd.clip_rect = cmd.clip_rect.has_value() ? cmd.clip_rect->intersection(clip) : clip;
	}
	sprites.push_back(std::move(cmd));
}

auto gse::gui::draw_context::queue_text(renderer::text_command cmd) const -> void {
	if (static_cast<std::uint8_t>(cmd.layer) < static_cast<std::uint8_t>(current_layer)) {
		cmd.layer = current_layer;
	}
	if (cmd.z_order == 0) {
		cmd.z_order = current_z_order;
	}
	if (!clip_stack.empty() && static_cast<std::uint8_t>(cmd.layer) <= static_cast<std::uint8_t>(render_layer::popup)) {
		const ui_rect& clip = clip_stack.back();
		cmd.clip_rect = cmd.clip_rect.has_value() ? cmd.clip_rect->intersection(clip) : clip;
	}
	texts.push_back(std::move(cmd));
}

auto gse::gui::draw_context::current_clip() const -> std::optional<ui_rect> {
	if (clip_stack.empty()) {
		return std::nullopt;
	}
	return clip_stack.back();
}

auto gse::gui::draw_context::input_available() const -> bool {
	return static_cast<std::uint8_t>(current_layer) >= static_cast<std::uint8_t>(input_layer);
}

auto gse::gui::draw_context::set_tooltip(const id& widget_id, const std::string& text) const -> void {
	if (!tooltip || text.empty()) {
		return;
	}

	tooltip->pending_widget_id = widget_id;
	tooltip->text = text;
	tooltip->position = input.mouse_position();
}

auto gse::gui::draw_context::next_row(const float height_multiplier) const -> ui_rect {
	if (!current_menu) {
		return {};
	}

	const float row_height =
		(font->line_height(style.font_size) + style.padding * style.widget_height_padding) * height_multiplier;
	const ui_rect content_rect = current_menu->rect.inset({ style.padding, style.padding });

	const ui_rect row =
		ui_rect::from_position_size({ content_rect.left(), layout_cursor.y() }, { content_rect.width(), row_height });

	layout_cursor.y() -= row_height + style.padding + style.item_spacing;
	return row;
}

auto gse::gui::draw_context::animated_color(const id& widget_id, const vec4f target, const float speed) const -> vec4f {
	const auto key = widget_id.number();
	auto it = widget_anim_colors.find(key);
	if (it == widget_anim_colors.end()) {
		widget_anim_colors[key] = target;
		return target;
	}

	const float dt = system_clock::dt<time>().as<seconds>();
	const float t = std::clamp(speed * dt, 0.f, 1.f);
	it->second = it->second + (target - it->second) * t;
	return it->second;
}

gse::gui::scroll_handle::scroll_handle(
	draw_context& ctx,
	scroll_state& state,
	const ui_rect& visible_rect,
	const float saved_layout_y,
	const scroll_config& config
) noexcept
	: m_ctx(&ctx),
	  m_state(&state),
	  m_visible_rect(visible_rect),
	  m_saved_layout_y(saved_layout_y),
	  m_content_start_y(visible_rect.top() + state.offset),
	  m_config(config),
	  m_active(true) {
	ctx.layout_cursor.y() = m_content_start_y;
	ctx.clip_stack.push_back(visible_rect);
}

gse::gui::scroll_handle::scroll_handle(scroll_handle&& other) noexcept
	: m_ctx(other.m_ctx),
	  m_state(other.m_state),
	  m_visible_rect(other.m_visible_rect),
	  m_saved_layout_y(other.m_saved_layout_y),
	  m_content_start_y(other.m_content_start_y),
	  m_config(other.m_config),
	  m_active(other.m_active) {
	other.m_active = false;
}

auto gse::gui::scroll_handle::operator=(scroll_handle&& other) noexcept -> scroll_handle& {
	if (this == &other) {
		return *this;
	}
	if (m_active) {
		run_scroll_end(*m_ctx, *m_state, m_visible_rect, m_content_start_y, m_config);
		m_ctx->clip_stack.pop_back();
		m_ctx->layout_cursor.y() = m_saved_layout_y - m_visible_rect.height() - m_ctx->style.padding;
	}
	m_ctx = other.m_ctx;
	m_state = other.m_state;
	m_visible_rect = other.m_visible_rect;
	m_saved_layout_y = other.m_saved_layout_y;
	m_content_start_y = other.m_content_start_y;
	m_config = other.m_config;
	m_active = other.m_active;
	other.m_active = false;
	return *this;
}

gse::gui::scroll_handle::~scroll_handle() noexcept {
	if (!m_active) {
		return;
	}
	run_scroll_end(*m_ctx, *m_state, m_visible_rect, m_content_start_y, m_config);
	m_ctx->clip_stack.pop_back();
	m_ctx->layout_cursor.y() = m_saved_layout_y - m_visible_rect.height() - m_ctx->style.padding;
}

auto gse::gui::scroll_handle::valid() const -> bool {
	return m_active;
}

auto gse::gui::scroll_handle::visible_rect() const -> const ui_rect& {
	return m_visible_rect;
}

auto gse::gui::scroll_handle::offset() const -> float {
	return m_state ? m_state->offset : 0.f;
}

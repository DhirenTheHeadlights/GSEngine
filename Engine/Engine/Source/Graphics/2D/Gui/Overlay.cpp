module gse.graphics:gui_overlay_impl;

import std;

import gse.os;
import gse.config;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.meta;
import gse.save;

import :gui;
import :gui_overlay;
import :gui_scale;

import :types;
import :layout;
import :font;
import :ui_renderer;
import :texture;
import :cursor;
import :save;
import :ids;
import :input_layers;
import :settings;
import :styles;
import :builder;
import :menu_stack;
import :render_layer;
import :interaction;
import :symbols;
import :tab_strip;
import :widget_context;

auto gse::gui::draw_drag_ghost(data& d, viewport_state& vp) -> void {
	if (!vp.active_drag_ghost || !d.fonts.text.valid()) {
		return;
	}

	const style& sty = vp.fstate.sty;
	const drag_ghost& state = *vp.active_drag_ghost;
	const auto text_view = d.fonts.text.resolve();
	const float label_w = text_view->width(state.label, sty.font_size);
	const float ghost_h = text_view->line_height(sty.font_size) + sty.padding;
	const float icon_w = state.detaching ? ghost_h : 0.f;
	const rectf ghost = rectf::from_position_size(
		{ state.position.x() + sty.padding, state.position.y() - sty.padding },
		{ label_w + icon_w + sty.padding * 2.f, ghost_h }
	);

	if (state.detaching) {
		d.sprite_commands.push_back({
			.rect = ghost.inset({ -1.f, -1.f }),
			.color = sty.color_accent,
			.texture = d.blank_texture,
			.layer = render_layer::overlay,
			.z_order = 19,
			.corner_radius = sty.corner_radius + 1.f,
		});
	}

	d.sprite_commands.push_back({
		.rect = ghost,
		.color = state.detaching ? sty.color_menu_body : sty.color_tab_active,
		.texture = d.blank_texture,
		.layer = render_layer::overlay,
		.z_order = 20,
		.corner_radius = sty.corner_radius,
	});

	if (state.detaching) {
		symbol::draw(d.sprite_commands, d.blank_texture, symbol::maximize(), rectf::from_position_size(ghost.top_left(), { icon_w, ghost_h }), {
			.color = sty.color_accent,
			.extent = sty.icon_extent,
			.layer = render_layer::overlay,
			.z_order = 21,
			.clip_rect = ghost,
		});
	}

	d.text_commands.push_back({
		.font = d.fonts.text,
		.text = intern_text(d, state.label),
		.position = { ghost.left() + icon_w + sty.padding, ghost.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_text,
		.clip_rect = ghost,
		.layer = render_layer::overlay,
		.z_order = 21,
	});
}

auto gse::gui::update_tooltip(data& d, viewport_state& vp) -> void {
	if (vp.tooltip.pending_widget_id.exists()) {
		if (vp.tooltip.pending_widget_id == vp.tooltip.widget_id) {
			vp.tooltip.hover_time += system_clock::dt<time>();
		}
		else {
			vp.tooltip.widget_id = vp.tooltip.pending_widget_id;
			vp.tooltip.hover_time = time{};
		}
	}
	else {
		vp.tooltip.widget_id.reset();
		vp.tooltip.hover_time = time{};
		vp.tooltip.text.clear();
	}

	if (vp.tooltip.widget_id.exists() && vp.tooltip.hover_time >= tooltip_state::show_delay && !vp.tooltip.text.empty() && d.fonts.text.valid()) {
		const float padding = vp.fstate.sty.padding;
		const float font_size = vp.fstate.sty.font_size;
		const auto text_view = d.fonts.text.resolve();
		const float line_height = text_view->line_height(font_size);

		auto lines = std::views::split(std::string_view(vp.tooltip.text), '\n');
		float text_width = 0.f;
		std::size_t line_count = 0;
		for (const auto line : lines) {
			text_width = std::max(text_width, text_view->width(std::string_view(line), font_size));
			++line_count;
		}

		const float tooltip_width = text_width + padding * 2.f;
		const float tooltip_height = line_height * static_cast<float>(line_count) + padding;

		vec2f tooltip_pos = vp.tooltip.position + vec2f(15.f, -15.f);

		if (tooltip_pos.x() + tooltip_width > vp.frame_rect.right()) {
			tooltip_pos.x() = vp.frame_rect.right() - tooltip_width;
		}
		if (tooltip_pos.x() < vp.frame_rect.left()) {
			tooltip_pos.x() = vp.frame_rect.left();
		}
		if (tooltip_pos.y() > vp.frame_rect.top()) {
			tooltip_pos.y() = vp.frame_rect.top();
		}
		if (tooltip_pos.y() - tooltip_height < vp.frame_rect.bottom()) {
			tooltip_pos.y() = vp.frame_rect.bottom() + tooltip_height;
		}

		const rectf tooltip_rect = rectf::from_position_size(
			tooltip_pos,
			{ tooltip_width, tooltip_height }
		);

		d.sprite_commands.push_back({
			.rect = tooltip_rect,
			.color = vp.fstate.sty.color_menu_body,
			.texture = d.blank_texture,
			.layer = render_layer::modal,
			.z_order = 100
		});

		d.sprite_commands.push_back({
			.rect = tooltip_rect.inset({ -1.f, -1.f }),
			.color = vp.fstate.sty.color_border,
			.texture = d.blank_texture,
			.layer = render_layer::modal,
			.z_order = 99
		});

		float line_center_y = tooltip_rect.top() - padding * 0.5f - line_height * 0.5f;
		for (const auto line : lines) {
			d.text_commands.push_back({
				.font = d.fonts.text,
				.text = intern_text(d, std::string_view(line)),
				.position = { tooltip_rect.left() + padding, line_center_y + text_view->vertical_center_offset(font_size) },
				.scale = font_size,
				.color = vp.fstate.sty.color_text,
				.layer = render_layer::modal,
				.z_order = 100
			});
			line_center_y -= line_height;
		}
	}

	vp.tooltip.pending_widget_id.reset();
}

auto gse::gui::process_context_menu(data& d, viewport_state& vp, const gse::input::state& input_state, const channel_write<ui_focus_request, popout_toggle, set_cursor_shape_request, renderer::sprite_command, renderer::text_command, context_menu_result, window_close_request, window_minimize_request, window_toggle_maximize_request, window_chrome_metrics_request, window_panel_drag_request> channels) -> void {
	context_menu_state& cm = vp.context_menu;
	if (!cm.open) {
		return;
	}
	if (!d.fonts.text.valid() || cm.items.empty()) {
		cm.open = false;
		return;
	}

	const style& sty = vp.fstate.sty;
	const vec2f mouse = input_state.mouse_position();
	constexpr render_layer layer = render_layer::popup;
	constexpr std::uint32_t base_z = 4000;

	const auto text_view = d.fonts.text.resolve();
	const float row_h = text_view->line_height(sty.font_size) + sty.padding * 0.5f;
	const float sep_h = sty.padding * 0.5f;

	float max_label = 0.f;
	for (const menu_item& it : cm.items) {
		max_label = std::max(max_label, text_view->width(it.label, sty.font_size));
	}
	const bool any_icon = std::ranges::any_of(cm.items, [](const menu_item& it) { return it.icon != nullptr; });
	const float icon_col = any_icon ? sty.font_size : 0.f;
	const float width = max_label + icon_col + sty.padding * 4.f;

	float total_h = sty.padding;
	for (const menu_item& it : cm.items) {
		total_h += (it.separator_before ? sep_h : 0.f) + row_h;
	}

	vec2f pos = cm.position;
	const float min_x = vp.frame_rect.left() + 2.f;
	const float min_y = vp.frame_rect.bottom() + total_h + 2.f;
	pos.x() = std::clamp(pos.x(), min_x, std::max(min_x, vp.frame_rect.right() - width - 2.f));
	pos.y() = std::clamp(pos.y(), min_y, std::max(min_y, vp.frame_rect.top() - 2.f));

	const rectf panel = rectf::from_position_size(pos, { width, total_h });

	d.sprite_commands.push_back({
		.rect = rectf::from_position_size({ pos.x() + 4.f, pos.y() - 4.f }, { width, total_h }),
		.color = sty.color_shadow,
		.texture = d.blank_texture,
		.layer = layer,
		.z_order = base_z,
		.corner_radius = sty.corner_radius_menu,
	});
	d.sprite_commands.push_back({
		.rect = panel,
		.color = { vec3f(sty.color_menu_body), 1.0f },
		.texture = d.blank_texture,
		.layer = layer,
		.z_order = base_z + 1,
		.corner_radius = sty.corner_radius_menu,
	});

	float y = panel.top() - sty.padding * 0.5f;
	bool selected = false;
	const bool left_pressed = input_state.mouse_button_pressed(mouse_button::button_1);
	const bool left_released = input_state.mouse_button_released(mouse_button::button_1);
	const std::uint64_t target_key = std::visit([](const auto& target) -> std::uint64_t {
		using target_type = std::remove_cvref_t<decltype(target)>;
		if constexpr (std::same_as<target_type, std::monostate>) {
			return stable_id("context_menu_no_target");
		}
		else if constexpr (std::same_as<target_type, id>) {
			return hash_combine(stable_id("context_menu_id_target"), target.number());
		}
		else {
			return hash_combine(stable_id("context_menu_numeric_target"), target);
		}
	}, cm.target);
	for (std::size_t i = 0; i < cm.items.size(); ++i) {
		const menu_item& it = cm.items[i];
		if (it.separator_before) {
			d.sprite_commands.push_back({
				.rect = rectf::from_position_size({ panel.left() + sty.padding, y - sep_h * 0.5f }, { width - sty.padding * 2.f, 1.f }),
				.color = sty.color_separator,
				.texture = d.blank_texture,
				.layer = layer,
				.z_order = base_z + 2,
			});
			y -= sep_h;
		}

		const rectf row = rectf::from_position_size({ panel.left(), y }, { width, row_h });
		const bool hovered = row.contains(mouse) && it.enabled;
		const id row_id = ids::make_from_key(hash_combine(
			hash_combine(cm.tag.number(), target_key),
			hash_combine(stable_id("context_menu_row"), static_cast<std::uint64_t>(i))
		));
		const bool activated = interaction::activate_on_click(vp.active_widget_id, row_id, hovered, hovered && left_pressed, left_released);

		if (hovered) {
			d.sprite_commands.push_back({
				.rect = rectf::from_position_size({ panel.left() + 3.f, y }, { width - 6.f, row_h }),
				.color = sty.color_widget_hovered,
				.texture = d.blank_texture,
				.layer = layer,
				.z_order = base_z + 2,
				.corner_radius = sty.corner_radius,
			});
		}

		const vec4f text_color = !it.enabled
			? sty.color_text_disabled
			: (it.destructive ? vec4f{ 0.92f, 0.45f, 0.45f, 1.f } : sty.color_text);
		if (it.icon) {
			symbol::draw(d.sprite_commands, d.blank_texture, it.icon(), rectf::from_position_size({ row.left() + sty.padding, y }, { sty.font_size, row_h }), {
				.color = text_color,
				.extent = sty.icon_extent,
				.layer = layer,
				.z_order = base_z + 3,
			});
		}
		d.text_commands.push_back({
			.font = d.fonts.text,
			.text = intern_text(d, it.label),
			.position = { row.left() + sty.padding * 1.5f + icon_col, row.center().y() + text_view->vertical_center_offset(sty.font_size) },
			.scale = sty.font_size,
			.color = text_color,
			.clip_rect = row,
			.layer = layer,
			.z_order = base_z + 3,
		});

		if (activated) {
			channels.push<context_menu_result>({
				.tag = cm.tag,
				.action_id = it.action_id,
				.target = cm.target,
			});
			selected = true;
		}

		y -= row_h;
	}

	bool dismiss = selected;
	if (cm.just_opened) {
		cm.just_opened = false;
	}
	else {
		const bool outside_press =
			(input_state.mouse_button_pressed(mouse_button::button_1) || input_state.mouse_button_pressed(mouse_button::button_2))
			&& !panel.contains(mouse);
		if (outside_press || input_state.key_pressed(key::escape)) {
			dismiss = true;
		}
	}

	if (dismiss) {
		cm.open = false;
		cm.just_opened = false;
		cm.items.clear();
	}
}

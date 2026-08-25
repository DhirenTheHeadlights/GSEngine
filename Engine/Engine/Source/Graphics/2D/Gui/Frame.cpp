module gse.graphics:gui_frame_impl;

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
import :gui_frame;
import :gui_chrome;
import :gui_drag_resize;
import :gui_menu;
import :gui_overlay;
import :gui_scale;
import :gui_screen;

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

auto gse::gui::migrate_menu(viewport_state& from, viewport_state& to, const std::string_view menu_name) -> bool {
	if (&from == &to) {
		return false;
	}

	const auto it = from.name_to_menu_id.find(stable_id(menu_name));
	if (it == from.name_to_menu_id.end()) {
		return false;
	}

	const id menu_id = it->second;
	if (to.menus.contains(menu_id)) {
		return false;
	}

	std::optional<menu> moved = from.menus.pop(menu_id);
	if (!moved) {
		return false;
	}

	if (from.current_menu && from.current_menu->id() == menu_id) {
		from.current_menu = nullptr;
	}
	std::erase(from.visible_menu_ids_last_frame, menu_id);
	std::erase_if(from.name_to_menu_id, [&](const auto& entry) { return entry.second == menu_id; });

	moved->z_order = 0;
	if (to.window.exists()) {
		moved->rect = to.frame_rect;
		moved->fixed = true;
	}
	to.menus.add(menu_id, std::move(*moved));
	return true;
}

auto gse::gui::adopt_menu(data& d, viewport_state& to, const std::string_view menu_name) -> bool {
	if (migrate_menu(d.primary, to, menu_name)) {
		return true;
	}
	for (const auto& vp : d.secondaries) {
		if (migrate_menu(*vp, to, menu_name)) {
			return true;
		}
	}
	return false;
}

auto gse::gui::reclaim_menus(data& d, viewport_state& from) -> void {
	for (const id& menu_id : std::vector<id>(from.menus.ids().begin(), from.menus.ids().end())) {
		if (d.primary.menus.contains(menu_id)) {
			continue;
		}
		if (std::optional<menu> moved = from.menus.pop(menu_id)) {
			moved->z_order = 0;
			d.primary.menus.add(menu_id, std::move(*moved));
		}
	}
}

auto gse::gui::viewport_for_window(data& d, const id window) -> viewport_state* {
	if (!window.exists()) {
		return &d.primary;
	}
	const auto it = std::ranges::find(d.secondaries, window, [](const auto& vp) { return vp->window; });
	return it == d.secondaries.end() ? nullptr : it->get();
}

auto gse::gui::close_window_viewport(data& d, const id window) -> void {
	const auto it = std::ranges::find(d.secondaries, window, [](const auto& vp) { return vp->window; });
	if (it == d.secondaries.end()) {
		return;
	}

	reclaim_menus(d, **it);
	d.secondaries.erase(it);

	d.primary.active_dock_space.reset();
	d.primary.active_drag_ghost.reset();
}

auto gse::gui::route_cursor(data& d, const vec2f mouse, const id focused_window, const id cursor_window) -> void {
	auto holds_capture = [](const viewport_state& vp) {
		return vp.menu_stack.captures_input() || !std::holds_alternative<states::idle>(vp.current_state.v);
	};

	viewport_state* owner = viewport_for_window(d, cursor_window);
	const viewport_state* keyboard_owner = viewport_for_window(d, focused_window);

	if (holds_capture(d.primary)) {
		owner = &d.primary;
	}
	for (const auto& vp : d.secondaries) {
		if (holds_capture(*vp)) {
			owner = vp.get();
			break;
		}
	}

	if (!owner) {
		for (const auto& vp : d.secondaries) {
			if (!vp->window.exists() && vp->frame_rect.contains(mouse)) {
				owner = vp.get();
				break;
			}
		}
	}

	if (!owner) {
		owner = &d.primary;
	}

	if (!keyboard_owner) {
		keyboard_owner = owner;
	}

	d.primary.owns_cursor = owner == &d.primary;
	d.primary.owns_keyboard = keyboard_owner == &d.primary;
	for (const auto& vp : d.secondaries) {
		vp->owns_cursor = owner == vp.get();
		vp->owns_keyboard = keyboard_owner == vp.get();
	}
}

auto gse::gui::begin_viewport_frame(data& d, viewport_state& vp, const shared_view<window::data> window_s, const vec2f viewport_size) -> void {
	vp.display_scale = window_s.primary.content_scale;
	sync_monitor_scale(d, vp, window_s.primary.monitor_key);

	const float current_scale_factor = scale_factor_for(d, vp, viewport_size.y());

	if (vp.previous_viewport_size.x() > 0.f && vp.previous_viewport_size.y() > 0.f && vp.previous_scale_factor > 0.f) {
		const bool viewport_changed = viewport_size.x() != vp.previous_viewport_size.x() || viewport_size.y() != vp.previous_viewport_size.y();
		const bool scale_changed = current_scale_factor != vp.previous_scale_factor;

		if (viewport_size.x() > 0.f && viewport_size.y() > 0.f && (viewport_changed || scale_changed)) {
			const float old_usable_height = vp.previous_viewport_size.y();
			const float new_usable_height = viewport_size.y();

			const float size_scale = current_scale_factor / vp.previous_scale_factor;

			const float top_inset = d.reserve_top_bar ? vp.fstate.sty.title_bar_height : 0.f;
			const float new_content_height = std::max(0.f, new_usable_height - top_inset);
			const rectf new_screen_rect = rectf::from_position_size(
				{ 0.f, new_content_height },
				{ viewport_size.x(), new_content_height }
			);

			for (menu& m : vp.menus.items()) {
				if (!m.owner_id().exists()) {
					if (m.docked_to != dock::location::none) {
						if (m.docked_to == dock::location::center) {
							m.rect = new_screen_rect;
						}
						else {
							m.rect = layout::dock_target_rect(new_screen_rect, m.docked_to, m.dock_split_ratio);
						}
					}
					else {
						const float ratio_x = m.rect.left() / vp.previous_viewport_size.x();
						const float ratio_y = (vp.previous_viewport_size.y() - m.rect.top()) / old_usable_height;

						const float new_left = ratio_x * viewport_size.x();
						const float new_top = viewport_size.y() - (ratio_y * new_usable_height);

						const float new_width = m.rect.width() * size_scale;
						const float new_height = m.rect.height() * size_scale;

						const float actual_width = std::min(new_width, viewport_size.x());
						const float actual_height = std::min(new_height, new_usable_height);

						const float clamped_left =
							std::clamp(
								new_left,
								0.f,
								std::max(0.f, viewport_size.x() - actual_width)
							);
						const float clamped_top = std::clamp(new_top, actual_height, new_usable_height);

						m.rect = rectf::from_position_size(
							{ clamped_left, clamped_top },
							{ actual_width, actual_height }
						);
					}

					layout::update(vp.menus, m.id());
				}
			}

			vp.previous_viewport_size = viewport_size;
			vp.previous_scale_factor = current_scale_factor;
		}
	}
	else {
		vp.previous_viewport_size = viewport_size;
		vp.previous_scale_factor = current_scale_factor;
	}

	vp.fstate = {};

	vp.input_layers_data.begin_frame();

	const style frame_sty = apply_scale(d, vp, d.style_override.value_or(style::from_theme(d.current_theme)), viewport_size.y());

	vp.fstate = {
		.sty = frame_sty,
		.active = d.fonts.text.valid()
	};

	vp.rect = usable_screen_rect(d.reserve_top_bar ? frame_sty.title_bar_height : 0.f, vp.frame_rect);

	vp.hot_widget_id = {};

	vp.input_layer_render = (vp.menu_stack.captures_input() || vp.context_menu.open) ? render_layer::popup : render_layer::content;
	vp.input_suppressed = window_s.primary.cursor_captured || !vp.owns_cursor;

	vp.name_to_menu_id.clear();
	for (menu& m : vp.menus.items()) {
		m.was_begun_this_frame = false;
		m.chrome_drawn_this_frame = false;
		for (const std::string& tab : m.tab_contents) {
			vp.name_to_menu_id.emplace(stable_id(tab), m.id());
		}
		vp.name_to_menu_id.emplace(stable_id(m.id().tag()), m.id());
	}
}

auto gse::gui::update_viewport_interaction(data& d, viewport_state& vp, const shared_view<window::data> window_s, const shared_view<input::data> input_state) -> void {
	if (!vp.owns_cursor) {
		vp.current_state = states::idle{};
		return;
	}

	const vec2f mouse_position = input::current_state(input_state).mouse_position();
	const bool mouse_held = input::current_state(input_state).mouse_button_held(mouse_button::button_1);
	const style& frame_sty = vp.fstate.sty;

	match(vp.current_state.v)
		.if_is([&](const states::idle&) {
			vp.current_state = handle_idle_state(
				d.fonts,
				vp,
				input::current_state(input_state),
				mouse_position,
				mouse_held,
				frame_sty
			);
		})
		.else_if_is([&](const states::dragging& st) {
			vp.current_state = handle_dragging_state(vp, st, window_s, mouse_position, mouse_held);
		})
		.else_if_is([&](const states::resizing& st) {
			vp.current_state = handle_resizing_state(vp, st, mouse_position, mouse_held, frame_sty, window_s);
		})
		.else_if_is([&](const states::resizing_divider& st) {
			vp.current_state = handle_resizing_divider_state(vp, st, mouse_position, mouse_held, frame_sty);
		})
		.else_if_is([&](const states::pending_drag& st) {
			vp.current_state = handle_pending_drag_state(vp, st, mouse_position, mouse_held);
		})
		.otherwise([&] {
			vp.current_state = states::idle{};
		});
}

auto gse::gui::update_viewport(data& d, viewport_state& vp, const input::state& input_st, const channel_read<push_screen_request, pop_screen_request, clear_screens_request, set_manual_cursor_request, menu_content, popout_closed> requests_in, const channel_write<ui_focus_request, popout_toggle, set_cursor_shape_request, renderer::sprite_command, renderer::text_command, context_menu_result, window_close_request, window_minimize_request, window_toggle_maximize_request, window_chrome_metrics_request> ui_out) -> void {
	const vec2f viewport_size = vp.frame_rect.size();

	if (vp.active_dock_space) {
		draw_dock_space(d, vp.fstate.sty, *vp.active_dock_space);
	}

	draw_drag_ghost(d, vp);

	if (!vp.menu_stack.empty()) {
		process_screen(d, vp, input_st, viewport_size);
	}

	ui_out.push<window_chrome_metrics_request>({
		.window = vp.window,
		.caption_height = static_cast<int>(vp.chrome.caption_height),
		.controls_width = static_cast<int>(vp.chrome.controls_width),
		.resize_exclude_y0 = vp.chrome.resize_exclude_y0,
		.resize_exclude_y1 = vp.chrome.resize_exclude_y1,
	});

	const bool occluded = vp.menu_stack.occludes();
	for (const auto& content : requests_in.of<menu_content>()) {
		if (occluded) {
			continue;
		}
		process_menu(d, vp, input_st, content.menu, content.layer, content.build);
	}

	for (const id& menu_id : vp.pending_popout_close_ids) {
		const menu* m = vp.menus.try_get(menu_id);
		if (!m) {
			continue;
		}
		const std::string_view category = popout_category_from_tag(m->id().tag());
		if (!category.empty()) {
			ui_out.push<popout_toggle>({ .category = std::string(category) });
		}
	}
	vp.pending_popout_close_ids.clear();

	if (const std::optional<caption_action> action = std::exchange(vp.pending_caption_action, std::nullopt)) {
		switch (*action) {
			case caption_action::minimize:
				ui_out.push<window_minimize_request>({ .window = vp.window });
				break;
			case caption_action::toggle_maximize:
				ui_out.push<window_toggle_maximize_request>({ .window = vp.window });
				break;
			case caption_action::close:
				ui_out.push<window_close_request>({ .window = vp.window });
				break;
		}
	}

	for (const auto& req : requests_in.of<popout_closed>()) {
		remove_tab_from_host(vp, req.menu_name);
	}

	if (vp.pending_tab_close.has_value()) {
		const auto [host_id, tab_index] = *vp.pending_tab_close;
		const menu* host = vp.menus.try_get(host_id);
		if (!host || !host->fixed) {
			vp.pending_tab_close.reset();
			if (host && tab_index < host->tab_contents.size()) {
				const std::string tab_name = host->tab_contents[tab_index];
				if (const std::string_view category = popout_category_from_tag(tab_name); !category.empty()) {
					ui_out.push<popout_toggle>({ .category = std::string(category) });
				}
				else {
					remove_tab_from_host(vp, tab_name);
				}
			}
		}
	}

	process_context_menu(d, vp, input_st, ui_out);

	update_tooltip(d, vp);

	vp.visible_menu_ids_last_frame.clear();
	vp.visible_menu_ids_last_frame.reserve(vp.menus.items().size());
	for (menu& m : vp.menus.items()) {
		m.was_visible_last_frame = m.was_begun_this_frame;
		if (m.was_begun_this_frame) {
			vp.visible_menu_ids_last_frame.push_back(m.id());
		}
	}

	for (menu& a : vp.menus.items()) {
		if (!a.was_visible_last_frame || a.docked_to != dock::location::none) {
			continue;
		}
		for (const menu& b : vp.menus.items()) {
			if (&a == &b || !b.was_visible_last_frame) {
				continue;
			}
			if (a.z_order < b.z_order &&
				b.rect.contains(a.rect.min()) && b.rect.contains(a.rect.max()) &&
				b.rect.width() * b.rect.height() > a.rect.width() * a.rect.height()) {
				a.z_order = ++vp.next_z_order;
				break;
			}
		}
	}
}
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

auto gse::gui::begin_viewport_frame(data& d, viewport_state& vp, const shared_view<window::data> window_s, const vec2f viewport_size) -> void {
	vp.display_scale = window_s.content_scale;
	sync_monitor_scale(d, vp, window_s.monitor_key);

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
	d.sprite_commands.clear();
	d.text_commands.clear();
	d.text_pool_slot ^= 1;
	d.text_pool_used = 0;

	vp.input_layers_data.begin_frame();

	const style frame_sty = apply_scale(d, vp, style::from_theme(d.current_theme), viewport_size.y());

	vp.fstate = {
		.sty = frame_sty,
		.active = d.fonts.text.valid()
	};

	vp.rect = usable_screen_rect(d.reserve_top_bar ? frame_sty.title_bar_height : 0.f, window_s);

	vp.hot_widget_id = {};

	vp.input_layer_render = (vp.menu_stack.captures_input() || vp.context_menu.open) ? render_layer::popup : render_layer::content;
	vp.input_suppressed = window_s.cursor_captured;

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

auto gse::gui::update_viewport(data& d, viewport_state& vp, const input::state& input_st, const vec2f viewport_size, const channel_read<push_screen_request, pop_screen_request, clear_screens_request, set_manual_cursor_request, menu_content, popout_closed> requests_in, const channel_write<ui_focus_request, popout_toggle, set_cursor_shape_request, renderer::sprite_command, renderer::text_command, context_menu_result, window_close_request, window_minimize_request, window_toggle_maximize_request, window_chrome_metrics_request> ui_out) -> void {
	if (vp.active_dock_space) {
		draw_dock_space(d, vp.fstate.sty, *vp.active_dock_space, input_st.mouse_position());
	}

	draw_drag_ghost(d, vp);

	if (!vp.menu_stack.empty()) {
		process_screen(d, vp, input_st, viewport_size, ui_out);
	}

	ui_out.push<ui_focus_request>({
		.focus = !vp.menu_stack.empty() || vp.manual_cursor,
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

	for (const auto& req : requests_in.of<popout_closed>()) {
		remove_tab_from_host(vp, req.menu_name);
	}

	if (vp.pending_tab_close.has_value()) {
		const auto [host_id, tab_index] = *vp.pending_tab_close;
		vp.pending_tab_close.reset();
		if (const menu* host = vp.menus.try_get(host_id); host && tab_index < host->tab_contents.size()) {
			const std::string tab_name = host->tab_contents[tab_index];
			if (const std::string_view category = popout_category_from_tag(tab_name); !category.empty()) {
				ui_out.push<popout_toggle>({ .category = std::string(category) });
			}
			else {
				remove_tab_from_host(vp, tab_name);
			}
		}
	}

	process_context_menu(d, vp, input_st, viewport_size, ui_out);

	update_tooltip(d, vp, viewport_size);

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
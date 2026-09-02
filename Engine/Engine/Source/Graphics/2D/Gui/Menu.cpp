module gse.graphics:gui_menu_impl;

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
import :gui_menu;
import :gui_chrome;
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

auto gse::gui::hosts_menu(const viewport_state& vp, const std::string_view name) -> bool {
	return vp.name_to_menu_id.contains(stable_id(name));
}

auto gse::gui::claims_content(const data& d, const viewport_state& vp, const std::string_view name) -> bool {
	if (hosts_menu(vp, name)) {
		return true;
	}
	if (!vp.adopts_unclaimed_content) {
		return false;
	}
	for (const auto& other : d.secondaries) {
		if (hosts_menu(*other, name)) {
			return false;
		}
	}
	return true;
}

auto gse::gui::begin_menu(viewport_state& vp, const std::string& name) -> bool {
	const std::uint64_t name_key = stable_id(name);
	if (const auto it = vp.name_to_menu_id.find(name_key); it != vp.name_to_menu_id.end()) {
		if (menu* m = vp.menus.try_get(it->second)) {
			vp.current_menu = m;
			vp.current_menu->was_begun_this_frame = true;
			vp.current_scope = std::make_unique<ids::scope>(vp.current_menu->id().number());
			return true;
		}
	}

	if (!vp.adopts_unclaimed_content) {
		return false;
	}

	menu new_menu(
		name,
		menu_data{
			.rect = rectf({
				.min = { 100.f, 100.f },
				.max = { 400.f, 300.f }
			}),
			.parent_id = id()
		}
	);

	const id new_id = new_menu.id();
	vp.menus.add(new_id, std::move(new_menu));

	if (menu* menu_ptr = vp.menus.try_get(new_id)) {
		vp.current_menu = menu_ptr;
		vp.current_menu->was_begun_this_frame = true;
		vp.current_scope = std::make_unique<ids::scope>(vp.current_menu->id().number());
		for (const std::string& tab : menu_ptr->tab_contents) {
			vp.name_to_menu_id.emplace(stable_id(tab), new_id);
		}
		vp.name_to_menu_id.emplace(stable_id(menu_ptr->id().tag()), new_id);
		return true;
	}

	return false;
}

auto gse::gui::end_menu(viewport_state& vp) -> void {
	vp.current_scope.reset();
	vp.current_menu = nullptr;
}

auto gse::gui::calculate_display_rect(viewport_state& vp, const menu& m) -> rectf {
	rectf display_rect = m.rect;

	for (const menu& child : vp.menus.items()) {
		if (child.owner_id() == m.id() && !child.was_begun_this_frame && child.was_visible_last_frame) {
			display_rect = rectf::bounding_box(display_rect, calculate_display_rect(vp, child));
		}
	}

	return display_rect;
}

auto gse::gui::process_menu(data& d, viewport_state& vp, const input::state& input_state, const std::string& name, const render_layer layer, const std::function<void(builder&)>& build) -> void {
	if (!vp.fstate.active) {
		return;
	}

	if (vp.suppressed_menus.contains(stable_id(name))) {
		return;
	}

	if (!claims_content(d, vp, name)) {
		return;
	}

	if (!begin_menu(vp, name)) {
		return;
	}

	menu& current_menu = *vp.current_menu;
	if (current_menu.z_order == 0) {
		current_menu.z_order = vp.next_z_order++;
	}
	const std::uint32_t menu_z = current_menu.z_order;
	const std::size_t sprite_start = d.sprite_commands.size();
	const std::size_t text_start = d.text_commands.size();
	auto stamp_z = [&] {
		for (std::size_t i = sprite_start; i < d.sprite_commands.size(); ++i) {
			if (d.sprite_commands[i].z_order == 0) {
				d.sprite_commands[i].z_order = menu_z;
			}
		}
		for (std::size_t i = text_start; i < d.text_commands.size(); ++i) {
			if (d.text_commands[i].z_order == 0) {
				d.text_commands[i].z_order = menu_z;
			}
		}
	};

	if (!current_menu.chrome_drawn_this_frame) {
		draw_menu_chrome(d, vp, input_state, current_menu, layer);
		current_menu.chrome_drawn_this_frame = true;
	}

	const auto it = std::ranges::find(current_menu.tab_contents, name);
	const bool is_active_tab = (it != current_menu.tab_contents.end()) &&
		(std::distance(current_menu.tab_contents.begin(), it) ==
		 static_cast<std::ptrdiff_t>(current_menu.active_tab_index));

	if (!is_active_tab) {
		stamp_z();
		end_menu(vp);
		return;
	}

	const style& sty = vp.fstate.sty;
	const rectf display_rect = calculate_display_rect(vp, current_menu);

	vp.input_layers_data.register_hit_region(layer, menu_z, display_rect);

	if (input_state.mouse_button_pressed(mouse_button::button_1) &&
		display_rect.contains(input_state.mouse_position()) &&
		vp.input_layers_data.input_available_at(layer, menu_z, input_state.mouse_position())) {
		current_menu.z_order = vp.next_z_order++;
	}

	const float top_inset = menu_chrome_height(d.fonts, current_menu, vp.fstate.sty, display_rect.width());
	const float body_height = std::max(0.f, display_rect.height() - top_inset);
	const float accent_gutter = has_side_accent(current_menu) ? accent_bar_extent(sty) : 0.f;
	const rectf body_rect = rectf::from_position_size(
		{ display_rect.left() + (current_menu.accent_edge == panel_edge::left ? accent_gutter : 0.f), display_rect.top() - top_inset },
		{ std::max(0.f, display_rect.width() - accent_gutter), body_height }
	);

	const bool is_floating = is_floating_menu(current_menu);
	const float menu_radius = is_floating ? sty.corner_radius_menu : 0.f;

	d.sprite_commands.push_back({
		.rect = body_rect,
		.color = sty.color_menu_body,
		.texture = d.blank_texture,
		.layer = layer,
		.corner_radius = menu_radius,
		.sample_scene_snapshot = true
	});

	const rectf content_rect = body_rect.inset({ sty.padding, sty.padding });
	vec2f layout_cursor = content_rect.top_left();

	ids::scope menu_scope(current_menu.id().number());

	widget_context ctx{ {
		.current_menu = &current_menu,
		.style = sty,
		.fonts = d.fonts,
		.blank_texture = d.blank_texture,
		.layout_cursor = layout_cursor,
		.sprites = d.sprite_commands,
		.texts = d.text_commands,
		.text_pool = d.text_pools[d.text_pool_slot],
		.text_pool_used = d.text_pool_used,
		.widget_anim_colors = d.widget_anim_colors,
		.widget_scrolls = d.widget_scrolls,
		.widget_tree_open = d.widget_tree_open,
		.current_layer = layer,
		.current_z_order = menu_z,
		.input_layer = vp.input_layer_render,
		.input_suppressed = vp.input_suppressed,
		.owns_keyboard = vp.owns_keyboard,
		.hit_regions = &vp.input_layers_data,
		.tooltip = &vp.tooltip,
		.context_menu = &vp.context_menu,
		.pending_text_edit = &vp.pending_text_edit,
		.clip_stack = { body_rect },
	}, input_state };

	vp.hot_widget_id = {};
	d.context = &ctx;

	builder b{
		.ctx = ctx,
		.hot_widget_id = vp.hot_widget_id,
		.active_widget_id = vp.active_widget_id,
		.focus_widget_id = vp.focus_widget_id
	};

	build(b);
	d.context = nullptr;

	stamp_z();
	end_menu(vp);
}
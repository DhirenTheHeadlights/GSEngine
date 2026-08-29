module gse.graphics:gui_chrome_impl;

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
import :gui_chrome;
import :gui_menu;
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

auto gse::gui::has_side_accent(const menu& m) -> bool {
	return m.accent_edge == panel_edge::left || m.accent_edge == panel_edge::right;
}

auto gse::gui::is_floating_menu(const menu& m) -> bool {
	return m.docked_to == dock::location::none && !m.owner_id().exists() && !m.bare && !m.fixed;
}

auto gse::gui::tab_group_grab_extent(const menu& m, const style& sty) -> float {
	return !m.bare && m.tab_contents.size() > 1 ? sty.bare_header_height : 0.f;
}

auto gse::gui::popout_close_button_rect(const rectf& title_bar_rect, const style& sty) -> rectf {
	const float button_size = std::min(sty.close_button_size, sty.title_bar_height);
	const float vertical_pad = std::max(0.f, (sty.title_bar_height - button_size) * 0.5f);
	const float horizontal_pad = vertical_pad;
	const float left = title_bar_rect.right() - button_size - horizontal_pad;
	const float top = title_bar_rect.top() - vertical_pad;
	return rectf(rectf::min_max_params{
		.min = vec2f{ left, top - button_size },
		.max = vec2f{ left + button_size, top }
	});
}

auto gse::gui::window_caption_buttons::extent() const -> float {
	return std::max({ minimize.right(), maximize.right(), close.right() }) -
		std::min({ minimize.left(), maximize.left(), close.left() });
}

auto gse::gui::window_caption_buttons_for(const rectf& title_bar_rect, const style& sty) -> window_caption_buttons {
	const float button_w = sty.title_bar_height * 1.5f;
	const float button_h = std::min(sty.title_bar_height, title_bar_rect.height());
	auto slot = [&](const float from_right) {
		return rectf::from_position_size(
			{ title_bar_rect.right() - button_w * from_right, title_bar_rect.top() },
			{ button_w, button_h }
		);
	};
	return {
		.minimize = slot(3.f),
		.maximize = slot(2.f),
		.close = slot(1.f),
	};
}

auto gse::gui::draw_window_caption_buttons(data& d, viewport_state& vp, const input::state& input_state, const menu& current_menu, const rectf& title_bar_rect, const render_layer layer) -> void {
	const style& sty = vp.fstate.sty;
	const window_caption_buttons buttons = window_caption_buttons_for(title_bar_rect, sty);
	const vec2f mouse_pos = input_state.mouse_position();
	const bool pressed = input_state.mouse_button_pressed(mouse_button::button_1);
	const bool released = input_state.mouse_button_released(mouse_button::button_1);

	auto button = [&](const rectf& rect, const std::string_view key, const std::span<const symbol::stroke> glyph, const vec4f hover_color) {
		const bool hovered = rect.contains(mouse_pos);
		const id widget_id = ids::make_from_key(hash_combine(current_menu.id().number(), stable_id(key)));
		const auto btn = interaction::press_from(vp.hot_widget_id, vp.active_widget_id, widget_id, hovered, hovered && pressed, released, true);

		d.sprite_commands.push_back({
			.rect = rect,
			.color = btn.color({
				.idle = vec4f{ 0.f, 0.f, 0.f, 0.f },
				.hot = hover_color,
				.active = hover_color,
				.disabled = vec4f{ 0.f, 0.f, 0.f, 0.f },
			}),
			.texture = d.blank_texture,
			.layer = layer,
		});

		symbol::draw(d.sprite_commands, d.blank_texture, glyph, rect, {
			.color = hovered ? sty.color_icon_hovered : sty.color_icon,
			.extent = sty.icon_extent,
			.layer = layer,
			.clip_rect = rect,
		});

		return btn.activated;
	};

	if (button(buttons.minimize, "window_caption_minimize", symbol::minimize(), sty.color_widget_hovered)) {
		vp.pending_caption_action = caption_action::minimize;
	}
	if (button(buttons.maximize, "window_caption_maximize", symbol::maximize(), sty.color_widget_hovered)) {
		vp.pending_caption_action = caption_action::toggle_maximize;
	}
	if (button(buttons.close, "window_caption_close", symbol::close(), vec4f{ 0.78f, 0.22f, 0.22f, 1.f })) {
		vp.pending_caption_action = caption_action::close;
	}
}

auto gse::gui::draw_dock_space(data& d, const style& sty, const dock::space& space) -> void {
	const float border = std::max(1.f, std::floor(sty.scale_factor));
	const float radius = std::max(2.f, 4.f * sty.scale_factor);
	const float inset = 5.f * sty.scale_factor;

	const dock::area* hot = nullptr;
	for (const dock::area& area : space.areas) {
		if (area.dock_location == space.hot) {
			hot = &area;
			break;
		}
	}

	if (hot) {
		d.sprite_commands.push_back({
			.rect = hot->target,
			.color = sty.color_dock_preview,
			.texture = d.blank_texture,
			.layer = render_layer::overlay,
			.z_order = 10,
			.corner_radius = radius,
		});
	}

	for (const dock::area& area : space.areas) {
		if (area.rect.width() <= 0.f || area.rect.height() <= 0.f) {
			continue;
		}

		const bool is_hot = &area == hot;

		d.sprite_commands.push_back({
			.rect = area.rect.inset({ -border, -border }),
			.color = is_hot ? sty.color_accent : sty.color_border,
			.texture = d.blank_texture,
			.layer = render_layer::overlay,
			.z_order = 11,
			.corner_radius = radius + border,
		});

		d.sprite_commands.push_back({
			.rect = area.rect,
			.color = is_hot ? sty.color_widget_hovered : sty.color_menu_body,
			.texture = d.blank_texture,
			.layer = render_layer::overlay,
			.z_order = 12,
			.corner_radius = radius,
		});

		d.sprite_commands.push_back({
			.rect = layout::dock_target_rect(area.rect.inset({ inset, inset }), area.dock_location, 0.5f),
			.color = is_hot ? sty.color_accent : sty.color_text_secondary,
			.texture = d.blank_texture,
			.layer = render_layer::overlay,
			.z_order = 13,
			.corner_radius = border,
		});
	}
}

auto gse::gui::remove_tab_from_host(viewport_state& vp, const std::string_view menu_name) -> void {
	id host_id;
	for (const menu& m : vp.menus.items()) {
		if (std::ranges::find(m.tab_contents, menu_name) != m.tab_contents.end()) {
			host_id = m.id();
			break;
		}
	}

	menu* host = vp.menus.try_get(host_id);
	if (!host) {
		return;
	}

	const auto tab_it = std::ranges::find(host->tab_contents, menu_name);
	const auto removed_idx = static_cast<std::uint32_t>(std::distance(host->tab_contents.begin(), tab_it));
	host->tab_contents.erase(tab_it);

	const bool host_has_children = std::ranges::any_of(vp.menus.items(), [host_id](const menu& m) {
		return m.owner_id() == host_id;
	});

	if (host->tab_contents.empty() && !host_has_children) {
		if (host->docked_to != dock::location::none) {
			layout::undock(vp.menus, host_id);
		}
		vp.menus.remove(host_id);
	}
	else if (!host->tab_contents.empty()) {
		if (host->active_tab_index >= host->tab_contents.size()) {
			host->active_tab_index = static_cast<std::uint32_t>(host->tab_contents.size() - 1);
		}
		else if (host->active_tab_index > removed_idx) {
			host->active_tab_index -= 1;
		}
	}
}

auto gse::gui::draw_menu_chrome(data& d, viewport_state& vp, const input::state& input_state, menu& current_menu, const render_layer layer) -> void {
	const style& sty = vp.fstate.sty;

	const rectf display_rect = calculate_display_rect(vp, current_menu);
	const bool is_floating = is_floating_menu(current_menu);
	const float menu_radius = is_floating ? sty.corner_radius_menu : 0.f;

	const float top_inset = menu_chrome_height(d.fonts, current_menu, vp.fstate.sty, display_rect.width());

	const rectf title_bar_rect =
		rectf::from_position_size(
			display_rect.top_left(),
			{ display_rect.width(), top_inset }
		);

	const float body_height = std::max(0.f, display_rect.height() - top_inset);
	const rectf body_rect = rectf::from_position_size(
		{ display_rect.left(), display_rect.top() - top_inset },
		{ display_rect.width(), body_height }
	);

	if (is_floating && sty.color_shadow.w() > 0.f) {
		const float shadow_offset = 4.f * (sty.font_size / 16.f);
		const rectf shadow_rect = rectf::from_position_size(
			{ display_rect.left() + shadow_offset, display_rect.top() - shadow_offset },
			display_rect.size()
		);
		d.sprite_commands.push_back({
			.rect = shadow_rect,
			.color = sty.color_shadow,
			.texture = d.blank_texture,
			.layer = layer,
			.corner_radius = menu_radius + 2.f
		});
	}

	if (menu_radius > 0.f) {
		const rectf border_rect = display_rect.inset({ -1.f, -1.f });
		d.sprite_commands.push_back({
			.rect = border_rect,
			.color = sty.color_border,
			.texture = d.blank_texture,
			.layer = layer,
			.corner_radius = menu_radius + 1.f
		});
	}

	d.sprite_commands.push_back({
		.rect = body_rect,
		.color = sty.color_menu_body,
		.texture = d.blank_texture,
		.layer = layer,
		.corner_radius = menu_radius,
		.sample_scene_snapshot = true
	});

	if (current_menu.bare) {
		d.sprite_commands.push_back({
			.rect = title_bar_rect,
			.color = sty.color_accent,
			.texture = d.blank_texture,
			.layer = layer,
			.corner_radius = menu_radius
		});
		if (display_rect.left() > 1.f) {
			d.sprite_commands.push_back({
				.rect = rectf::from_position_size(display_rect.top_left(), { 1.f, display_rect.height() }),
				.color = sty.color_border,
				.texture = d.blank_texture,
				.layer = layer
			});
		}
	}
	else if (current_menu.tab_contents.size() > 1) {
		const float grab_extent = tab_group_grab_extent(current_menu, sty);
		d.sprite_commands.push_back({
			.rect = rectf::from_position_size(title_bar_rect.top_left(), { title_bar_rect.width(), grab_extent }),
			.color = sty.color_accent,
			.texture = d.blank_texture,
			.layer = layer,
			.corner_radius = menu_radius
		});
		draw_tab_bar(d, vp, input_state, current_menu, rectf::from_position_size(
			{ title_bar_rect.left(), title_bar_rect.top() - grab_extent },
			{ title_bar_rect.width(), std::max(0.f, title_bar_rect.height() - grab_extent) }
		), layer);
	}
	else {
		d.sprite_commands.push_back({
			.rect = title_bar_rect,
			.color = sty.color_title_bar,
			.texture = d.blank_texture,
			.layer = layer,
			.corner_radius = menu_radius
		});

		if (d.fonts.text.valid() && !current_menu.tab_contents.empty()) {
			d.text_commands.push_back({
				.font = d.fonts.text,
				.text = intern_text(d, current_menu.tab_contents[0]),
				.position = { title_bar_rect.left() + sty.padding, title_bar_rect.center().y() + d.fonts.text.resolve()->vertical_center_offset(sty.font_size) },
				.scale = sty.font_size,
				.clip_rect = title_bar_rect,
				.layer = layer
			});
		}
	}

	if (has_side_accent(current_menu)) {
		const float bar_width = accent_bar_extent(sty);
		const float bar_left = current_menu.accent_edge == panel_edge::right ? display_rect.right() - bar_width : display_rect.left();
		d.sprite_commands.push_back({
			.rect = rectf::from_position_size({ bar_left, display_rect.top() }, { bar_width, display_rect.height() }),
			.color = sty.color_accent,
			.texture = d.blank_texture,
			.layer = layer
		});
	}

	if (is_popout(vp) && !current_menu.owner_id().exists()) {
		draw_window_caption_buttons(d, vp, input_state, current_menu, title_bar_rect, layer);
	}

	if (is_popout_menu_tag(current_menu.id().tag())) {
		const rectf close_rect = popout_close_button_rect(title_bar_rect, sty);
		const vec2f mouse_pos = input_state.mouse_position();
		const bool hovered = close_rect.contains(mouse_pos);
		const bool pressed = input_state.mouse_button_pressed(mouse_button::button_1);
		const bool released = input_state.mouse_button_released(mouse_button::button_1);

		const id close_id = ids::make_from_key(
			hash_combine(current_menu.id().number(), stable_id("popout_close"))
		);
		const auto btn = interaction::press_from(vp.hot_widget_id, vp.active_widget_id, close_id, hovered, hovered && pressed, released, true);

		const vec4f transparent{ 0.f, 0.f, 0.f, 0.f };
		const vec4f bg_color = btn.color({
			.idle = transparent,
			.hot = sty.color_widget_hovered,
			.active = sty.color_widget_hovered,
			.disabled = transparent,
		});
		d.sprite_commands.push_back({
			.rect = close_rect,
			.color = bg_color,
			.texture = d.blank_texture,
			.layer = layer,
			.corner_radius = close_rect.width() * 0.5f,
		});

		symbol::draw(d.sprite_commands, d.blank_texture, symbol::close(), close_rect, {
			.color = hovered ? sty.color_icon_hovered : sty.color_icon,
			.extent = sty.icon_extent,
			.layer = layer,
			.clip_rect = close_rect,
		});

		if (btn.activated) {
			vp.pending_popout_close_ids.push_back(current_menu.id());
		}
	}
}

auto gse::gui::draw_tab_bar(data& d, viewport_state& vp, const input::state& input_state, menu& current_menu, const rectf& title_bar_rect, const render_layer layer) -> void {
	const style& sty = vp.fstate.sty;

	d.sprite_commands.push_back({
		.rect = title_bar_rect,
		.color = sty.color_title_bar,
		.texture = d.blank_texture,
		.layer = layer
	});

	if (current_menu.tab_contents.empty() || !d.fonts.text.valid()) {
		return;
	}

	std::vector<tab_desc> descs;
	descs.reserve(current_menu.tab_contents.size());
	for (std::size_t i = 0; i < current_menu.tab_contents.size(); ++i) {
		descs.push_back({
			.tab_id = generate_temp_id(i + 1),
			.caption = current_menu.tab_contents[i],
			.closeable = current_menu.tabs_closeable,
		});
	}

	vec2f dummy_cursor{};
	widget_context ctx{ {
		.current_menu = &current_menu,
		.style = sty,
		.fonts = d.fonts,
		.blank_texture = d.blank_texture,
		.layout_cursor = dummy_cursor,
		.sprites = d.sprite_commands,
		.texts = d.text_commands,
		.text_pool = d.text_pools[d.text_pool_slot],
		.text_pool_used = d.text_pool_used,
		.widget_anim_colors = d.widget_anim_colors,
		.widget_scrolls = d.widget_scrolls,
		.widget_tree_open = d.widget_tree_open,
		.current_layer = layer,
		.current_z_order = current_menu.z_order,
		.input_layer = vp.input_layer_render,
		.input_suppressed = vp.input_suppressed,
		.owns_keyboard = vp.owns_keyboard,
		.hit_regions = &vp.input_layers_data,
		.tooltip = &vp.tooltip,
		.context_menu = &vp.context_menu,
		.clip_stack = { title_bar_rect },
	}, input_state };

	const tab_strip_result tabs = tab_strip(ctx, {
		.area = title_bar_rect,
		.tabs = descs,
		.active = generate_temp_id(current_menu.active_tab_index + 1),
		.orientation = tab_orientation::horizontal,
		.overflow = tab_overflow::wrap,
		.allow_reorder = true,
		.min_tab_extent = 60.f,
		.max_tab_extent = 200.f,
	}, current_menu.tab_bar);

	if (tabs.activated.exists()) {
		current_menu.active_tab_index = static_cast<std::uint32_t>(tabs.activated.number() - 1);
	}
	if (tabs.close_requested.exists()) {
		vp.pending_tab_close = std::pair{ current_menu.id(), static_cast<std::uint32_t>(tabs.close_requested.number() - 1) };
	}
	if (tabs.reorder_id.exists()) {
		const auto from = static_cast<std::size_t>(tabs.reorder_id.number() - 1);
		const std::size_t to = std::min(tabs.reorder_to, current_menu.tab_contents.size() - 1);
		if (from < current_menu.tab_contents.size() && from != to) {
			const auto begin = current_menu.tab_contents.begin();
			std::string moved = std::move(current_menu.tab_contents[from]);
			current_menu.tab_contents.erase(begin + static_cast<std::ptrdiff_t>(from));
			current_menu.tab_contents.insert(current_menu.tab_contents.begin() + static_cast<std::ptrdiff_t>(to), std::move(moved));
			current_menu.active_tab_index = static_cast<std::uint32_t>(to);
		}
	}
}

auto gse::gui::menu_chrome_height(const font_set& fonts, const menu& m, const style& sty, const float width) -> float {
	if (m.bare) {
		return sty.bare_header_height;
	}

	if (m.tab_contents.size() <= 1 || !fonts.text.valid()) {
		return sty.title_bar_height;
	}

	const float available_width = std::max(0.f, width - sty.padding * 2.f);

	std::vector<tab_desc> descs;
	descs.reserve(m.tab_contents.size());
	for (const std::string& tag : m.tab_contents) {
		descs.push_back({ .caption = tag });
	}

	const tab_strip_metrics metrics = tab_strip_measure(sty, {
		.font = fonts.text,
		.tabs = descs,
		.available_extent = available_width,
		.min_tab_extent = 60.f,
		.max_tab_extent = 200.f,
	});
	const std::uint32_t rows = std::max(1u, std::min(metrics.required_rows, std::max(1u, m.tab_bar.visible_rows)));
	return tab_group_grab_extent(m, sty) + tab_strip_extent(metrics, rows) + 4.f * sty.scale_factor;
}

auto gse::gui::tab_index_at(const font_set& fonts, const menu& m, const style& sty, const rectf& title_bar_rect, const vec2f mouse) -> std::optional<std::uint32_t> {
	if (m.tab_contents.size() <= 1 || !fonts.text.valid()) {
		return std::nullopt;
	}

	std::vector<tab_desc> descs;
	descs.reserve(m.tab_contents.size());
	for (std::size_t i = 0; i < m.tab_contents.size(); ++i) {
		descs.push_back({
			.tab_id = generate_temp_id(i + 1),
			.caption = m.tab_contents[i],
		});
	}

	const float grab_extent = tab_group_grab_extent(m, sty);
	const rectf strip_rect = rectf::from_position_size(
		{ title_bar_rect.left(), title_bar_rect.top() - grab_extent },
		{ title_bar_rect.width(), std::max(0.f, title_bar_rect.height() - grab_extent) }
	);

	for (const tab_strip_placement& p : tab_strip_layout(fonts.text, sty, strip_rect, descs, m.tab_bar, tab_overflow::wrap, 60.f, 200.f)) {
		if (p.rect.intersection(strip_rect).contains(mouse)) {
			return static_cast<std::uint32_t>(p.index);
		}
	}
	return std::nullopt;
}

auto gse::gui::caption_button(builder& b, const rectf& rect, const std::string& key, const std::span<const symbol::stroke> glyph, const vec4f hover_color, const bool enabled) -> bool {
	const draw_context& ctx = b.ctx;
	const id widget_id = ids::make(key);

	const auto btn = interaction::press_in_rect(ctx, b.hot_widget_id, b.active_widget_id, widget_id, rect, enabled);

	ctx.queue_sprite({
		.rect = rect,
		.color = btn.color({
			.idle = ctx.style.color_input_background,
			.hot = hover_color,
			.active = hover_color,
			.disabled = ctx.style.color_input_background,
		}),
		.texture = ctx.blank_texture,
	});

	symbol::draw(ctx, glyph, rect, {
		.color = enabled ? ctx.style.color_text : ctx.style.color_text_disabled,
		.extent = std::min(ctx.style.icon_extent, std::min(rect.width(), rect.height())),
	});

	return btn.activated;
}
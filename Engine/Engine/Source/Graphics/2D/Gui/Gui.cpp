module gse.graphics;

import std;
import gse.std_meta;

import :gui;
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

auto gse::gui::system::init_body(run_context& ctx, const window::data& window_s, asset::data& assets, data& d) -> async::task<> {
	d.font.options = asset::enumerate_resources<font>();

	if (d.font.options.empty()) {
		d.font.options.push_back("default");
	}

	if (d.font.value < 0 || d.font.value >= static_cast<int>(d.font.options.size())) {
		d.font.value = 0;
	}

	d.blank_texture = asset::queue<texture>(assets, "blank", vec4f(1, 1, 1, 1));
	d.gui_font = co_await asset::load<gse::font>(ctx, "Fonts/" + d.font.options[d.font.value]);
	while (asset::resource_state<texture>(assets, d.blank_texture.id()) != resource::state::loaded) {
		co_await ctx.yield_tick();
	}
	d.menus = load(config::resource_path / d.file_path, d.menus);

	d.last_font_index = d.font.value;

	auto calculate_group_bounds = [&d](const id root_id) -> ui_rect {
		const menu* root = d.menus.try_get(root_id);
		if (!root) {
			return {};
		}

		ui_rect bounds = root->rect;

		std::function<void(id)> expand = [&](const id parent_id) {
			for (const menu& item : d.menus.items()) {
				if (item.owner_id() == parent_id) {
					bounds = ui_rect::bounding_box(bounds, item.rect);
					expand(item.id());
				}
			}
		};

		expand(root_id);
		return bounds;
	};

	const ui_rect screen_rect = usable_screen_rect(d, window_s);

	for (menu& m : d.menus.items()) {
		if (!m.owner_id().exists()) {
			if (m.docked_to != dock::location::none) {
				if (m.docked_to == dock::location::center) {
					m.rect = screen_rect;
				}
				else {
					m.rect = layout::dock_target_rect(screen_rect, m.docked_to, m.dock_split_ratio);
				}
			}
			else {
				m.rect = calculate_group_bounds(m.id());

				const float max_width = screen_rect.width();
				const float max_height = screen_rect.height();
				const float clamped_width = std::min(m.rect.width(), max_width);
				const float clamped_height = std::min(m.rect.height(), max_height);

				float new_left = m.rect.left();
				float new_top = m.rect.top();

				if (new_left < 0.f) {
					new_left = 0.f;
				}
				else if (new_left + clamped_width > screen_rect.width()) {
					new_left = std::max(0.f, screen_rect.width() - clamped_width);
				}

				if (new_top > screen_rect.top()) {
					new_top = screen_rect.top();
				}
				else if (new_top - clamped_height < 0.f) {
					new_top = clamped_height;
				}

				m.rect = ui_rect::from_position_size(
					{ new_left, new_top },
					{ clamped_width, clamped_height }
				);
			}

			layout::update(d.menus, m.id());
		}
	}

	d.visible_menu_ids_last_frame.clear();
	d.visible_menu_ids_last_frame.reserve(d.menus.items().size());

	for (menu& m : d.menus.items()) {
		d.visible_menu_ids_last_frame.push_back(m.id());
	}

	d.previous_viewport_size = vec2f(window::viewport(window_s));
}

auto gse::gui::system::run(run_context& ctx, const window::data& window_s, const asset::data& assets_s, const gse::input::system::data& input_state, const save::registry& save_reg, data& d) -> async::task<> {
	co_await init_body(ctx, window_s, const_cast<asset::data&>(assets_s), d);

	while (true) {
		co_await update_body(ctx, window_s, assets_s, input_state, save_reg, d);
		co_await ctx.next_tick();
	}
}

auto gse::gui::system::update_body(run_context& ctx, const window::data& window_s, const asset::data& assets_s, const gse::input::system::data& input_state, const save::registry& save_reg, data& d) -> async::task<> {
	const auto current_viewport_size = vec2f(window::viewport(window_s));

	if (d.previous_viewport_size.x() > 0.f && d.previous_viewport_size.y() > 0.f) {
		if (current_viewport_size.x() > 0.f && current_viewport_size.y() > 0.f && (current_viewport_size.x() != d.previous_viewport_size.x() || current_viewport_size.y() != d.previous_viewport_size.y())) {
			const style old_sty = apply_scale(
				d,
				style::from_theme(d.current_theme),
				d.previous_viewport_size.y()
			);
			const style new_sty = apply_scale(
				d,
				style::from_theme(d.current_theme),
				current_viewport_size.y()
			);

			const float old_usable_height = d.previous_viewport_size.y();
			const float new_usable_height = current_viewport_size.y();

			const float scale_x = current_viewport_size.x() / d.previous_viewport_size.x();
			const float scale_y = new_usable_height / old_usable_height;

			const ui_rect new_screen_rect = ui_rect::from_position_size(
				{ 0.f, new_usable_height },
				{ current_viewport_size.x(), new_usable_height }
			);

			for (menu& m : d.menus.items()) {
				if (!m.owner_id().exists()) {
					if (m.docked_to != dock::location::none) {
						if (m.docked_to == dock::location::center) {
							m.rect = new_screen_rect;
						}
						else {
							m.rect = layout::dock_target_rect(
								new_screen_rect,
								m.docked_to,
								m.dock_split_ratio
							);
						}
					}
					else {
						const float ratio_x = m.rect.left() / d.previous_viewport_size.x();
						const float ratio_y = (d.previous_viewport_size.y() - m.rect.top()) / old_usable_height;

						const float new_left = ratio_x * current_viewport_size.x();
						const float new_top = current_viewport_size.y() - (ratio_y * new_usable_height);

						const float new_width = m.rect.width() * scale_x;
						const float new_height = m.rect.height() * scale_y;

						const float actual_width = std::min(new_width, current_viewport_size.x());
						const float actual_height = std::min(new_height, new_usable_height);

						const float clamped_left =
							std::clamp(
								new_left,
								0.f,
								std::max(
									0.f,
									current_viewport_size.x() - actual_width
								)
							);
						const float clamped_top = std::clamp(new_top, actual_height, new_usable_height);

						m.rect =
							ui_rect::from_position_size(
								{ clamped_left, clamped_top },
								{ actual_width, actual_height }
							);
					}

					layout::update(d.menus, m.id());
				}
			}

			d.previous_viewport_size = current_viewport_size;
		}
	}
	else {
		d.previous_viewport_size = current_viewport_size;
	}

	d.fstate = {};
	d.sprite_commands.clear();
	d.text_commands.clear();
	d.input_layers_data.begin_frame();

	const style frame_sty = apply_scale(
		d,
		style::from_theme(d.current_theme),
		current_viewport_size.y()
	);

	d.fstate = {
		.sty = frame_sty,
		.active = d.gui_font.valid()
	};

	d.hot_widget_id = {};

	d.input_layer_render = !d.menu_stack.empty() ? render_layer::popup : render_layer::content;

	d.name_to_menu_id.clear();
	for (menu& m : d.menus.items()) {
		m.was_begun_this_frame = false;
		m.chrome_drawn_this_frame = false;
		for (const std::string& tab : m.tab_contents) {
			d.name_to_menu_id.emplace(stable_id(tab), m.id());
		}
		d.name_to_menu_id.emplace(stable_id(m.id().tag()), m.id());
	}

	if (d.font.value != d.last_font_index) {
		reload_font(d, assets_s);
		d.last_font_index = d.font.value;
	}

	const vec2f mouse_position = gse::input::system::current_state(input_state).mouse_position();
	const bool mouse_held = gse::input::system::current_state(input_state).mouse_button_held(mouse_button::button_1);

	match(d.current_state.v)
		.if_is([&](const states::idle&) {
			d.current_state = handle_idle_state(
				d,
				gse::input::system::current_state(input_state),
				mouse_position,
				mouse_held,
				frame_sty
			);
		})
		.else_if_is([&](const states::dragging& st) {
			d.current_state = handle_dragging_state(d, st, window_s, mouse_position, mouse_held);
		})
		.else_if_is([&](const states::resizing& st) {
			d.current_state = handle_resizing_state(d, st, mouse_position, mouse_held, frame_sty, window_s);
		})
		.else_if_is([&](const states::resizing_divider& st) {
			d.current_state = handle_resizing_divider_state(d, st, mouse_position, mouse_held, frame_sty);
		})
		.else_if_is([&](const states::pending_drag& st) {
			d.current_state = handle_pending_drag_state(d, st, mouse_position, mouse_held);
		})
		.otherwise([&] {
			d.current_state = states::idle{};
		});

	if (d.save_clock.elapsed() > data::update_interval) {
		gui::save(d.menus, config::resource_path / d.file_path);
		d.save_clock.reset();
	}

	if (!d.fstate.active) {
		d.fstate = {};
		co_return;
	}

	const gse::input::state& input_st = gse::input::system::current_state(input_state);
	const auto viewport_size = vec2f(window::viewport(window_s));

	if (d.active_dock_space) {
		const auto [areas] = d.active_dock_space.value();
		const vec2f mouse_pos = input_st.mouse_position();

		for (const dock::area& area : areas) {
			if (area.rect.contains(mouse_pos)) {
				d.sprite_commands.push_back({
					.rect = area.target,
					.color = d.fstate.sty.color_dock_preview,
					.texture = d.blank_texture
				});
				break;
			}
		}

		for (const dock::area& area : areas) {
			d.sprite_commands.push_back({
				.rect = area.rect,
				.color = d.fstate.sty.color_dock_preview,
				.texture = d.blank_texture
			});
		}
	}

	for (const auto& req : ctx.read_channel<push_screen_request>()) {
		d.menu_stack.push_factory(req.factory);
	}
	for ([[maybe_unused]] const auto& req : ctx.read_channel<pop_screen_request>()) {
		d.menu_stack.pop();
	}
	for ([[maybe_unused]] const auto& req : ctx.read_channel<clear_screens_request>()) {
		d.menu_stack.clear();
	}
	for (const auto& req : ctx.read_channel<set_manual_cursor_request>()) {
		d.manual_cursor = req.show;
	}

	if (!d.menu_stack.empty()) {
		process_screen(d, input_st, viewport_size);
	}

	ctx.channels.push<ui_focus_request>({
		.focus = !d.menu_stack.empty() || d.manual_cursor,
	});

	for (const auto& content : ctx.read_channel<menu_content>()) {
		process_menu(d, input_st, content.menu, content.layer, content.build);
	}

	if (d.tooltip.pending_widget_id.exists()) {
		if (d.tooltip.pending_widget_id == d.tooltip.widget_id) {
			d.tooltip.hover_time += system_clock::dt<time>();
		}
		else {
			d.tooltip.widget_id = d.tooltip.pending_widget_id;
			d.tooltip.hover_time = time{};
		}
	}
	else {
		d.tooltip.widget_id.reset();
		d.tooltip.hover_time = time{};
		d.tooltip.text.clear();
	}

	if (d.tooltip.widget_id.exists() && d.tooltip.hover_time >= tooltip_state::show_delay && !d.tooltip.text.empty() && d.gui_font.valid()) {
		const float padding = d.fstate.sty.padding;
		const float font_size = d.fstate.sty.font_size;
		const float text_width = d.gui_font->width(d.tooltip.text, font_size);
		const float text_height = d.gui_font->line_height(font_size);

		const float tooltip_width = text_width + padding * 2.f;
		const float tooltip_height = text_height + padding;

		vec2f tooltip_pos = d.tooltip.position + vec2f(15.f, -15.f);

		if (tooltip_pos.x() + tooltip_width > viewport_size.x()) {
			tooltip_pos.x() = viewport_size.x() - tooltip_width;
		}
		if (tooltip_pos.y() - tooltip_height < 0.f) {
			tooltip_pos.y() = tooltip_height;
		}

		const ui_rect tooltip_rect = ui_rect::from_position_size(
			tooltip_pos,
			{ tooltip_width, tooltip_height }
		);

		d.sprite_commands.push_back({
			.rect = tooltip_rect,
			.color = d.fstate.sty.color_menu_body,
			.texture = d.blank_texture,
			.layer = render_layer::modal,
			.z_order = 100
		});

		d.sprite_commands.push_back({
			.rect = tooltip_rect.inset({ -1.f, -1.f }),
			.color = d.fstate.sty.color_border,
			.texture = d.blank_texture,
			.layer = render_layer::modal,
			.z_order = 99
		});

		d.text_commands.push_back({
			.font = d.gui_font,
			.text = d.tooltip.text,
			.position = { tooltip_rect.left() + padding, tooltip_rect.center().y() + d.gui_font->vertical_center_offset(font_size) },
			.scale = font_size,
			.color = d.fstate.sty.color_text,
			.layer = render_layer::modal,
			.z_order = 100
		});
	}

	d.tooltip.pending_widget_id.reset();

	if (window_s.ui_focus) {
		cursor::render_to(assets_s, d.sprite_commands, input_st.mouse_position());
	}

	d.visible_menu_ids_last_frame.clear();
	d.visible_menu_ids_last_frame.reserve(d.menus.items().size());
	for (menu& m : d.menus.items()) {
		m.was_visible_last_frame = m.was_begun_this_frame;
		if (m.was_begun_this_frame) {
			d.visible_menu_ids_last_frame.push_back(m.id());
		}
	}

	auto already_sorted = [](const auto& commands) {
		render_layer prev_layer = render_layer::background;
		std::uint32_t prev_z = 0;
		for (const auto& cmd : commands) {
			if (static_cast<std::uint8_t>(cmd.layer) < static_cast<std::uint8_t>(prev_layer)) {
				return false;
			}
			if (cmd.layer == prev_layer && cmd.z_order < prev_z) {
				return false;
			}
			prev_layer = cmd.layer;
			prev_z = cmd.z_order;
		}
		return true;
	};

	auto sort_by_layer = [](auto& commands) {
		std::ranges::stable_sort(
			commands,
			[](const auto& a, const auto& b) {
				if (a.layer != b.layer) {
					return static_cast<std::uint8_t>(a.layer) < static_cast<std::uint8_t>(b.layer);
				}
				return a.z_order < b.z_order;
			}
		);
	};

	if (!already_sorted(d.sprite_commands)) {
		sort_by_layer(d.sprite_commands);
	}
	if (!already_sorted(d.text_commands)) {
		sort_by_layer(d.text_commands);
	}

	for (auto& cmd : d.sprite_commands) {
		ctx.channels.push<renderer::sprite_command>(std::move(cmd));
	}

	for (auto& cmd : d.text_commands) {
		ctx.channels.push<renderer::text_command>(std::move(cmd));
	}

	d.fstate = {};

	co_return;
}

auto gse::gui::system::shutdown(shutdown_context&, data& d) -> void {
	gui::save(d.menus, config::resource_path / d.file_path);
}

auto gse::gui::system::save(data& d) -> void {
	gui::save(d.menus, config::resource_path / d.file_path);
}

auto gse::gui::system::process_menu(data& d, const gse::input::state& input_state, const std::string& name, const render_layer layer, const std::function<void(builder&)>& build) -> void {
	if (!d.fstate.active) {
		return;
	}

	if (!begin_menu(d, name)) {
		return;
	}

	menu& current_menu = *d.current_menu;

	if (!current_menu.chrome_drawn_this_frame) {
		draw_menu_chrome(d, input_state, current_menu, layer);
		current_menu.chrome_drawn_this_frame = true;
	}

	const auto it = std::ranges::find(current_menu.tab_contents, name);
	const bool is_active_tab = (it != current_menu.tab_contents.end()) &&
		(std::distance(
			 current_menu.tab_contents.begin(),
			 it
		 ) ==
		 static_cast<std::ptrdiff_t>(current_menu.active_tab_index));

	if (!is_active_tab) {
		end_menu(d);
		return;
	}

	const style& sty = d.fstate.sty;
	const ui_rect display_rect = calculate_display_rect(d, current_menu);

	const ui_rect body_rect = ui_rect::from_position_size(
		{ display_rect.left(), display_rect.top() - sty.title_bar_height },
		{ display_rect.width(), display_rect.height() - sty.title_bar_height }
	);

	const bool is_floating = current_menu.docked_to == dock::location::none && !current_menu.owner_id().exists();
	const float menu_radius = is_floating ? sty.corner_radius_menu : 0.f;

	d.sprite_commands.push_back({
		.rect = body_rect,
		.color = sty.color_menu_body,
		.texture = d.blank_texture,
		.layer = layer,
		.corner_radius = menu_radius,
		.sample_scene_snapshot = true
	});

	const ui_rect content_rect = body_rect.inset({ sty.padding, sty.padding });
	vec2f layout_cursor = content_rect.top_left();

	ids::scope menu_scope(current_menu.id().number());

	draw_context ctx{
		.current_menu = &current_menu,
		.style = sty,
		.input = input_state,
		.font = d.gui_font,
		.blank_texture = d.blank_texture,
		.layout_cursor = layout_cursor,
		.sprites = d.sprite_commands,
		.texts = d.text_commands,
		.widget_anim_colors = d.widget_anim_colors,
		.widget_scrolls = d.widget_scrolls,
		.current_layer = layer,
		.input_layer = d.input_layer_render,
		.hit_regions = &d.input_layers_data,
		.tooltip = &d.tooltip,
		.clip_stack = { body_rect },
	};

	d.hot_widget_id = {};
	d.context = &ctx;

	builder b{
		.ctx = ctx,
		.hot_widget_id = d.hot_widget_id,
		.active_widget_id = d.active_widget_id,
		.focus_widget_id = d.focus_widget_id
	};

	build(b);
	d.context = nullptr;

	end_menu(d);
}

auto gse::gui::system::begin_menu(data& d, const std::string& name) -> bool {
	const std::uint64_t name_key = stable_id(name);
	if (const auto it = d.name_to_menu_id.find(name_key); it != d.name_to_menu_id.end()) {
		if (menu* m = d.menus.try_get(it->second)) {
			d.current_menu = m;
			d.current_menu->was_begun_this_frame = true;
			d.current_scope = std::make_unique<ids::scope>(d.current_menu->id().number());
			return true;
		}
	}

	menu new_menu(
		name,
		menu_data{
			.rect = ui_rect({
				.min = { 100.f, 100.f },
				.max = { 400.f, 300.f }
			}),
			.parent_id = id()
		}
	);

	const id new_id = new_menu.id();
	d.menus.add(new_id, std::move(new_menu));

	if (menu* menu_ptr = d.menus.try_get(new_id)) {
		d.current_menu = menu_ptr;
		d.current_menu->was_begun_this_frame = true;
		d.current_scope = std::make_unique<ids::scope>(d.current_menu->id().number());
		for (const std::string& tab : menu_ptr->tab_contents) {
			d.name_to_menu_id.emplace(stable_id(tab), new_id);
		}
		d.name_to_menu_id.emplace(stable_id(menu_ptr->id().tag()), new_id);
		return true;
	}

	return false;
}

auto gse::gui::system::end_menu(data& d) -> void {
	d.current_scope.reset();
	d.current_menu = nullptr;
}

auto gse::gui::system::process_screen(data& d, const gse::input::state& input_state, const vec2f viewport_size) -> void {
	if (!d.fstate.active) {
		return;
	}

	auto* top = d.menu_stack.top();
	if (top == nullptr) {
		return;
	}

	const style& sty = d.fstate.sty;
	const ui_rect body_rect = top->body_rect(sty, viewport_size);

	if (!d.screen_surface) {
		d.screen_surface.emplace(
			"__gui_screen__",
			menu_data{
				.rect = body_rect,
				.parent_id = id(),
			}
		);
	}
	d.screen_surface->rect = body_rect;

	const ui_rect content_rect = body_rect.inset({ sty.padding, sty.padding });
	vec2f layout_cursor = content_rect.top_left();

	ids::scope screen_scope(d.screen_surface->id().number());

	draw_context ctx{
		.current_menu = &*d.screen_surface,
		.style = sty,
		.input = input_state,
		.font = d.gui_font,
		.blank_texture = d.blank_texture,
		.layout_cursor = layout_cursor,
		.sprites = d.sprite_commands,
		.texts = d.text_commands,
		.widget_anim_colors = d.widget_anim_colors,
		.widget_scrolls = d.widget_scrolls,
		.current_layer = render_layer::popup,
		.input_layer = d.input_layer_render,
		.hit_regions = &d.input_layers_data,
		.tooltip = &d.tooltip,
		.clip_stack = { body_rect },
	};

	d.hot_widget_id = {};
	d.context = &ctx;

	top->draw_backdrop(ctx, viewport_size);

	builder b{
		.ctx = ctx,
		.hot_widget_id = d.hot_widget_id,
		.active_widget_id = d.active_widget_id,
		.focus_widget_id = d.focus_widget_id,
	};

	d.menu_stack.tick(b);

	d.context = nullptr;
}

auto gse::gui::system::usable_screen_rect(data& d, const window::data& window_s) -> ui_rect {
	const auto viewport_size = vec2f(window::viewport(window_s));
	return ui_rect::from_position_size(
		{ 0.f, viewport_size.y() },
		{ viewport_size.x(), viewport_size.y() }
	);
}

auto gse::gui::system::calculate_display_rect(data& d, const menu& m) -> ui_rect {
	ui_rect display_rect = m.rect;

	for (const menu& child : d.menus.items()) {
		if (child.owner_id() == m.id() && !child.was_begun_this_frame && child.was_visible_last_frame) {
			display_rect = ui_rect::bounding_box(display_rect, calculate_display_rect(d, child));
		}
	}

	return display_rect;
}

auto gse::gui::system::apply_scale(const data& d, style sty, const float viewport_height) -> style {
	constexpr float reference_height = 1080.f;
	const float base_scale = viewport_height / reference_height;
	const float final_scale = base_scale * d.ui_scale;

	sty.scale_factor = final_scale;

	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^style, std::meta::access_context::unchecked()))) {
		if constexpr (has_annotation<scaled_tag>(m)) {
			sty.[:m:] *= final_scale;
		}
	}

	return sty;
}

auto gse::gui::system::reload_font(data& d, const asset::data& assets) -> void {
	if (d.font.value >= 0 && d.font.value < static_cast<int>(d.font.options.size())) {
		d.gui_font = asset::get<font>(assets, "Fonts/" + d.font.options[d.font.value]);
	}
}

auto gse::gui::system::draw_menu_chrome(data& d, const gse::input::state& input_state, menu& current_menu, const render_layer layer) -> void {
	const style& sty = d.fstate.sty;

	const ui_rect display_rect = calculate_display_rect(d, current_menu);
	const bool is_floating = current_menu.docked_to == dock::location::none && !current_menu.owner_id().exists();
	const float menu_radius = is_floating ? sty.corner_radius_menu : 0.f;

	const ui_rect title_bar_rect =
		ui_rect::from_position_size(
			display_rect.top_left(),
			{ display_rect.width(), sty.title_bar_height }
		);

	const ui_rect body_rect = ui_rect::from_position_size(
		{ display_rect.left(), display_rect.top() - sty.title_bar_height },
		{ display_rect.width(), display_rect.height() - sty.title_bar_height }
	);

	if (is_floating && sty.color_shadow.w() > 0.f) {
		const float shadow_offset = 4.f * (sty.font_size / 16.f);
		const ui_rect shadow_rect = ui_rect::from_position_size(
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
		const ui_rect border_rect = display_rect.inset({ -1.f, -1.f });
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

	if (current_menu.tab_contents.size() > 1) {
		draw_tab_bar(d, input_state, current_menu, title_bar_rect, layer);
	}
	else {
		d.sprite_commands.push_back({
			.rect = title_bar_rect,
			.color = sty.color_title_bar,
			.texture = d.blank_texture,
			.layer = layer,
			.corner_radius = menu_radius
		});

		if (d.gui_font.valid() && !current_menu.tab_contents.empty()) {
			d.text_commands.push_back({
				.font = d.gui_font,
				.text = current_menu.tab_contents[0],
				.position = { title_bar_rect.left() + sty.padding, title_bar_rect.center().y() + d.gui_font->vertical_center_offset(sty.font_size) },
				.scale = sty.font_size,
				.clip_rect = title_bar_rect,
				.layer = layer
			});
		}
	}
}

auto gse::gui::system::draw_tab_bar(data& d, const gse::input::state& input_state, menu& current_menu, const ui_rect& title_bar_rect, const render_layer layer) -> void {
	const style& sty = d.fstate.sty;
	const vec2f mouse_pos = input_state.mouse_position();
	const bool mouse_clicked = input_state.mouse_button_pressed(mouse_button::button_1);

	d.sprite_commands.push_back({
		.rect = title_bar_rect,
		.color = sty.color_title_bar,
		.texture = d.blank_texture,
		.layer = layer
	});

	const std::size_t tab_count = current_menu.tab_contents.size();
	if (tab_count == 0) {
		return;
	}

	const float tab_height = sty.title_bar_height - 4.0f;
	const float tab_top = title_bar_rect.top() - 2.0f;
	const float tab_padding_h = sty.padding;
	constexpr float tab_gap = 2.0f;
	constexpr float min_tab_width = 60.0f;
	constexpr float max_tab_width = 200.0f;

	const float available_width = title_bar_rect.width() - sty.padding * 2.0f;
	const float total_gaps = tab_gap * static_cast<float>(tab_count - 1);
	const float width_per_tab = (available_width - total_gaps) / static_cast<float>(tab_count);
	const float tab_width = std::clamp(width_per_tab, min_tab_width, max_tab_width);

	float tab_x = title_bar_rect.left() + sty.padding;

	auto truncate_text = [&d, &sty](const std::string& text, const float max_width) -> std::string {
		if (!d.gui_font.valid()) {
			return text;
		}

		if (const float text_width = d.gui_font->width(text, sty.font_size); text_width <= max_width) {
			return text;
		}

		struct cache_key {
			std::uint64_t text_hash;
			std::uint32_t width_bucket;
			std::uint32_t font_size_bucket;
			auto operator==(
				const cache_key&
			) const -> bool = default;
		};
		struct cache_key_hash {
			auto operator()(const cache_key& k) const -> std::size_t {
				return hash_combine(hash_combine(k.text_hash, k.width_bucket), k.font_size_bucket);
			}
		};
		thread_local std::unordered_map<cache_key, std::string, cache_key_hash> truncation_cache;
		const cache_key key{
			stable_id(text),
			static_cast<std::uint32_t>(max_width),
			static_cast<std::uint32_t>(sty.font_size * 16.f)
		};
		if (const auto it = truncation_cache.find(key); it != truncation_cache.end()) {
			return it->second;
		}

		constexpr std::string_view ellipsis = "...";
		const float ellipsis_width = d.gui_font->width(ellipsis, sty.font_size);
		const float target_width = max_width - ellipsis_width;

		if (target_width <= 0) {
			return std::string(ellipsis);
		}

		std::string truncated;
		truncated.reserve(text.size() + ellipsis.size());
		for (const char c : text) {
			truncated.push_back(c);
			if (d.gui_font->width(truncated, sty.font_size) > target_width) {
				truncated.pop_back();
				break;
			}
		}
		truncated.append(ellipsis);

		const auto [it, _] = truncation_cache.emplace(key, std::move(truncated));
		return it->second;
	};

	for (std::size_t i = 0; i < tab_count; ++i) {
		const std::string& tab_name = current_menu.tab_contents[i];
		const bool is_active = (i == current_menu.active_tab_index);

		const ui_rect tab_rect = ui_rect::from_position_size(
			{ tab_x, tab_top },
			{ tab_width, tab_height }
		);

		const bool is_hovered = tab_rect.contains(mouse_pos);

		if (is_hovered && mouse_clicked && !is_active) {
			current_menu.active_tab_index = static_cast<std::uint32_t>(i);
		}

		vec4f tab_color;
		if (is_active) {
			tab_color = sty.color_menu_body;
		}
		else if (is_hovered) {
			tab_color = vec4f(
				sty.color_title_bar.x() * 1.2f,
				sty.color_title_bar.y() * 1.2f,
				sty.color_title_bar.z() * 1.2f,
				sty.color_title_bar.w()
			);
		}
		else {
			tab_color = sty.color_title_bar_inactive;
		}

		d.sprite_commands.push_back({
			.rect = tab_rect,
			.color = tab_color,
			.texture = d.blank_texture,
			.layer = layer
		});

		if (is_active) {
			const ui_rect connector =
				ui_rect::from_position_size(
					{ tab_rect.left(), title_bar_rect.bottom() },
					{ tab_width, 2.0f }
				);
			d.sprite_commands.push_back({
				.rect = connector,
				.color = sty.color_menu_body,
				.texture = d.blank_texture,
				.layer = layer
			});
		}

		if (d.gui_font.valid()) {
			const float text_max_width = tab_width - tab_padding_h * 2.0f;
			const std::string display_text = truncate_text(tab_name, text_max_width);

			d.text_commands.push_back({
				.font = d.gui_font,
				.text = display_text,
				.position = { tab_rect.left() + tab_padding_h, tab_rect.center().y() + d.gui_font->vertical_center_offset(sty.font_size) },
				.scale = sty.font_size,
				.clip_rect = tab_rect,
				.layer = layer
			});
		}

		tab_x += tab_width + tab_gap;
	}
}

auto gse::gui::system::handle_idle_state(data& d, const gse::input::state& input_state, vec2f mouse_position, const bool mouse_held, const style& style) -> gui::state {
	struct interaction_candidate {
		std::variant<states::resizing, states::dragging, states::resizing_divider, states::pending_drag> future_state;
		cursor::style cursor;
	};

	struct resize_rule {
		std::function<bool(const ui_rect&, const vec2f&)> condition;
		resize_handle handle;
		cursor::style cursor;
	};

	const std::array<resize_rule, 8> resize_rules = { {
		{ [style](
		const ui_rect& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.y() - r.top()) < t && std::abs(p.x() - r.left()) < t;
		 },
		  resize_handle::top_left,
		  cursor::style::resize_nw },
		{ [style](
		const ui_rect& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.y() - r.top()) < t && std::abs(p.x() - r.right()) < t;
		 },
		  resize_handle::top_right,
		  cursor::style::resize_ne },
		{ [style](
		const ui_rect& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.y() - r.bottom()) < t && std::abs(p.x() - r.left()) < t;
		 },
		  resize_handle::bottom_left,
		  cursor::style::resize_sw },
		{ [style](
		const ui_rect& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.y() - r.bottom()) < t && std::abs(p.x() - r.right()) < t;
		 },
		  resize_handle::bottom_right,
		  cursor::style::resize_se },
		{ [style](
		const ui_rect& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.x() - r.left()) < t && p.y() <= r.top() + t && p.y() >= r.bottom() - t;
		 },
		  resize_handle::left,
		  cursor::style::resize_w },
		{ [style](
		const ui_rect& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.x() - r.right()) < t && p.y() <= r.top() + t && p.y() >= r.bottom() - t;
		 },
		  resize_handle::right,
		  cursor::style::resize_e },
		{ [style](
		const ui_rect& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.y() - r.top()) < t && p.x() >= r.left() - t && p.x() <= r.right() + t;
		 },
		  resize_handle::top,
		  cursor::style::resize_n },
		{ [style](
		const ui_rect& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.y() - r.bottom()) < t && p.x() >= r.left() - t && p.x() <= r.right() + t;
		 },
		  resize_handle::bottom,
		  cursor::style::resize_s },
	} };

	auto calculate_group_bounds = [&d](const id root_id) -> ui_rect {
		const menu* root = d.menus.try_get(root_id);
		if (!root) {
			return {};
		}

		ui_rect bounds = root->rect;

		std::function<void(id)> expand = [&](const id parent_id) {
			for (const menu& item : d.menus.items()) {
				if (item.owner_id() == parent_id && item.was_visible_last_frame) {
					bounds = ui_rect::bounding_box(bounds, item.rect);
					expand(item.id());
				}
			}
		};

		expand(root_id);
		return bounds;
	};

	std::vector<menu*> visible_menus;
	visible_menus.reserve(d.visible_menu_ids_last_frame.size());
	for (const id& mid : d.visible_menu_ids_last_frame) {
		if (menu* m = d.menus.try_get(mid)) {
			visible_menus.push_back(m);
		}
	}

	auto hot_item = [&]() -> std::optional<interaction_candidate> {
		for (auto it = visible_menus.rbegin(); it != visible_menus.rend(); ++it) {
			menu& current_menu = **it;

			if (!current_menu.owner_id().exists()) {
				if (current_menu.docked_to == dock::location::none) {
					const ui_rect group_rect = calculate_group_bounds(current_menu.id());

					for (const auto& [condition, handle, cursor] : resize_rules) {
						if (condition(group_rect, mouse_position)) {
							return interaction_candidate{
								.future_state =
									states::resizing{
										.menu_id = current_menu.id(),
										.handle = handle
									},
								.cursor = cursor
							};
						}
					}
				}
				else {
					const ui_rect& rect = current_menu.rect;

					switch (current_menu.docked_to) {
						case dock::location::left:
							if (std::abs(mouse_position.x() - rect.right()) < style.resize_border_thickness) {
								return interaction_candidate{ states::resizing{ current_menu.id(),
																				resize_handle::right },
															  cursor::style::resize_e };
							}
							break;
						case dock::location::right:
							if (std::abs(mouse_position.x() - rect.left()) < style.resize_border_thickness) {
								return interaction_candidate{ states::resizing{ current_menu.id(),
																				resize_handle::left },
															  cursor::style::resize_w };
							}
							break;
						case dock::location::top:
							if (std::abs(mouse_position.y() - rect.bottom()) < style.resize_border_thickness) {
								return interaction_candidate{ states::resizing{ current_menu.id(),
																				resize_handle::bottom },
															  cursor::style::resize_s };
							}
							break;
						case dock::location::bottom:
							if (std::abs(mouse_position.y() - rect.top()) < style.resize_border_thickness) {
								return interaction_candidate{ states::resizing{ current_menu.id(), resize_handle::top },
															  cursor::style::resize_n };
							}
							break;
						default:
							break;
					}
				}
			}
			else {
				if (const menu* parent = d.menus.try_get(current_menu.owner_id())) {
					bool hovering = false;
					auto new_cursor = cursor::style::arrow;
					const ui_rect& r = current_menu.rect;

					switch (current_menu.docked_to) {
						case dock::location::left:
							if (std::abs(mouse_position.x() - r.right()) < style.resize_border_thickness && mouse_position.y() < r.top() && mouse_position.y() > r.bottom()) {
								hovering = true;
								new_cursor = cursor::style::resize_e;
							}
							break;
						case dock::location::right:
							if (std::abs(mouse_position.x() - r.left()) < style.resize_border_thickness && mouse_position.y() < r.top() && mouse_position.y() > r.bottom()) {
								hovering = true;
								new_cursor = cursor::style::resize_w;
							}
							break;
						case dock::location::top:
							if (std::abs(mouse_position.y() - r.bottom()) < style.resize_border_thickness && mouse_position.x() > r.left() && mouse_position.x() < r.right()) {
								hovering = true;
								new_cursor = cursor::style::resize_s;
							}
							break;
						case dock::location::bottom:
							if (std::abs(mouse_position.y() - r.top()) < style.resize_border_thickness && mouse_position.x() > r.left() && mouse_position.x() < r.right()) {
								hovering = true;
								new_cursor = cursor::style::resize_n;
							}
							break;
						default:
							break;
					}

					if (hovering) {
						return interaction_candidate{
							.future_state =
								states::resizing_divider{
									.parent_id = parent->id(),
									.child_id = current_menu.id()
								},
							.cursor = new_cursor
						};
					}
				}
			}

			const ui_rect title_bar_rect = ui_rect::from_position_size(
				{ current_menu.rect.left(), current_menu.rect.top() },
				{ current_menu.rect.width(), style.title_bar_height }
			);

			if (title_bar_rect.contains(mouse_position)) {
				std::optional<std::uint32_t> clicked_tab;

				if (current_menu.tab_contents.size() > 1) {
					const float tab_height = style.title_bar_height - 4.0f;
					const float tab_top = title_bar_rect.top() - 2.0f;
					constexpr float tab_gap = 2.0f;
					constexpr float min_tab_width = 60.0f;
					constexpr float max_tab_width = 200.0f;

					const std::size_t tab_count = current_menu.tab_contents.size();
					const float available_width = title_bar_rect.width() - style.padding * 2.0f;
					const float total_gaps = tab_gap * static_cast<float>(tab_count - 1);
					const float width_per_tab = (available_width - total_gaps) / static_cast<float>(tab_count);
					const float tab_width = std::clamp(width_per_tab, min_tab_width, max_tab_width);

					float tab_x = title_bar_rect.left() + style.padding;

					for (std::size_t i = 0; i < tab_count; ++i) {
						const ui_rect tab_rect =
							ui_rect::from_position_size(
								{ tab_x, tab_top },
								{ tab_width, tab_height }
							);

						if (tab_rect.contains(mouse_position)) {
							clicked_tab = static_cast<std::uint32_t>(i);
							break;
						}

						tab_x += tab_width + tab_gap;
					}
				}

				return interaction_candidate{
					.future_state =
						states::pending_drag{
							.menu_id = current_menu.id(),
							.start_position = mouse_position,
							.offset = current_menu.rect.top_left() - mouse_position,
							.tab_index = clicked_tab
						},
					.cursor = cursor::style::arrow
				};
			}
		}

		return std::nullopt;
	}();

	if (hot_item) {
		set_style(hot_item->cursor);

		if (mouse_held) {
			if (std::holds_alternative<states::dragging>(hot_item->future_state)) {
				const auto& [menu_id, offset] = std::get<states::dragging>(hot_item->future_state);
				if (const menu* m = d.menus.try_get(menu_id); m && m->docked_to != dock::location::none) {
					layout::undock(d.menus, m->id());
				}
			}

			return std::visit(
				[](auto&& arg) -> gui::state {
					return arg;
				},
				hot_item->future_state
			);
		}
	}
	else {
		set_style(cursor::style::arrow);
	}

	return states::idle{};
}

auto gse::gui::system::handle_dragging_state(data& d, const states::dragging& current, const window::data& window_s, const vec2f mouse_position, const bool mouse_held) -> gui::state {
	menu* m = d.menus.try_get(current.menu_id);
	if (!m) {
		set_style(cursor::style::arrow);
		return states::idle{};
	}

	std::vector<menu*> visible_menus;
	visible_menus.reserve(d.visible_menu_ids_last_frame.size());
	for (const id& mid : d.visible_menu_ids_last_frame) {
		if (menu* vm = d.menus.try_get(mid)) {
			visible_menus.push_back(vm);
		}
	}

	if (!mouse_held) {
		if (d.active_dock_space) {
			id potential_dock_parent_id;

			for (auto it = visible_menus.rbegin(); it != visible_menus.rend(); ++it) {
				if (const menu& other_menu = **it; other_menu.id() != current.menu_id && other_menu.rect.contains(mouse_position)) {
					potential_dock_parent_id = other_menu.id();
					break;
				}
			}

			for (const dock::area& area : d.active_dock_space->areas) {
				if (area.rect.contains(mouse_position)) {
					if (potential_dock_parent_id.exists()) {
						if (area.dock_location == dock::location::center) {
							if (menu* parent = d.menus.try_get(potential_dock_parent_id)) {
								parent->tab_contents.insert(
									parent->tab_contents.end(),
									std::make_move_iterator(m->tab_contents.begin()),
									std::make_move_iterator(m->tab_contents.end())
								);
								m->tab_contents.clear();
								parent->active_tab_index = static_cast<std::uint32_t>(parent->tab_contents.size() - 1);
								d.menus.remove(current.menu_id);
							}
						}
						else {
							layout::dock(
								d.menus,
								current.menu_id,
								potential_dock_parent_id,
								area.dock_location
							);
							layout::update(d.menus, m->id());
						}
					}
					else {
						const ui_rect screen_rect = usable_screen_rect(d, window_s);

						if (area.dock_location == dock::location::center) {
							m->rect = screen_rect;
							m->docked_to = dock::location::center;
							m->swap_parent(id());
							layout::update(d.menus, m->id());
						}
						else {
							m->rect = layout::dock_target_rect(screen_rect, area.dock_location, 0.5f);
							m->docked_to = area.dock_location;
							m->swap_parent(id());
							layout::update(d.menus, m->id());
						}
					}

					break;
				}
			}
		}

		d.active_dock_space.reset();
		set_style(cursor::style::arrow);
		return states::idle{};
	}

	set_style(cursor::style::omni_move);

	const ui_rect screen_rect = usable_screen_rect(d, window_s);
	const vec2f old_top_left = m->rect.top_left();
	vec2f new_top_left = mouse_position + current.offset;

	const float max_x = std::max(0.f, screen_rect.width() - m->rect.width());
	new_top_left.x() = std::clamp(new_top_left.x(), 0.f, max_x);

	const float min_y = std::min(m->rect.height(), screen_rect.top());
	new_top_left.y() = std::clamp(new_top_left.y(), min_y, screen_rect.top());

	if (const vec2f delta = new_top_left - old_top_left; delta.x() != 0 || delta.y() != 0) {
		std::function<void(id)> move_group = [&](const id current_id) {
			if (menu* item = d.menus.try_get(current_id)) {
				item->rect = ui_rect::from_position_size(
					item->rect.top_left() + delta,
					item->rect.size()
				);

				for (menu& potential_child : d.menus.items()) {
					if (potential_child.owner_id() == current_id) {
						move_group(potential_child.id());
					}
				}
			}
		};

		move_group(current.menu_id);
	}

	d.active_dock_space.reset();
	bool found_parent_menu = false;

	for (auto it = visible_menus.rbegin(); it != visible_menus.rend(); ++it) {
		menu& other_menu = **it;

		if (other_menu.id() == current.menu_id) {
			continue;
		}

		if (other_menu.rect.contains(mouse_position)) {
			d.active_dock_space = layout::dock_space(other_menu.rect);
			found_parent_menu = true;
			break;
		}
	}

	if (!found_parent_menu) {
		d.active_dock_space = layout::dock_space(screen_rect);
	}

	return current;
}

auto gse::gui::system::handle_resizing_state(data& d, const states::resizing& current, const vec2f mouse_position, const bool mouse_held, const style& style, const window::data& window_s) -> gui::state {
	if (!mouse_held) {
		d.active_dock_space.reset();
		set_style(cursor::style::arrow);
		return states::idle{};
	}

	menu* m = d.menus.try_get(current.menu_id);
	if (!m) {
		set_style(cursor::style::arrow);
		return states::idle{};
	}

	auto handle_to_cursor = [](const resize_handle h) -> cursor::style {
		switch (h) {
			case resize_handle::top_left:
				return cursor::style::resize_nw;
			case resize_handle::top_right:
				return cursor::style::resize_ne;
			case resize_handle::bottom_left:
				return cursor::style::resize_sw;
			case resize_handle::bottom_right:
				return cursor::style::resize_se;
			case resize_handle::left:
				return cursor::style::resize_w;
			case resize_handle::right:
				return cursor::style::resize_e;
			case resize_handle::top:
				return cursor::style::resize_n;
			case resize_handle::bottom:
				return cursor::style::resize_s;
			default:
				return cursor::style::arrow;
		}
	};

	set_style(handle_to_cursor(current.handle));

	auto calculate_group_bounds = [&d](const id root_id) -> ui_rect {
		const menu* root = d.menus.try_get(root_id);
		if (!root) {
			return {};
		}

		ui_rect bounds = root->rect;

		std::function<void(id)> expand = [&](const id parent_id) {
			for (const menu& item : d.menus.items()) {
				if (item.owner_id() == parent_id && item.was_visible_last_frame) {
					bounds = ui_rect::bounding_box(bounds, item.rect);
					expand(item.id());
				}
			}
		};

		expand(root_id);
		return bounds;
	};

	auto calculate_min_required_size = [&d, &style](const id root_id) -> vec2f {
		std::function<vec2f(id)> rec = [&](const id node_id) -> vec2f {
			vec2f req = style.min_menu_size;

			for (const menu& child : d.menus.items()) {
				if (child.owner_id() != node_id || !child.was_visible_last_frame) {
					continue;
				}

				const vec2f c = rec(child.id());

				switch (child.docked_to) {
					case dock::location::left:
					case dock::location::right:
						req.x() += c.x();
						req.y() = std::max(req.y(), c.y());
						break;
					case dock::location::top:
					case dock::location::bottom:
						req.y() += c.y();
						req.x() = std::max(req.x(), c.x());
						break;
					default:
						req.x() = std::max(req.x(), c.x());
						req.y() = std::max(req.y(), c.y());
						break;
				}
			}

			return req;
		};

		return rec(root_id);
	};

	const ui_rect group_rect = calculate_group_bounds(m->id());
	vec2f min_corner = group_rect.min();
	vec2f max_corner = group_rect.max();

	const vec2f subtree_min = calculate_min_required_size(m->id());
	const float min_w = subtree_min.x();
	const float min_h = subtree_min.y();

	float opposing_left = 0.f;
	float opposing_right = std::numeric_limits<float>::max();
	float opposing_top = std::numeric_limits<float>::max();
	float opposing_bottom = 0.f;

	constexpr float dock_gap = 8.f;

	if (m->docked_to != dock::location::none) {
		for (const menu& other : d.menus.items()) {
			if (other.id() == m->id()) {
				continue;
			}
			if (other.owner_id() != m->owner_id()) {
				continue;
			}
			if (other.docked_to == dock::location::none) {
				continue;
			}
			if (!other.was_visible_last_frame) {
				continue;
			}

			const ui_rect other_bounds = calculate_group_bounds(other.id());

			switch (other.docked_to) {
				case dock::location::left:
					opposing_left = std::max(opposing_left, other_bounds.right() + dock_gap);
					break;
				case dock::location::right:
					opposing_right = std::min(opposing_right, other_bounds.left() - dock_gap);
					break;
				case dock::location::top:
					opposing_top = std::min(opposing_top, other_bounds.bottom() - dock_gap);
					break;
				case dock::location::bottom:
					opposing_bottom = std::max(opposing_bottom, other_bounds.top() + dock_gap);
					break;
				default:
					break;
			}
		}
	}

	auto clamp_left = [&](const float x) -> float {
		float result = std::min(x, max_corner.x() - min_w);
		result = std::max(result, opposing_left);
		return result;
	};

	auto clamp_right = [&](const float x) -> float {
		float result = std::max(x, min_corner.x() + min_w);
		result = std::min(result, opposing_right);
		return result;
	};

	auto clamp_bottom = [&](const float y) -> float {
		float result = std::min(y, max_corner.y() - min_h);
		result = std::max(result, opposing_bottom);
		return result;
	};

	auto clamp_top = [&](const float y) -> float {
		float result = std::max(y, min_corner.y() + min_h);
		result = std::min(result, opposing_top);
		return result;
	};

	switch (current.handle) {
		case resize_handle::left:
			min_corner.x() = clamp_left(mouse_position.x());
			break;
		case resize_handle::right:
			max_corner.x() = clamp_right(mouse_position.x());
			break;
		case resize_handle::bottom:
			min_corner.y() = clamp_bottom(mouse_position.y());
			break;
		case resize_handle::top:
			max_corner.y() = clamp_top(mouse_position.y());
			break;
		case resize_handle::bottom_left:
			min_corner.x() = clamp_left(mouse_position.x());
			min_corner.y() = clamp_bottom(mouse_position.y());
			break;
		case resize_handle::bottom_right:
			min_corner.y() = clamp_bottom(mouse_position.y());
			max_corner.x() = clamp_right(mouse_position.x());
			break;
		case resize_handle::top_left:
			min_corner.x() = clamp_left(mouse_position.x());
			max_corner.y() = clamp_top(mouse_position.y());
			break;
		case resize_handle::top_right:
			max_corner.x() = clamp_right(mouse_position.x());
			max_corner.y() = clamp_top(mouse_position.y());
			break;
		default:
			break;
	}

	m->rect = ui_rect({
		.min = min_corner,
		.max = max_corner
	});

	if (!m->owner_id().exists()) {
		const ui_rect screen_rect = usable_screen_rect(d, window_s);

		switch (m->docked_to) {
			case dock::location::left:
			case dock::location::right: {
				const float denom = screen_rect.width();
				const float ratio = denom > 0.f ? (m->rect.width() / denom) : 1.f;
				const float min_ratio = denom > 0.f ? std::min(1.f, min_w / denom) : 0.f;
				m->dock_split_ratio = std::clamp(ratio, min_ratio, 1.f);
				break;
			}
			case dock::location::top:
			case dock::location::bottom: {
				const float denom = screen_rect.height();
				const float ratio = denom > 0.f ? (m->rect.height() / denom) : 1.f;
				const float min_ratio = denom > 0.f ? std::min(1.f, min_h / denom) : 0.f;
				m->dock_split_ratio = std::clamp(ratio, min_ratio, 1.f);
				break;
			}
			default:
				break;
		}
	}

	layout::update(d.menus, m->id());

	return current;
}

auto gse::gui::system::handle_resizing_divider_state(data& d, const states::resizing_divider& current, const vec2f mouse_position, const bool mouse_held, const style& style) -> gui::state {
	menu* parent = d.menus.try_get(current.parent_id);
	menu* child = d.menus.try_get(current.child_id);

	if (!parent || !child) {
		set_style(cursor::style::arrow);
		return states::idle{};
	}

	if (!mouse_held) {
		set_style(cursor::style::arrow);
		return states::idle{};
	}

	const dock::location location = child->docked_to;

	auto location_to_cursor = [](const dock::location loc) -> cursor::style {
		switch (loc) {
			case dock::location::left:
				return cursor::style::resize_e;
			case dock::location::right:
				return cursor::style::resize_w;
			case dock::location::top:
				return cursor::style::resize_s;
			case dock::location::bottom:
				return cursor::style::resize_n;
			default:
				return cursor::style::arrow;
		}
	};

	set_style(location_to_cursor(location));

	const ui_rect combined_rect = ui_rect::bounding_box(parent->rect, child->rect);

	switch (location) {
		case dock::location::left:
		case dock::location::right: {
			if (combined_rect.width() < style.min_menu_size.x() * 2.f) {
				return current;
			}

			const float min_clamp = combined_rect.left() + style.min_menu_size.x();
			const float max_clamp = combined_rect.right() - style.min_menu_size.x();
			const float divider_x = std::clamp(mouse_position.x(), min_clamp, max_clamp);

			if (location == dock::location::left) {
				child->rect = ui_rect({
					.min = { combined_rect.left(), combined_rect.bottom() },
					.max = { divider_x, combined_rect.top() }
				});
				parent->rect = ui_rect({
					.min = { divider_x, combined_rect.bottom() },
					.max = { combined_rect.right(), combined_rect.top() }
				});
			}
			else {
				parent->rect = ui_rect({
					.min = { combined_rect.left(), combined_rect.bottom() },
					.max = { divider_x, combined_rect.top() }
				});
				child->rect = ui_rect({
					.min = { divider_x, combined_rect.bottom() },
					.max = { combined_rect.right(), combined_rect.top() }
				});
			}

			if (combined_rect.width() > 0.f) {
				const float child_width = (location == dock::location::left) ? (divider_x - combined_rect.left())
																			 : (combined_rect.right() - divider_x);
				child->dock_split_ratio = child_width / combined_rect.width();
			}
			break;
		}
		case dock::location::top:
		case dock::location::bottom: {
			if (combined_rect.height() < style.min_menu_size.y() * 2.f) {
				return current;
			}

			const float min_clamp = combined_rect.bottom() + style.min_menu_size.y();
			const float max_clamp = combined_rect.top() - style.min_menu_size.y();
			const float divider_y = std::clamp(mouse_position.y(), min_clamp, max_clamp);

			if (location == dock::location::top) {
				child->rect = ui_rect({
					.min = { combined_rect.left(), divider_y },
					.max = { combined_rect.right(), combined_rect.top() }
				});
				parent->rect = ui_rect({
					.min = { combined_rect.left(), combined_rect.bottom() },
					.max = { combined_rect.right(), divider_y }
				});
			}
			else {
				parent->rect = ui_rect({
					.min = { combined_rect.left(), divider_y },
					.max = { combined_rect.right(), combined_rect.top() }
				});
				child->rect = ui_rect({
					.min = { combined_rect.left(), combined_rect.bottom() },
					.max = { combined_rect.right(), divider_y }
				});
			}

			if (combined_rect.height() > 0.f) {
				const float child_height = (location == dock::location::top) ? (combined_rect.top() - divider_y)
																			 : (divider_y - combined_rect.bottom());
				child->dock_split_ratio = child_height / combined_rect.height();
			}
			break;
		}
		default:
			break;
	}

	layout::update(d.menus, child->id());

	return current;
}

auto gse::gui::system::handle_pending_drag_state(data& d, const states::pending_drag& current, const vec2f mouse_position, const bool mouse_held) -> gui::state {
	if (!mouse_held) {
		return states::idle{};
	}

	const float distance = magnitude(mouse_position - current.start_position);

	if (constexpr float drag_threshold = 5.0f; distance > drag_threshold) {
		menu* m = d.menus.try_get(current.menu_id);
		if (!m) {
			return states::idle{};
		}

		id drag_menu_id = current.menu_id;
		vec2f drag_offset = current.offset;

		if (current.tab_index.has_value() && m->tab_contents.size() > 1) {
			if (const std::uint32_t tab_idx = current.tab_index.value(); tab_idx < m->tab_contents.size()) {
				std::string tab_name = m->tab_contents[tab_idx];

				m->tab_contents.erase(m->tab_contents.begin() + tab_idx);

				if (m->active_tab_index >= m->tab_contents.size()) {
					m->active_tab_index = static_cast<std::uint32_t>(m->tab_contents.size() - 1);
				}
				else if (m->active_tab_index > tab_idx) {
					m->active_tab_index--;
				}

				constexpr vec2f default_size = { 300.f, 200.f };

				const style sty = d.fstate.sty;
				const vec2f new_top_left = { mouse_position.x() - default_size.x() * 0.5f,
											 mouse_position.y() + sty.title_bar_height * 0.5f };

				menu new_menu(
					tab_name,
					menu_data{
						.rect = ui_rect::from_position_size(
							new_top_left,
							default_size
						),
						.parent_id = id()
					}
				);

				const id new_id = new_menu.id();
				d.menus.add(new_id, std::move(new_menu));

				drag_menu_id = new_id;
				drag_offset = { -default_size.x() * 0.5f, sty.title_bar_height * 0.5f };
			}
		}
		else if (m->docked_to != dock::location::none) {
			layout::undock(d.menus, m->id());
		}

		set_style(cursor::style::omni_move);
		return states::dragging{
			.menu_id = drag_menu_id,
			.offset = drag_offset
		};
	}

	return current;
}

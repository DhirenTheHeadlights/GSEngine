module gse.graphics:gui_impl;

import std;

import :gui;
import gse.math;
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
import gse.meta;
import gse.save;

namespace gse::gui {
	[[nodiscard]] auto intern_text(
		data& d,
		std::string_view text
	) -> std::string_view;

	[[nodiscard]] auto popout_close_button_rect(
		const rectf& title_bar_rect,
		const style& sty
	) -> rectf;

	auto remove_tab_from_host(
		data& d,
		std::string_view menu_name
	) -> void;

	auto tab_chrome_height(
		const data& d,
		const menu& m,
		float width
	) -> float;
}

auto gse::gui::intern_text(data& d, const std::string_view text) -> std::string_view {
	std::deque<std::string>& pool = d.text_pools[d.text_pool_slot];
	if (d.text_pool_used == pool.size()) {
		pool.emplace_back();
	}
	std::string& slot = pool[d.text_pool_used++];
	slot.assign(text);
	return slot;
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

auto gse::gui::tab_chrome_height(const data& d, const menu& m, const float width) -> float {
	if (m.tab_contents.size() <= 1 || !d.fonts.text.valid()) {
		return d.fstate.sty.title_bar_height;
	}

	const style& sty = d.fstate.sty;
	const float row_h = d.fonts.text->line_height(sty.font_size) + sty.padding;
	const float tab_gap = 2.f * sty.scale_factor;
	const float scrollbar_h = 6.f * sty.scale_factor;
	const float available_width = std::max(0.f, width - sty.padding * 2.f);

	std::vector<tab_desc> descs;
	descs.reserve(m.tab_contents.size());
	for (const std::string& tag : m.tab_contents) {
		descs.push_back({ .caption = tag });
	}

	const tab_strip_metrics metrics = tab_strip_measure(d.fonts.text, sty, descs, available_width, 60.f, 200.f);
	const std::uint32_t rows = std::max(1u, std::min(metrics.required_rows, std::max(1u, m.tab_bar.visible_rows)));
	const float scrollbar_extra = rows == 1 && metrics.content_extent > available_width ? scrollbar_h + tab_gap : 0.f;
	return static_cast<float>(rows) * row_h + static_cast<float>(rows - 1) * tab_gap + scrollbar_extra + 4.f * sty.scale_factor;
}

auto gse::gui::remove_tab_from_host(data& d, const std::string_view menu_name) -> void {
	id host_id;
	for (const menu& m : d.menus.items()) {
		if (std::ranges::find(m.tab_contents, menu_name) != m.tab_contents.end()) {
			host_id = m.id();
			break;
		}
	}

	menu* host = d.menus.try_get(host_id);
	if (!host) {
		return;
	}

	const auto tab_it = std::ranges::find(host->tab_contents, menu_name);
	const auto removed_idx = static_cast<std::uint32_t>(std::distance(host->tab_contents.begin(), tab_it));
	host->tab_contents.erase(tab_it);

	const bool host_has_children = std::ranges::any_of(d.menus.items(), [host_id](const menu& m) {
		return m.owner_id() == host_id;
	});

	if (host->tab_contents.empty() && !host_has_children) {
		if (host->docked_to != dock::location::none) {
			layout::undock(d.menus, host_id);
		}
		d.menus.remove(host_id);
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

auto gse::gui::init_body(context& ctx, const shared_view<window::data> window_s, const shared_view<asset::data> assets, data& d) -> async::task<> {
	std::vector<std::string> font_names;
	for (const std::string& name : asset::enumerate_resources<font>()) {
		if (gse::exists("Fonts/" + name)) {
			font_names.push_back(name);
		}
	}

	d.ui_font.options = font_names;
	d.code_font.options = font_names;

	const auto index_of = [&](const std::string& name) -> int {
		const auto it = std::ranges::find(d.ui_font.options, name);
		if (it == d.ui_font.options.end()) {
			return 0;
		}
		return static_cast<int>(std::ranges::distance(d.ui_font.options.begin(), it));
	};

	if (d.ui_font.value < 0 || d.ui_font.value >= static_cast<int>(d.ui_font.options.size())) {
		d.ui_font.value = index_of("Inter-Regular");
	}
	if (d.code_font.value < 0 || d.code_font.value >= static_cast<int>(d.code_font.options.size())) {
		d.code_font.value = index_of("MonaspaceNeon-Regular");
	}

	d.blank_texture = asset::queue<texture>(assets, "blank", vec4f(1, 1, 1, 1));

	if (!d.ui_font.options.empty()) {
		const std::string& ui_name = d.ui_font.options[d.ui_font.value];
		const std::string& code_name = d.code_font.options[d.code_font.value];
		d.fonts.text = co_await asset::load<gse::font>(ctx, assets, "Fonts/" + ui_name);
		d.fonts.code = co_await asset::load<gse::font>(ctx, assets, "Fonts/" + code_name);
		d.fonts.registry[ui_name] = d.fonts.text;
		d.fonts.registry[code_name] = d.fonts.code;
	}

	while (asset::resource_state<texture>(assets, d.blank_texture.id()) != resource::state::loaded) {
		co_await ctx.yield_tick();
	}
	d.menus = load(d.file_path, d.menus);

	d.last_ui_font_index = d.ui_font.value;
	d.last_code_font_index = d.code_font.value;

	auto calculate_group_bounds = [&d](const id root_id) -> rectf {
		const menu* root = d.menus.try_get(root_id);
		if (!root) {
			return {};
		}

		rectf bounds = root->rect;

		std::function<void(id)> expand = [&](const id parent_id) {
			for (const menu& item : d.menus.items()) {
				if (item.owner_id() == parent_id) {
					bounds = rectf::bounding_box(bounds, item.rect);
					expand(item.id());
				}
			}
		};

		expand(root_id);
		return bounds;
	};

	const rectf screen_rect = usable_screen_rect(d, window_s);

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

				m.rect = rectf::from_position_size(
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

auto gse::gui::init(context& ctx, const shared_view<window::data> window_s, const shared_view<asset::data> assets_s, data& d) -> async::task<> {
	co_await init_body(ctx, window_s, assets_s, d);
	co_return;
}

auto gse::gui::run(context& ctx, const shared_view<window::data> window_s, const shared_view<gpu::context::data> gpu_s, const shared_view<asset::data> assets_s, const shared_view<gse::input::data> input_state, const save::registry& save_reg, data& d) -> async::task<> {
	co_await update_body(ctx, window_s, gpu_s, assets_s, input_state, save_reg, d);
	co_return;
}

auto gse::gui::clear_menu_interaction(data& d) -> void {
	d.active_dock_space.reset();
	d.current_state = states::idle{};
}

auto gse::gui::update_body(context& ctx, const shared_view<window::data> window_s, const shared_view<gpu::context::data> gpu_s, const shared_view<asset::data> assets_s, const shared_view<gse::input::data> input_state, const save::registry& save_reg, data& d) -> async::task<> {
	const auto current_viewport_size = vec2f(gpu_s.render_graph->extent());

	if (d.previous_viewport_size.x() > 0.f && d.previous_viewport_size.y() > 0.f) {
		if (current_viewport_size.x() > 0.f && current_viewport_size.y() > 0.f && (current_viewport_size.x() != d.previous_viewport_size.x() || current_viewport_size.y() != d.previous_viewport_size.y())) {
			const style old_sty = apply_scale(d, style::from_theme(d.current_theme), d.previous_viewport_size.y());
			const style new_sty = apply_scale(d, style::from_theme(d.current_theme), current_viewport_size.y());

			const float old_usable_height = d.previous_viewport_size.y();
			const float new_usable_height = current_viewport_size.y();

			const float scale_x = current_viewport_size.x() / d.previous_viewport_size.x();
			const float scale_y = new_usable_height / old_usable_height;

			const float top_inset = d.reserve_top_bar ? d.fstate.sty.title_bar_height : 0.f;
			const float new_content_height = std::max(0.f, new_usable_height - top_inset);
			const rectf new_screen_rect = rectf::from_position_size(
				{ 0.f, new_content_height },
				{ current_viewport_size.x(), new_content_height }
			);

			for (menu& m : d.menus.items()) {
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
								std::max(0.f, current_viewport_size.x() - actual_width)
							);
						const float clamped_top = std::clamp(new_top, actual_height, new_usable_height);

						m.rect = rectf::from_position_size(
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
	d.text_pool_slot ^= 1;
	d.text_pool_used = 0;

	d.input_layers_data.begin_frame();

	const style frame_sty = apply_scale(d, style::from_theme(d.current_theme), current_viewport_size.y());

	d.fstate = {
		.sty = frame_sty,
		.active = d.fonts.text.valid()
	};

	d.hot_widget_id = {};

	d.input_layer_render = (d.menu_stack.captures_input() || d.context_menu.open) ? render_layer::popup : render_layer::content;

	d.name_to_menu_id.clear();
	for (menu& m : d.menus.items()) {
		m.was_begun_this_frame = false;
		m.chrome_drawn_this_frame = false;
		for (const std::string& tab : m.tab_contents) {
			d.name_to_menu_id.emplace(stable_id(tab), m.id());
		}
		d.name_to_menu_id.emplace(stable_id(m.id().tag()), m.id());
	}

	if (d.ui_font.value != d.last_ui_font_index || d.code_font.value != d.last_code_font_index) {
		reload_font(d, assets_s);
		d.last_ui_font_index = d.ui_font.value;
		d.last_code_font_index = d.code_font.value;
	}

	const vec2f mouse_position = gse::input::current_state(input_state).mouse_position();
	const bool mouse_held = gse::input::current_state(input_state).mouse_button_held(mouse_button::button_1);

	match(d.current_state.v)
		.if_is([&](const states::idle&) {
			d.current_state = handle_idle_state(
				d,
				gse::input::current_state(input_state),
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
		gui::save(d.menus, d.file_path);
		d.save_clock.reset();
	}

	if (!d.fstate.active) {
		d.fstate = {};
		co_return;
	}

	const gse::input::state& input_st = gse::input::current_state(input_state);
	const auto viewport_size = vec2f(gpu_s.render_graph->extent());

	if (d.active_dock_space) {
		const auto [areas] = d.active_dock_space.value();
		const vec2f mouse_pos = input_st.mouse_position();

		for (const dock::area& area : areas) {
			if (area.rect.contains(mouse_pos)) {
				d.sprite_commands.push_back({
					.rect = area.target,
					.color = d.fstate.sty.color_dock_preview,
					.texture = d.blank_texture,
					.layer = render_layer::overlay,
					.z_order = 10,
				});
				break;
			}
		}

		for (const dock::area& area : areas) {
			d.sprite_commands.push_back({
				.rect = area.rect,
				.color = d.fstate.sty.color_dock_preview,
				.texture = d.blank_texture,
				.layer = render_layer::overlay,
				.z_order = 11,
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

	for (const id& menu_id : d.pending_popout_close_ids) {
		const menu* m = d.menus.try_get(menu_id);
		if (!m) {
			continue;
		}
		const std::string_view category = popout_category_from_tag(m->id().tag());
		if (!category.empty()) {
			ctx.channels.push<popout_toggle>({ .category = std::string(category) });
		}
	}
	d.pending_popout_close_ids.clear();

	for (const auto& req : ctx.read_channel<popout_closed>()) {
		remove_tab_from_host(d, req.menu_name);
	}

	if (d.pending_tab_close.has_value()) {
		const auto [host_id, tab_index] = *d.pending_tab_close;
		d.pending_tab_close.reset();
		if (const menu* host = d.menus.try_get(host_id); host && tab_index < host->tab_contents.size()) {
			const std::string tab_name = host->tab_contents[tab_index];
			if (const std::string_view category = popout_category_from_tag(tab_name); !category.empty()) {
				ctx.channels.push<popout_toggle>({ .category = std::string(category) });
			}
			else {
				remove_tab_from_host(d, tab_name);
			}
		}
	}

	process_context_menu(d, input_st, viewport_size, ctx.channels);

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

	if (d.tooltip.widget_id.exists() && d.tooltip.hover_time >= tooltip_state::show_delay && !d.tooltip.text.empty() && d.fonts.text.valid()) {
		const float padding = d.fstate.sty.padding;
		const float font_size = d.fstate.sty.font_size;
		const float text_width = d.fonts.text->width(d.tooltip.text, font_size);
		const float text_height = d.fonts.text->line_height(font_size);

		const float tooltip_width = text_width + padding * 2.f;
		const float tooltip_height = text_height + padding;

		vec2f tooltip_pos = d.tooltip.position + vec2f(15.f, -15.f);

		if (tooltip_pos.x() + tooltip_width > viewport_size.x()) {
			tooltip_pos.x() = viewport_size.x() - tooltip_width;
		}
		if (tooltip_pos.x() < 0.f) {
			tooltip_pos.x() = 0.f;
		}
		if (tooltip_pos.y() > viewport_size.y()) {
			tooltip_pos.y() = viewport_size.y();
		}
		if (tooltip_pos.y() - tooltip_height < 0.f) {
			tooltip_pos.y() = tooltip_height;
		}

		const rectf tooltip_rect = rectf::from_position_size(
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
			.font = d.fonts.text,
			.text = intern_text(d, d.tooltip.text),
			.position = { tooltip_rect.left() + padding, tooltip_rect.center().y() + d.fonts.text->vertical_center_offset(font_size) },
			.scale = font_size,
			.color = d.fstate.sty.color_text,
			.layer = render_layer::modal,
			.z_order = 100
		});
	}

	d.tooltip.pending_widget_id.reset();

	if (window_s.ui_focus && !window_s.mouse_visible) {
		cursor::render_to(assets_s, d.sprite_commands, input_st.mouse_position());
	}
	else if (window_s.mouse_visible) {
		cursor_shape os_cursor = cursor_shape::arrow;
		switch (cursor::current()) {
			case cursor::style::resize_e:
			case cursor::style::resize_w:
			case cursor::style::resize_ew:
				os_cursor = cursor_shape::resize_ew;
				break;
			case cursor::style::resize_n:
			case cursor::style::resize_s:
				os_cursor = cursor_shape::resize_ns;
				break;
			case cursor::style::resize_nw:
			case cursor::style::resize_se:
				os_cursor = cursor_shape::resize_nwse;
				break;
			case cursor::style::resize_ne:
			case cursor::style::resize_sw:
				os_cursor = cursor_shape::resize_nesw;
				break;
			default:
				break;
		}
		if (os_cursor != cursor_shape::arrow) {
			ctx.channels.push<set_cursor_shape_request>({ .shape = os_cursor });
		}
	}

	d.visible_menu_ids_last_frame.clear();
	d.visible_menu_ids_last_frame.reserve(d.menus.items().size());
	for (menu& m : d.menus.items()) {
		m.was_visible_last_frame = m.was_begun_this_frame;
		if (m.was_begun_this_frame) {
			d.visible_menu_ids_last_frame.push_back(m.id());
		}
	}

	for (menu& a : d.menus.items()) {
		if (!a.was_visible_last_frame || a.docked_to != dock::location::none) {
			continue;
		}
		for (const menu& b : d.menus.items()) {
			if (&a == &b || !b.was_visible_last_frame) {
				continue;
			}
			if (a.z_order < b.z_order &&
				b.rect.contains(a.rect.min()) && b.rect.contains(a.rect.max()) &&
				b.rect.width() * b.rect.height() > a.rect.width() * a.rect.height()) {
				a.z_order = ++d.next_z_order;
				break;
			}
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

auto gse::gui::shutdown(data& d) -> void {
	gui::save(d.menus, d.file_path);
}

auto gse::gui::save(data& d) -> void {
	gui::save(d.menus, d.file_path);
}

auto gse::gui::process_menu(data& d, const gse::input::state& input_state, const std::string& name, const render_layer layer, const std::function<void(builder&)>& build) -> void {
	if (!d.fstate.active) {
		return;
	}

	if (!begin_menu(d, name)) {
		return;
	}

	menu& current_menu = *d.current_menu;
	if (current_menu.z_order == 0) {
		current_menu.z_order = d.next_z_order++;
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
		draw_menu_chrome(d, input_state, current_menu, layer);
		current_menu.chrome_drawn_this_frame = true;
	}

	const auto it = std::ranges::find(current_menu.tab_contents, name);
	const bool is_active_tab = (it != current_menu.tab_contents.end()) &&
		(std::distance(current_menu.tab_contents.begin(), it) ==
		 static_cast<std::ptrdiff_t>(current_menu.active_tab_index));

	if (!is_active_tab) {
		stamp_z();
		end_menu(d);
		return;
	}

	const style& sty = d.fstate.sty;
	const rectf display_rect = calculate_display_rect(d, current_menu);

	d.input_layers_data.register_hit_region(layer, menu_z, display_rect);

	if (input_state.mouse_button_pressed(mouse_button::button_1) &&
		display_rect.contains(input_state.mouse_position()) &&
		d.input_layers_data.input_available_at(layer, menu_z, input_state.mouse_position())) {
		current_menu.z_order = d.next_z_order++;
	}

	const float top_inset = current_menu.bare ? 2.f : tab_chrome_height(d, current_menu, display_rect.width());
	const float body_height = std::max(0.f, display_rect.height() - top_inset);
	const rectf body_rect = rectf::from_position_size(
		{ display_rect.left(), display_rect.top() - top_inset },
		{ display_rect.width(), body_height }
	);

	const bool is_floating = current_menu.docked_to == dock::location::none && !current_menu.owner_id().exists() && !current_menu.bare;
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

	draw_context ctx{
		.current_menu = &current_menu,
		.style = sty,
		.input = input_state,
		.fonts = d.fonts,
		.blank_texture = d.blank_texture,
		.layout_cursor = layout_cursor,
		.sprites = d.sprite_commands,
		.texts = d.text_commands,
		.text_pool = d.text_pools[d.text_pool_slot],
		.text_pool_used = d.text_pool_used,
		.widget_anim_colors = d.widget_anim_colors,
		.widget_scrolls = d.widget_scrolls,
		.current_layer = layer,
		.current_z_order = menu_z,
		.input_layer = d.input_layer_render,
		.hit_regions = &d.input_layers_data,
		.tooltip = &d.tooltip,
		.context_menu = &d.context_menu,
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

	stamp_z();
	end_menu(d);
}

auto gse::gui::begin_menu(data& d, const std::string& name) -> bool {
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
			.rect = rectf({
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

auto gse::gui::end_menu(data& d) -> void {
	d.current_scope.reset();
	d.current_menu = nullptr;
}

auto gse::gui::process_screen(data& d, const gse::input::state& input_state, const vec2f viewport_size) -> void {
	if (!d.fstate.active) {
		return;
	}

	auto* top = d.menu_stack.top();
	if (top == nullptr) {
		return;
	}

	const style& sty = d.fstate.sty;
	const rectf body_rect = top->body_rect(sty, viewport_size);

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

	const rectf content_rect = body_rect.inset({ sty.padding, sty.padding });
	vec2f layout_cursor = content_rect.top_left();

	ids::scope screen_scope(d.screen_surface->id().number());

	draw_context ctx{
		.current_menu = &*d.screen_surface,
		.style = sty,
		.input = input_state,
		.fonts = d.fonts,
		.blank_texture = d.blank_texture,
		.layout_cursor = layout_cursor,
		.sprites = d.sprite_commands,
		.texts = d.text_commands,
		.text_pool = d.text_pools[d.text_pool_slot],
		.text_pool_used = d.text_pool_used,
		.widget_anim_colors = d.widget_anim_colors,
		.widget_scrolls = d.widget_scrolls,
		.current_layer = render_layer::popup,
		.input_layer = d.input_layer_render,
		.hit_regions = &d.input_layers_data,
		.tooltip = &d.tooltip,
		.context_menu = &d.context_menu,
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

	if (top->dismissable()
		&& input_state.mouse_button_pressed(mouse_button::button_1)
		&& !body_rect.contains(input_state.mouse_position())) {
		ctx.consume_press(mouse_button::button_1);
		d.menu_stack.pop();
		d.context = nullptr;
		return;
	}

	d.menu_stack.tick(b);

	d.context = nullptr;
}

auto gse::gui::usable_screen_rect(data& d, const shared_view<window::data> window_s) -> rectf {
	const auto viewport_size = vec2f(window::viewport(window_s));
	const float top_inset = d.reserve_top_bar ? d.fstate.sty.title_bar_height : 0.f;
	const float usable_height = std::max(0.f, viewport_size.y() - top_inset);
	return rectf::from_position_size(
		{ 0.f, usable_height },
		{ viewport_size.x(), usable_height }
	);
}

auto gse::gui::calculate_display_rect(data& d, const menu& m) -> rectf {
	rectf display_rect = m.rect;

	for (const menu& child : d.menus.items()) {
		if (child.owner_id() == m.id() && !child.was_begun_this_frame && child.was_visible_last_frame) {
			display_rect = rectf::bounding_box(display_rect, calculate_display_rect(d, child));
		}
	}

	return display_rect;
}

auto gse::gui::apply_scale(const data& d, style sty, const float viewport_height) -> style {
	constexpr float reference_height = 1080.f;
	const float base_scale = d.scale_with_resolution ? viewport_height / reference_height : 1.f;
	const float final_scale = base_scale * d.ui_scale;

	sty.scale_factor = final_scale;

	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^style, std::meta::access_context::unchecked()))) {
		if constexpr (has_annotation<scaled_tag>(m)) {
			sty.[:m:] *= final_scale;
		}
	}

	return sty;
}

auto gse::gui::reload_font(data& d, const shared_view<asset::data> assets) -> void {
	if (d.ui_font.value >= 0 && d.ui_font.value < static_cast<int>(d.ui_font.options.size())) {
		if (const std::string& name = d.ui_font.options[d.ui_font.value]; gse::exists("Fonts/" + name)) {
			d.fonts.text = asset::get<font>(assets, "Fonts/" + name);
			d.fonts.registry[name] = d.fonts.text;
		}
	}
	if (d.code_font.value >= 0 && d.code_font.value < static_cast<int>(d.code_font.options.size())) {
		if (const std::string& name = d.code_font.options[d.code_font.value]; gse::exists("Fonts/" + name)) {
			d.fonts.code = asset::get<font>(assets, "Fonts/" + name);
			d.fonts.registry[name] = d.fonts.code;
		}
	}
}

auto gse::gui::draw_menu_chrome(data& d, const gse::input::state& input_state, menu& current_menu, const render_layer layer) -> void {
	const style& sty = d.fstate.sty;

	const rectf display_rect = calculate_display_rect(d, current_menu);
	const bool is_floating = current_menu.docked_to == dock::location::none && !current_menu.owner_id().exists() && !current_menu.bare;
	const float menu_radius = is_floating ? sty.corner_radius_menu : 0.f;

	const float top_inset = current_menu.bare ? 2.f : tab_chrome_height(d, current_menu, display_rect.width());

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

		if (d.fonts.text.valid() && !current_menu.tab_contents.empty()) {
			d.text_commands.push_back({
				.font = d.fonts.text,
				.text = intern_text(d, current_menu.tab_contents[0]),
				.position = { title_bar_rect.left() + sty.padding, title_bar_rect.center().y() + d.fonts.text->vertical_center_offset(sty.font_size) },
				.scale = sty.font_size,
				.clip_rect = title_bar_rect,
				.layer = layer
			});
		}
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
		interaction::mark_hot(d.hot_widget_id, close_id, hovered);
		const bool click = interaction::activate_on_click(d.active_widget_id, close_id, hovered, hovered && pressed, released);

		const vec4f bg_color = hovered ? sty.color_widget_hovered : vec4f{ 0.f, 0.f, 0.f, 0.f };
		d.sprite_commands.push_back({
			.rect = close_rect,
			.color = bg_color,
			.texture = d.blank_texture,
			.layer = layer,
			.z_order = 1,
			.corner_radius = close_rect.width() * 0.5f,
		});

		symbol::draw(d.sprite_commands, d.blank_texture, symbol::close(), close_rect, {
			.color = hovered ? sty.color_icon_hovered : sty.color_icon,
			.extent = sty.icon_extent,
			.layer = layer,
			.z_order = 2,
			.clip_rect = close_rect,
		});

		if (click) {
			d.pending_popout_close_ids.push_back(current_menu.id());
		}
	}
}

auto gse::gui::draw_tab_bar(data& d, const gse::input::state& input_state, menu& current_menu, const rectf& title_bar_rect, const render_layer layer) -> void {
	const style& sty = d.fstate.sty;

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
			.id = i + 1,
			.caption = current_menu.tab_contents[i],
		});
	}

	vec2f dummy_cursor{};
	draw_context ctx{
		.current_menu = &current_menu,
		.style = sty,
		.input = input_state,
		.fonts = d.fonts,
		.blank_texture = d.blank_texture,
		.layout_cursor = dummy_cursor,
		.sprites = d.sprite_commands,
		.texts = d.text_commands,
		.text_pool = d.text_pools[d.text_pool_slot],
		.text_pool_used = d.text_pool_used,
		.widget_anim_colors = d.widget_anim_colors,
		.widget_scrolls = d.widget_scrolls,
		.current_layer = layer,
		.current_z_order = 0,
		.input_layer = d.input_layer_render,
		.hit_regions = &d.input_layers_data,
		.tooltip = &d.tooltip,
		.context_menu = &d.context_menu,
		.clip_stack = { title_bar_rect },
	};

	const tab_strip_result tabs = tab_strip(ctx, {
		.area = title_bar_rect,
		.tabs = descs,
		.active = current_menu.active_tab_index + 1,
		.orientation = tab_orientation::horizontal,
		.overflow = tab_overflow::wrap,
		.min_tab_extent = 60.f,
		.max_tab_extent = 200.f,
	}, current_menu.tab_bar);

	if (tabs.activated != 0) {
		current_menu.active_tab_index = static_cast<std::uint32_t>(tabs.activated - 1);
	}
	if (tabs.close_requested != 0) {
		d.pending_tab_close = std::pair{ current_menu.id(), static_cast<std::uint32_t>(tabs.close_requested - 1) };
	}
}

auto gse::gui::process_context_menu(data& d, const gse::input::state& input_state, const vec2f viewport_size, channel_writer& channels) -> void {
	context_menu_state& cm = d.context_menu;
	if (!cm.open) {
		return;
	}
	if (!d.fonts.text.valid() || cm.items.empty()) {
		cm.open = false;
		return;
	}

	const style& sty = d.fstate.sty;
	const vec2f mouse = input_state.mouse_position();
	constexpr render_layer layer = render_layer::popup;
	constexpr std::uint32_t base_z = 4000;

	const float row_h = d.fonts.text->line_height(sty.font_size) + sty.padding * 0.5f;
	const float sep_h = sty.padding * 0.5f;

	float max_label = 0.f;
	for (const menu_item& it : cm.items) {
		max_label = std::max(max_label, d.fonts.text->width(it.label, sty.font_size));
	}
	const bool any_icon = std::ranges::any_of(cm.items, [](const menu_item& it) { return it.icon != nullptr; });
	const float icon_col = any_icon ? sty.font_size : 0.f;
	const float width = max_label + icon_col + sty.padding * 4.f;

	float total_h = sty.padding;
	for (const menu_item& it : cm.items) {
		total_h += (it.separator_before ? sep_h : 0.f) + row_h;
	}

	vec2f pos = cm.position;
	pos.x() = std::clamp(pos.x(), 2.f, std::max(2.f, viewport_size.x() - width - 2.f));
	pos.y() = std::clamp(pos.y(), total_h + 2.f, std::max(total_h + 2.f, viewport_size.y() - 2.f));

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
		.color = { sty.color_menu_body.x(), sty.color_menu_body.y(), sty.color_menu_body.z(), 1.0f },
		.texture = d.blank_texture,
		.layer = layer,
		.z_order = base_z + 1,
		.corner_radius = sty.corner_radius_menu,
	});

	float y = panel.top() - sty.padding * 0.5f;
	bool selected = false;
	const bool left_pressed = input_state.mouse_button_pressed(mouse_button::button_1);
	const bool left_released = input_state.mouse_button_released(mouse_button::button_1);
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
			hash_combine(cm.tag.number(), cm.target),
			hash_combine(stable_id("context_menu_row"), static_cast<std::uint64_t>(i))
		));
		const bool activated = interaction::activate_on_click(d.active_widget_id, row_id, hovered, hovered && left_pressed, left_released);

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
			.position = { row.left() + sty.padding * 1.5f + icon_col, row.center().y() + d.fonts.text->vertical_center_offset(sty.font_size) },
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

auto gse::gui::handle_idle_state(data& d, const gse::input::state& input_state, vec2f mouse_position, const bool mouse_held, const style& style) -> gui::state {
	if (d.menu_stack.captures_input() || d.active_widget_id.exists()) {
		set_style(cursor::style::arrow);
		return states::idle{};
	}

	struct interaction_candidate {
		std::variant<states::resizing, states::dragging, states::resizing_divider, states::pending_drag> future_state;
		cursor::style cursor;
	};

	struct resize_rule {
		std::function<bool(const rectf&, const vec2f&)> condition;
		resize_handle handle;
		cursor::style cursor;
	};

	const std::array<resize_rule, 8> resize_rules = { {
		{ [style](
		const rectf& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.y() - r.top()) < t && std::abs(p.x() - r.left()) < t;
		 },
		  resize_handle::top_left,
		  cursor::style::resize_nw },
		{ [style](
		const rectf& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.y() - r.top()) < t && std::abs(p.x() - r.right()) < t;
		 },
		  resize_handle::top_right,
		  cursor::style::resize_ne },
		{ [style](
		const rectf& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.y() - r.bottom()) < t && std::abs(p.x() - r.left()) < t;
		 },
		  resize_handle::bottom_left,
		  cursor::style::resize_sw },
		{ [style](
		const rectf& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.y() - r.bottom()) < t && std::abs(p.x() - r.right()) < t;
		 },
		  resize_handle::bottom_right,
		  cursor::style::resize_se },
		{ [style](
		const rectf& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.x() - r.left()) < t && p.y() <= r.top() + t && p.y() >= r.bottom() - t;
		 },
		  resize_handle::left,
		  cursor::style::resize_w },
		{ [style](
		const rectf& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.x() - r.right()) < t && p.y() <= r.top() + t && p.y() >= r.bottom() - t;
		 },
		  resize_handle::right,
		  cursor::style::resize_e },
		{ [style](
		const rectf& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.y() - r.top()) < t && p.x() >= r.left() - t && p.x() <= r.right() + t;
		 },
		  resize_handle::top,
		  cursor::style::resize_n },
		{ [style](
		const rectf& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.y() - r.bottom()) < t && p.x() >= r.left() - t && p.x() <= r.right() + t;
		 },
		  resize_handle::bottom,
		  cursor::style::resize_s },
	} };

	auto calculate_group_bounds = [&d](const id root_id) -> rectf {
		const menu* root = d.menus.try_get(root_id);
		if (!root) {
			return {};
		}

		rectf bounds = root->rect;

		std::function<void(id)> expand = [&](const id parent_id) {
			for (const menu& item : d.menus.items()) {
				if (item.owner_id() == parent_id && item.was_visible_last_frame) {
					bounds = rectf::bounding_box(bounds, item.rect);
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
		const bool resize_blocked = d.input_layers_data.is_resize_blocked(mouse_position);
		for (auto it = visible_menus.rbegin(); it != visible_menus.rend(); ++it) {
			menu& current_menu = **it;

			if (current_menu.fixed) {
				continue;
			}

			if (!current_menu.owner_id().exists()) {
				if (current_menu.docked_to == dock::location::none) {
					const rectf group_rect = calculate_group_bounds(current_menu.id());

					for (const auto& [condition, handle, cursor] : resize_rules) {
						if (condition(group_rect, mouse_position) && !resize_blocked) {
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
					const rectf& rect = current_menu.rect;

					switch (current_menu.docked_to) {
						case dock::location::left:
							if (std::abs(mouse_position.x() - rect.right()) < style.resize_border_thickness && !resize_blocked) {
								return interaction_candidate{ states::resizing{ current_menu.id(),
																				resize_handle::right },
															  cursor::style::resize_e };
							}
							break;
						case dock::location::right:
							if (std::abs(mouse_position.x() - rect.left()) < style.resize_border_thickness && !resize_blocked) {
								return interaction_candidate{ states::resizing{ current_menu.id(),
																				resize_handle::left },
															  cursor::style::resize_w };
							}
							break;
						case dock::location::top:
							if (std::abs(mouse_position.y() - rect.bottom()) < style.resize_border_thickness && !resize_blocked) {
								return interaction_candidate{ states::resizing{ current_menu.id(),
																				resize_handle::bottom },
															  cursor::style::resize_s };
							}
							break;
						case dock::location::bottom:
							if (std::abs(mouse_position.y() - rect.top()) < style.resize_border_thickness && !resize_blocked) {
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
					const rectf& r = current_menu.rect;

					switch (current_menu.docked_to) {
						case dock::location::left:
							if (std::abs(mouse_position.x() - r.right()) < style.resize_border_thickness && mouse_position.y() < r.top() && mouse_position.y() > r.bottom() && !resize_blocked) {
								hovering = true;
								new_cursor = cursor::style::resize_e;
							}
							break;
						case dock::location::right:
							if (std::abs(mouse_position.x() - r.left()) < style.resize_border_thickness && mouse_position.y() < r.top() && mouse_position.y() > r.bottom() && !resize_blocked) {
								hovering = true;
								new_cursor = cursor::style::resize_w;
							}
							break;
						case dock::location::top:
							if (std::abs(mouse_position.y() - r.bottom()) < style.resize_border_thickness && mouse_position.x() > r.left() && mouse_position.x() < r.right() && !resize_blocked) {
								hovering = true;
								new_cursor = cursor::style::resize_s;
							}
							break;
						case dock::location::bottom:
							if (std::abs(mouse_position.y() - r.top()) < style.resize_border_thickness && mouse_position.x() > r.left() && mouse_position.x() < r.right() && !resize_blocked) {
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

			const rectf title_bar_rect = rectf::from_position_size(
				{ current_menu.rect.left(), current_menu.rect.top() },
				{ current_menu.rect.width(), current_menu.bare ? 2.f : tab_chrome_height(d, current_menu, current_menu.rect.width()) }
			);

			if (title_bar_rect.contains(mouse_position)) {
				if (is_popout_menu_tag(current_menu.id().tag())) {
					const rectf close_rect = popout_close_button_rect(title_bar_rect, style);
					if (close_rect.contains(mouse_position)) {
						return std::nullopt;
					}
				}

				std::optional<std::uint32_t> clicked_tab;

				if (current_menu.tab_contents.size() > 1 && d.fonts.text.valid()) {
					std::vector<tab_desc> descs;
					descs.reserve(current_menu.tab_contents.size());
					for (std::size_t i = 0; i < current_menu.tab_contents.size(); ++i) {
						descs.push_back({ .id = i + 1, .caption = current_menu.tab_contents[i] });
					}
					const std::vector<tab_strip_placement> placements = tab_strip_layout(d.fonts.text, d.fstate.sty, title_bar_rect, descs, current_menu.tab_bar, tab_overflow::wrap, 60.f, 200.f);
					for (const tab_strip_placement& p : placements) {
						if (p.rect.intersection(title_bar_rect).contains(mouse_position)) {
							clicked_tab = static_cast<std::uint32_t>(p.index);
							break;
						}
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

auto gse::gui::handle_dragging_state(data& d, const states::dragging& current, const shared_view<window::data> window_s, const vec2f mouse_position, const bool mouse_held) -> gui::state {
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
							layout::dock(d.menus, current.menu_id, potential_dock_parent_id, area.dock_location);
							layout::update(d.menus, m->id());
						}
					}
					else {
						const rectf screen_rect = usable_screen_rect(d, window_s);

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

	const rectf screen_rect = usable_screen_rect(d, window_s);
	const vec2f old_top_left = m->rect.top_left();
	vec2f new_top_left = mouse_position + current.offset;

	const float max_x = std::max(0.f, screen_rect.width() - m->rect.width());
	new_top_left.x() = std::clamp(new_top_left.x(), 0.f, max_x);

	const float min_y = std::min(m->rect.height(), screen_rect.top());
	new_top_left.y() = std::clamp(new_top_left.y(), min_y, screen_rect.top());

	if (const vec2f delta = new_top_left - old_top_left; delta.x() != 0 || delta.y() != 0) {
		std::function<void(id)> move_group = [&](const id current_id) {
			if (menu* item = d.menus.try_get(current_id)) {
				item->rect = rectf::from_position_size(item->rect.top_left() + delta, item->rect.size());

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

auto gse::gui::handle_resizing_state(data& d, const states::resizing& current, const vec2f mouse_position, const bool mouse_held, const style& style, const shared_view<window::data> window_s) -> gui::state {
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

	auto calculate_group_bounds = [&d](const id root_id) -> rectf {
		const menu* root = d.menus.try_get(root_id);
		if (!root) {
			return {};
		}

		rectf bounds = root->rect;

		std::function<void(id)> expand = [&](const id parent_id) {
			for (const menu& item : d.menus.items()) {
				if (item.owner_id() == parent_id && item.was_visible_last_frame) {
					bounds = rectf::bounding_box(bounds, item.rect);
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

	const rectf group_rect = calculate_group_bounds(m->id());
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

			const rectf other_bounds = calculate_group_bounds(other.id());

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

	m->rect = rectf({
		.min = min_corner,
		.max = max_corner
	});

	if (!m->owner_id().exists()) {
		const rectf screen_rect = usable_screen_rect(d, window_s);

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

auto gse::gui::handle_resizing_divider_state(data& d, const states::resizing_divider& current, const vec2f mouse_position, const bool mouse_held, const style& style) -> gui::state {
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

	const rectf combined_rect = rectf::bounding_box(parent->rect, child->rect);

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
				child->rect = rectf({
					.min = { combined_rect.left(), combined_rect.bottom() },
					.max = { divider_x, combined_rect.top() }
				});
				parent->rect = rectf({
					.min = { divider_x, combined_rect.bottom() },
					.max = { combined_rect.right(), combined_rect.top() }
				});
			}
			else {
				parent->rect = rectf({
					.min = { combined_rect.left(), combined_rect.bottom() },
					.max = { divider_x, combined_rect.top() }
				});
				child->rect = rectf({
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
				child->rect = rectf({
					.min = { combined_rect.left(), divider_y },
					.max = { combined_rect.right(), combined_rect.top() }
				});
				parent->rect = rectf({
					.min = { combined_rect.left(), combined_rect.bottom() },
					.max = { combined_rect.right(), divider_y }
				});
			}
			else {
				parent->rect = rectf({
					.min = { combined_rect.left(), divider_y },
					.max = { combined_rect.right(), combined_rect.top() }
				});
				child->rect = rectf({
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

auto gse::gui::handle_pending_drag_state(data& d, const states::pending_drag& current, const vec2f mouse_position, const bool mouse_held) -> gui::state {
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
						.rect = rectf::from_position_size(new_top_left, default_size),
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

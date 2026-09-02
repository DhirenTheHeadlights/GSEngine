module gse.graphics:gui_impl;

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

auto gse::gui::is_popout(const viewport_state& vp) -> bool {
	return vp.window.exists();
}

auto gse::gui::init(context& ctx, const shared_view<window::data> window_s, const shared_view<asset::data> assets_s, data& d) -> async::task<> {
	std::vector<std::string> font_names;
	for (const std::string& tag : asset::enumerate_resources<font>()) {
		if (exists(tag)) {
			font_names.push_back(tag);
		}
	}

	d.ui_font.options = font_names;
	d.code_font.options = font_names;

	const std::string built_in_fonts(config::engine_asset_prefix);

	const auto resolve = [&font_names](const std::string& current, const std::string& fallback) -> std::string {
		if (std::ranges::find(font_names, current) != font_names.end()) {
			return current;
		}
		if (std::ranges::find(font_names, fallback) != font_names.end()) {
			return fallback;
		}
		return font_names.empty() ? std::string{} : font_names.front();
	};

	d.ui_font.value = resolve(d.ui_font.value, built_in_fonts + "Fonts/Geist-Regular");
	d.code_font.value = resolve(d.code_font.value, built_in_fonts + "Fonts/GeistMono-Regular");

	d.blank_texture = asset::queue<texture>(assets_s, "blank", vec4f(1, 1, 1, 1));

	const bool fonts_requested = !d.ui_font.value.empty() && !d.code_font.value.empty();
	const std::string& ui_name = d.ui_font.value;
	const std::string& code_name = d.code_font.value;

	if (fonts_requested) {
		assign_faces(d.fonts, assets_s, ui_name, code_name);
	}

	auto settled = [](const auto& handle) {
		return handle.valid() || handle.state() == resource::state::failed;
	};
	auto failed = [](const auto& handle, const std::string_view what) {
		if (handle.state() != resource::state::failed) {
			return false;
		}
		const auto error = handle.error();
		assert(false, "Unable to load {}: {}", what, error ? error->detail : "Unknown asset error");
		return true;
	};

	while (!settled(d.blank_texture) || (fonts_requested && (!settled(d.fonts.text) || !settled(d.fonts.code)))) {
		co_await ctx.yield_tick();
	}

	if (failed(d.blank_texture, "blank texture") || (fonts_requested && (failed(d.fonts.text, "UI font") || failed(d.fonts.code, "code font")))) {
		co_return;
	}

	d.primary.display_scale = window_s.primary.content_scale;
	d.ui_scale_by_monitor = load_ui_scales(d.file_path);
	sync_monitor_scale(d, d.primary, window_s.primary.monitor_key);

	const auto viewport_size = vec2f(window::viewport(window_s));
	d.primary.frame_rect = rectf::from_position_size({ 0.f, viewport_size.y() }, viewport_size);
	d.primary.menus = load(d.file_path, d.primary.menus, viewport_size, scale_factor_for(d, d.primary, viewport_size.y()));

	d.last_ui_font = d.ui_font.value;
	d.last_code_font = d.code_font.value;

	auto calculate_group_bounds = [&d](const id root_id) -> rectf {
		const menu* root = d.primary.menus.try_get(root_id);
		if (!root) {
			return {};
		}

		rectf bounds = root->rect;

		auto expand = [&](this auto& self, const id parent_id) -> void {
			for (const menu& item : d.primary.menus.items()) {
				if (item.owner_id() == parent_id) {
					bounds = rectf::bounding_box(bounds, item.rect);
					self(item.id());
				}
			}
		};

		expand(root_id);
		return bounds;
	};

	d.primary.rect = usable_screen_rect(d.reserve_top_bar ? d.primary.fstate.sty.title_bar_height : 0.f, d.primary.frame_rect);
	const rectf screen_rect = d.primary.rect;

	for (menu& m : d.primary.menus.items()) {
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

			layout::update(d.primary.menus, m.id());
		}
	}

	d.primary.visible_menu_ids_last_frame.clear();
	d.primary.visible_menu_ids_last_frame.reserve(d.primary.menus.items().size());

	for (menu& m : d.primary.menus.items()) {
		d.primary.visible_menu_ids_last_frame.push_back(m.id());
	}

	d.primary.previous_viewport_size = vec2f(window::viewport(window_s));
	d.primary.previous_scale_factor = scale_factor_for(d, d.primary, d.primary.previous_viewport_size.y());
}

auto gse::gui::run(context& ctx, const shared_view<window::data> window_s, const shared_view<gpu::context::data> gpu_s, const shared_view<asset::data> assets_s, const shared_view<input::data> input_state, const channel_read<push_screen_request, pop_screen_request, clear_screens_request, set_manual_cursor_request, menu_content, popout_closed, menu_migrate_request, window_opened, window_closed, window_resized> requests_in, const channel_write<ui_focus_request, popout_toggle, set_cursor_shape_request, renderer::sprite_command, renderer::text_command, context_menu_result, window_close_request, window_minimize_request, window_toggle_maximize_request, window_chrome_metrics_request> ui_out, data& d) -> async::task<> {
	const auto current_viewport_size = vec2f(gpu_s.render_graph->extent());
	const auto window_size = vec2f(window::viewport(window_s));

	d.primary.frame_rect = rectf::from_position_size({ 0.f, window_size.y() }, window_size);
	for (const auto& req : requests_in.of<window_opened>()) {
		auto vp = std::make_unique<viewport_state>();
		vp->window = req.id;
		vp->frame_rect = rectf::from_position_size(
			{ 0.f, static_cast<float>(req.size.y()) },
			vec2f(req.size)
		);
		viewport_state& created = *vp;
		d.secondaries.push_back(std::move(vp));

		if (!req.for_menu.empty()) {
			adopt_menu(d, created, req.for_menu);
		}
	}

	for (const auto& req : requests_in.of<window_resized>()) {
		if (viewport_state* vp = viewport_for_window(d, req.id)) {
			vp->frame_rect = rectf::from_position_size(
				{ 0.f, static_cast<float>(req.size.y()) },
				vec2f(req.size)
			);
		}
	}

	for (const auto& req : requests_in.of<window_closed>()) {
		close_window_viewport(d, req.id);
	}

	for (const auto& req : requests_in.of<menu_migrate_request>()) {
		if (viewport_state* target = viewport_for_window(d, req.target_window)) {
			adopt_menu(d, *target, req.menu_name);
		}
	}

	route_cursor(d, input::current_state(input_state).mouse_position(), window_s.focused_window, window_s.cursor_window);

	d.sprite_commands.clear();
	d.text_commands.clear();
	d.text_pool_slot ^= 1;
	d.text_pool_used = 0;

	begin_viewport_frame(d, d.primary, window_s, current_viewport_size, input::current_state(input_state));
	for (const auto& vp : d.secondaries) {
		begin_viewport_frame(d, *vp, window_s, vp->frame_rect.size(), input::current_state(input_state));
	}

	if (d.ui_font.value != d.last_ui_font || d.code_font.value != d.last_code_font) {
		reload_font(d, assets_s);
		d.last_ui_font = d.ui_font.value;
		d.last_code_font = d.code_font.value;
	}

	update_viewport_interaction(d, d.primary, window_s, input_state);
	for (const auto& vp : d.secondaries) {
		update_viewport_interaction(d, *vp, window_s, input_state);
	}

	if (d.save_clock.elapsed() > data::update_interval) {
		save(d);
		d.save_clock.reset();
	}

	if (!d.primary.fstate.active) {
		d.primary.fstate = {};
		for (const auto& vp : d.secondaries) {
			vp->fstate = {};
		}
		co_return;
	}

	const input::state& input_st = input::current_state(input_state);

	for (const auto& req : requests_in.of<push_screen_request>()) {
		if (viewport_state* target = viewport_for_window(d, req.window)) {
			target->menu_stack.push_factory(req.factory);
		}
	}
	for ([[maybe_unused]] const auto& req : requests_in.of<pop_screen_request>()) {
		d.primary.menu_stack.pop();
	}
	for ([[maybe_unused]] const auto& req : requests_in.of<clear_screens_request>()) {
		d.primary.menu_stack.clear();
	}
	for (const auto& req : requests_in.of<set_manual_cursor_request>()) {
		d.primary.manual_cursor = req.show;
	}

	ui_out.push<ui_focus_request>({
		.focus = !d.primary.menu_stack.empty() || d.primary.manual_cursor,
	});

	auto stamp_viewport = [&d](const std::size_t sprite_start, const std::size_t text_start, const id window, const rectf& bounds) {
		auto stamp = [&](auto& commands, const std::size_t start) {
			for (std::size_t i = start; i < commands.size(); ++i) {
				auto& cmd = commands[i];
				cmd.window = window;
				cmd.clip_rect = cmd.clip_rect ? cmd.clip_rect->intersection(bounds) : bounds;
			}
		};
		stamp(d.sprite_commands, sprite_start);
		stamp(d.text_commands, text_start);
	};

	auto select_text_in_viewport = [&d, &input_st](viewport_state& vp, const std::size_t text_start) {
		update_text_selection({
			.selection = vp.text_selection,
			.input = input_st,
			.layers = vp.input_layers_data,
			.context_menu = vp.context_menu,
			.fonts = d.fonts,
			.style = vp.fstate.sty,
			.blank_texture = d.blank_texture,
			.texts = std::span(d.text_commands).subspan(text_start),
			.sprites = d.sprite_commands,
			.bounds = vp.frame_rect,
			.gesture = vp.current_state,
		});
	};

	const std::size_t primary_sprite_start = d.sprite_commands.size();
	const std::size_t primary_text_start = d.text_commands.size();
	update_viewport(d, d.primary, input_st, requests_in, ui_out);
	select_text_in_viewport(d.primary, primary_text_start);
	stamp_viewport(primary_sprite_start, primary_text_start, d.primary.window, d.primary.frame_rect);

	for (std::size_t i = 0; i < d.secondaries.size(); ++i) {
		viewport_state& vp = *d.secondaries[i];
		const std::size_t sprite_start = d.sprite_commands.size();
		const std::size_t text_start = d.text_commands.size();
		update_viewport(d, vp, input_st, requests_in, ui_out);
		select_text_in_viewport(vp, text_start);
		stamp_viewport(sprite_start, text_start, vp.window, vp.frame_rect);
	}

	if (window_s.primary.ui_focus && !window_s.mouse_visible) {
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
			ui_out.push<set_cursor_shape_request>({ .shape = os_cursor });
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
		ui_out.push<renderer::sprite_command>(std::move(cmd));
	}

	for (auto& cmd : d.text_commands) {
		ui_out.push<renderer::text_command>(std::move(cmd));
	}

	d.primary.fstate = {};
	for (const auto& vp : d.secondaries) {
		vp->fstate = {};
	}

	co_return;
}

auto gse::gui::shutdown(data& d) -> void {
	save(d);
}

auto gse::gui::save(data& d) -> void {
	save(d.primary.menus, d.file_path, d.primary.previous_viewport_size, scale_factor_for(d, d.primary, d.primary.previous_viewport_size.y()));
	save_ui_scales(d.ui_scale_by_monitor, d.file_path);
}
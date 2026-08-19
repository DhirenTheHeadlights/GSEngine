export module gse.ide.app:editor_app;

import std;
import gse;
import gse.gpu;

import gse.ide.workspace;
import gse.ide.agent;
import gse.ide.git;
import gse.ide.terminal;
import gse.ide.build;
import gse.ide.config;
import gse.ide.project;
import gse.ide.search;
import gse.ide.graph;
import gse.ide.docs;
import gse.ide.viewport;
import gse.ide.profile;

import :chrome;
import :code_panel;
import :layout;
import :project_screen;

export namespace gse::ide {
	namespace editor_app {
		struct [[= system_state<"Editor">{}]] data {
			bool initialized = false;
			bool screen_pushed = false;
			float explorer_ratio = 0.22f;
			float terminal_ratio = 0.22f;
			float agent_ratio = 0.72f;
			bool resizing_explorer = false;
			bool resizing_terminal = false;
			bool resizing_agent = false;
			cursor_shape frame_cursor = cursor_shape::arrow;
			clock save_clock;
		};

		[[= system_run<>{}]]
		auto run(
			context& ctx,
			data& d,
			channel_read<window_open_file_result, toggle_project_switcher_request, toggle_settings_request> requests_in,
			channel_write<gui::push_screen_request, settings::change_request, gui::popout_toggle, set_cursor_shape_request, jump_to_request, window_launcher_mode_request, window_open_file_request, build_runner::build_request, toggle_project_switcher_request, toggle_settings_request> ui_out,
			shared_view<search_system::data> search_d,
			shared_view<input::data> input_d,
			const save::registry& save_reg
		) -> async::task<>;

		[[= system_shutdown{}]]
		auto shutdown(
			data& d
		) -> void;
	}

	namespace workspace_system {
		struct [[= system_state<"Workspace">{}]] data {
			workspace::data ws;
			graph_data graph;
			std::uint32_t graph_game_gen = 0;
			std::uint32_t graph_pending_gen = 0;
			std::uint32_t graph_load_attempts = 0;
			clock graph_load_clock;
			quick_search_state search;
			git::status_snapshot git_status;
			std::vector<std::filesystem::path> git_rootless;
			bool initialized = false;
			bool cursor_capture_sent = false;
			bool profile_capture_sent = false;
			bool game_was_running = false;
			profile_source profile_source_sent = profile_source::editor;
			bool game_input_forwarding = false;
			clock save_clock;
		};

		[[= system_run<>{}]]
		auto run(
			context& ctx,
			data& d,
			channel_read<git::status_updated, jump_to_request, gui::context_menu_result, analysis::diagnostics_completed, build_runner::build_finished> requests_in,
			channel_write<gui::menu_content, cursor_capture_request, profile_capture_request, profile_report_request, build_runner::attached_input, build_runner::build_request, git_system::init_request, jump_to_request, toggle_project_switcher_request, toggle_settings_request, analysis::diagnostics_request, git_system::refresh_request, set_cursor_shape_request, search::index_merge_request> ui_out,
			const scheduler& sched,
			shared_view<config_system::data> config_d,
			shared_view<search_system::data> search_d,
			shared_view<input::data> input_d,
			shared_view<viewport::data> viewport_d,
			shared_view<build_runner::data> build_d,
			shared_view<window::data> window_d,
			shared_view<profile_system::data> profile_d
		) -> async::task<>;

		[[= system_shutdown{}]]
		auto shutdown(
			data& d
		) -> void;
	}
}

namespace gse::ide {
	constexpr std::string_view explorer_panel_name = "Explorer";
	constexpr std::string_view code_panel_name = "Code";
	constexpr time editor_layout_save_interval = seconds(30.f);
	constexpr time system_graph_retry_interval = milliseconds(100.f);
	constexpr std::uint32_t system_graph_max_attempts = 100;

	auto load_editor_layout(
		editor_app::data& d
	) -> void;

	auto save_editor_layout(
		const editor_app::data& d
	) -> void;

	auto forward_game_input(
		const input::state& input,
		channel_write<build_runner::attached_input> channels,
		vec2f game_cursor,
		bool release_held
	) -> void;
}

auto gse::ide::load_editor_layout(editor_app::data& d) -> void {
	const std::vector<layout_store::section> sections = layout_store::parse_sections(layout_store::read(editor_layout_path()));
	for (const layout_store::section& section : sections) {
		if (section.name != "editor") {
			continue;
		}
		if (const auto it = section.values.find("explorer_ratio"); it != section.values.end()) {
			d.explorer_ratio = std::clamp(parse_layout_float(it->second, d.explorer_ratio), 0.05f, 0.95f);
		}
		if (const auto it = section.values.find("agent_ratio"); it != section.values.end()) {
			d.agent_ratio = std::clamp(parse_layout_float(it->second, d.agent_ratio), 0.05f, 0.95f);
		}
		if (const auto it = section.values.find("terminal_ratio"); it != section.values.end()) {
			d.terminal_ratio = std::clamp(parse_layout_float(it->second, d.terminal_ratio), 0.05f, 0.95f);
		}
		return;
	}
}

auto gse::ide::save_editor_layout(const editor_app::data& d) -> void {
	std::string out;
	out.append("[editor]\n");
	out.append(std::format("explorer_ratio = {}\n", d.explorer_ratio));
	out.append(std::format("terminal_ratio = {}\n", d.terminal_ratio));
	out.append(std::format("agent_ratio = {}\n", d.agent_ratio));
	replace_layout_sections(editor_layout_owner(), out);
}

auto gse::ide::forward_game_input(const input::state& input, const channel_write<build_runner::attached_input> channels, const vec2f game_cursor, const bool release_held) -> void {
	const auto game_x = static_cast<double>(game_cursor.x());
	const auto game_y = static_cast<double>(game_cursor.y());

	if (release_held) {
		for (const key held : input.keys_held()) {
			if (held != key::escape) {
				channels.push<build_runner::attached_input>({ .event = gse::input::key_released{ .key_code = held } });
			}
		}
		for (const mouse_button held : input.mouse_buttons_held()) {
			channels.push<build_runner::attached_input>({ .event = gse::input::mouse_button_released{ .button = held, .x_pos = game_x, .y_pos = game_y } });
		}
		return;
	}

	channels.push<build_runner::attached_input>({ .event = gse::input::mouse_moved{ .x_pos = game_x, .y_pos = game_y } });
	for (const key pressed : input.keys_pressed()) {
		channels.push<build_runner::attached_input>({ .event = gse::input::key_pressed{ .key_code = pressed } });
	}
	for (const key released : input.keys_released()) {
		channels.push<build_runner::attached_input>({ .event = gse::input::key_released{ .key_code = released } });
	}
	for (const mouse_button pressed : input.mouse_buttons_pressed()) {
		channels.push<build_runner::attached_input>({ .event = gse::input::mouse_button_pressed{ .button = pressed, .x_pos = game_x, .y_pos = game_y } });
	}
	for (const mouse_button released : input.mouse_buttons_released()) {
		channels.push<build_runner::attached_input>({ .event = gse::input::mouse_button_released{ .button = released, .x_pos = game_x, .y_pos = game_y } });
	}
	if (const vec2f scroll = input.scroll_delta(); scroll.x() != 0.f || scroll.y() != 0.f) {
		channels.push<build_runner::attached_input>({ .event = gse::input::mouse_scrolled{ .x_offset = scroll.x(), .y_offset = scroll.y() } });
	}

	const std::string_view typed = input.text_entered();
	for (std::size_t i = 0; i < typed.size();) {
		const auto lead = static_cast<unsigned char>(typed[i]);
		const std::size_t length = lead < 0x80 ? 1 : (lead < 0xE0 ? 2 : (lead < 0xF0 ? 3 : 4));
		if (i + length > typed.size()) {
			break;
		}
		auto codepoint = static_cast<std::uint32_t>(length == 1 ? lead : (lead & (0xFF >> (length + 1))));
		for (std::size_t byte = 1; byte < length; ++byte) {
			codepoint = codepoint << 6 | static_cast<unsigned char>(typed[i + byte]) & 0x3F;
		}
		channels.push<build_runner::attached_input>({ .event = gse::input::text_entered{ .codepoint = codepoint } });
		i += length;
	}
}

auto gse::ide::editor_app::run(context& ctx, data& d, const channel_read<window_open_file_result, toggle_project_switcher_request, toggle_settings_request> requests_in, const channel_write<gui::push_screen_request, settings::change_request, gui::popout_toggle, set_cursor_shape_request, jump_to_request, window_launcher_mode_request, window_open_file_request, build_runner::build_request, toggle_project_switcher_request, toggle_settings_request> ui_out, const shared_view<search_system::data> search_d, const shared_view<input::data> input_d, const save::registry& save_reg) -> async::task<> {
	if (!d.initialized) {
		load_editor_layout(d);
		d.save_clock.reset();
		d.initialized = true;
	}

	if (!d.screen_pushed && search_d.index) {
		ui_out.push<gui::push_screen_request>({
			.factory = [channels = channel_write<build_runner::build_request, jump_to_request, toggle_project_switcher_request, toggle_settings_request>(ui_out), index = search_d.index, input_d] {
				return std::make_unique<editor_screen>(channels, index, input_d);
			},
		});
		const project::manifest& active = project::current();
		if (!active.valid || !active.requested || !active.engine_problem.empty()) {
			ui_out.push<gui::push_screen_request>({
				.factory = [channels = channel_write<window_launcher_mode_request, window_open_file_request>(ui_out), input_d] {
					return std::make_unique<project_screen>(channels, input_d, true);
				},
			});
		}
		d.screen_pushed = true;
	}
	for (const auto& res : requests_in.of<window_open_file_result>()) {
		if (res.path.empty() || res.path.extension() != ".gseproj") {
			continue;
		}
		app::relaunch_on_exit(gse::config::executable_file(), res.path.parent_path(), { res.path });
		gse::shutdown();
	}

	for ([[maybe_unused]] const auto& req : requests_in.of<toggle_project_switcher_request>()) {
		ui_out.push<gui::push_screen_request>({
			.factory = [channels = channel_write<window_launcher_mode_request, window_open_file_request>(ui_out), input_d] {
				return std::make_unique<project_screen>(channels, input_d);
			},
		});
	}
	for ([[maybe_unused]] const auto& req : requests_in.of<toggle_settings_request>()) {
		ui_out.push<gui::push_screen_request>({
			.factory = [save = &save_reg, channels = settings::panel_writer(ui_out)] {
				return std::make_unique<gui::settings_screen>(*save, channels, gui::settings_screen_config{ .opaque = true });
			},
		});
	}

	const input::state& input = input::current_state(input_d);
	const vec2f mouse = input.mouse_position();
	const bool pressed = input.mouse_button_pressed(mouse_button::button_1);
	const bool held = input.mouse_button_held(mouse_button::button_1);

	ui_out.push<settings::change_request>({
		.state_type = id_of<gui::data>(),
		.apply = [&d, mouse, pressed, held](void* p) {
			gui::data& s = *static_cast<gui::data*>(p);
			gui::menu* explorer = editor_menu(s, explorer_panel_name);
			gui::menu* code = editor_menu(s, code_panel_name);
			gui::menu* term = editor_menu(s, terminal::panel_name);
			gui::menu* agent_panel = editor_menu(s, agent::panel_name);
			if (!explorer || !code || !term || !agent_panel || explorer == code || explorer == term || code == term) {
				return;
			}
			if (agent_panel == explorer || agent_panel == code || agent_panel == term) {
				return;
			}

			const vec2f vp = s.previous_viewport_size;
			if (vp.x() <= 0.f || vp.y() <= 0.f) {
				return;
			}

			const gui::style sty = gui::apply_scale(s, gui::style::from_theme(s.current_theme), vp.y());
			const float inset = s.reserve_top_bar ? sty.title_bar_height : 0.f;
			const float top = vp.y() - inset;
			if (top <= 0.f) {
				return;
			}

			const float min_explorer_width = 180.f * sty.scale_factor;
			const float min_agent_width = 260.f * sty.scale_factor;
			const float min_code_width = 320.f * sty.scale_factor + min_agent_width;
			const float min_terminal_height = 120.f * sty.scale_factor;
			const float min_main_height = 180.f * sty.scale_factor;
			if (vp.x() < min_explorer_width + min_code_width || top < min_main_height + min_terminal_height) {
				return;
			}
			const float hit_width = std::max(6.f * sty.scale_factor, sty.resize_border_thickness);
			const bool blocked = s.menu_stack.captures_input() || s.context_menu.open || s.input_layers_data.is_resize_blocked(mouse);

			const float divider_thickness = hit_width * 2.f;
			const rectf frame = rectf::from_position_size({ 0.f, top }, { vp.x(), top });

			const gui::layout::split_params vertical_split{
				.container = frame,
				.axis = gui::layout::split_axis::rows,
				.ratio = 1.f - d.terminal_ratio,
				.min_first = min_main_height,
				.min_second = min_terminal_height,
				.divider_thickness = divider_thickness,
			};
			const rectf main_area = gui::layout::resolve_split(vertical_split).first;

			const gui::layout::split_params horizontal_split{
				.container = main_area,
				.axis = gui::layout::split_axis::columns,
				.ratio = d.explorer_ratio,
				.min_first = min_explorer_width,
				.min_second = min_code_width,
				.divider_thickness = divider_thickness,
			};

			const gui::layout::split_result columns = gui::layout::update_split(
				horizontal_split,
				{
					.mouse = mouse,
					.pressed = pressed,
					.held = held,
					.blocked = blocked,
				},
				d.resizing_explorer
			);
			d.explorer_ratio = columns.ratio;

			const gui::layout::split_result rows = gui::layout::update_split(
				vertical_split,
				{
					.mouse = mouse,
					.pressed = pressed,
					.held = held,
					.blocked = blocked || d.resizing_explorer,
				},
				d.resizing_terminal
			);
			d.terminal_ratio = 1.f - rows.ratio;

			const gui::layout::split_result final_rows = gui::layout::resolve_split({
				.container = frame,
				.axis = gui::layout::split_axis::rows,
				.ratio = 1.f - d.terminal_ratio,
				.min_first = min_main_height,
				.min_second = min_terminal_height,
				.divider_thickness = divider_thickness,
			});
			const gui::layout::split_result final_columns = gui::layout::resolve_split({
				.container = final_rows.first,
				.axis = gui::layout::split_axis::columns,
				.ratio = d.explorer_ratio,
				.min_first = min_explorer_width,
				.min_second = min_code_width,
				.divider_thickness = divider_thickness,
			});
			d.terminal_ratio = 1.f - final_rows.ratio;
			d.explorer_ratio = final_columns.ratio;

			const gui::layout::split_params agent_split{
				.container = final_columns.second,
				.axis = gui::layout::split_axis::columns,
				.ratio = d.agent_ratio,
				.min_first = min_code_width - min_agent_width,
				.min_second = min_agent_width,
				.divider_thickness = divider_thickness,
			};

			const gui::layout::split_result agent_columns = gui::layout::update_split(
				agent_split,
				{
					.mouse = mouse,
					.pressed = pressed,
					.held = held,
					.blocked = blocked || d.resizing_explorer || d.resizing_terminal,
				},
				d.resizing_agent
			);
			d.agent_ratio = agent_columns.ratio;

			const gui::layout::split_result final_agent = gui::layout::resolve_split({
				.container = final_columns.second,
				.axis = gui::layout::split_axis::columns,
				.ratio = d.agent_ratio,
				.min_first = min_code_width - min_agent_width,
				.min_second = min_agent_width,
				.divider_thickness = divider_thickness,
			});
			d.agent_ratio = final_agent.ratio;

			cursor_shape want_cursor = cursor_shape::arrow;
			if (!blocked) {
				if (final_columns.divider.contains(mouse) || d.resizing_explorer || final_agent.divider.contains(mouse) || d.resizing_agent) {
					want_cursor = cursor_shape::resize_ew;
				}
				else if (final_rows.divider.contains(mouse) || d.resizing_terminal) {
					want_cursor = cursor_shape::resize_ns;
				}
			}
			d.frame_cursor = want_cursor;

			explorer->rect = final_columns.first;
			explorer->swap_parent(id());
			explorer->docked_to = gui::dock::location::none;
			explorer->fixed = true;
			explorer->bare = true;

			code->rect = final_agent.first;
			code->swap_parent(id());
			code->docked_to = gui::dock::location::none;
			code->fixed = true;
			code->bare = true;

			agent_panel->rect = final_agent.second;
			agent_panel->swap_parent(id());
			agent_panel->docked_to = gui::dock::location::none;
			agent_panel->fixed = true;
			agent_panel->bare = true;

			term->rect = final_rows.second;
			term->swap_parent(id());
			term->docked_to = gui::dock::location::none;
			term->fixed = true;
			term->bare = true;

			gui::clear_menu_interaction(s);
		},
	});

	if (d.frame_cursor != cursor_shape::arrow) {
		ui_out.push<set_cursor_shape_request>({ .shape = d.frame_cursor });
	}

	if (d.save_clock.elapsed() > editor_layout_save_interval) {
		save_editor_layout(d);
		d.save_clock.reset();
	}

	return {};
}

auto gse::ide::workspace_system::run(context& ctx, data& d, const channel_read<git::status_updated, jump_to_request, gui::context_menu_result, analysis::diagnostics_completed, build_runner::build_finished> requests_in, const channel_write<gui::menu_content, cursor_capture_request, profile_capture_request, profile_report_request, build_runner::attached_input, build_runner::build_request, git_system::init_request, jump_to_request, toggle_project_switcher_request, toggle_settings_request, analysis::diagnostics_request, git_system::refresh_request, set_cursor_shape_request, search::index_merge_request> ui_out, const scheduler& sched, const shared_view<config_system::data> config_d, const shared_view<search_system::data> search_d, const shared_view<input::data> input_d, const shared_view<viewport::data> viewport_d, const shared_view<build_runner::data> build_d, const shared_view<window::data> window_d, const shared_view<profile_system::data> profile_d) -> async::task<> {
	if (!d.initialized) {
		d.ws.fs_root.is_dir = true;
		d.ws.fs_root.children.clear();
		for (const config::browse_root& browse : config::browse_roots()) {
			const gui::symbol_glyph glyph = browse.is_project ? gui::symbol::project : (browse.analyzable ? nullptr : gui::symbol::gear);
			d.ws.fs_root.children.push_back(workspace::make_root(browse.path, browse.name, glyph));
		}
		d.ws.fs_root.loaded = true;
		project::record_recent();
		load_workspace_layout(d.ws);
		if (!d.ws.cppref.loaded) {
			d.ws.cppref.load(config::cppref_index());
		}
		d.save_clock.reset();
		d.initialized = true;
	}

	const std::uint32_t game_gen = build_d.game_generation;
	if (d.graph_game_gen != game_gen) {
		const std::filesystem::path game_graph_file = build_d.game_graph_path;
		if (!game_graph_file.empty()) {
			if (d.graph_pending_gen != game_gen) {
				d.graph_pending_gen = game_gen;
				d.graph_load_attempts = 0;
				d.graph_load_clock.reset();
			}
			const bool load_due = d.graph_load_attempts == 0 || d.graph_load_clock.elapsed() >= system_graph_retry_interval;
			if (load_due) {
				++d.graph_load_attempts;
				d.graph_load_clock.reset();
				std::expected<graph_data, graph_load_error> loaded = build_graph_from_file(game_graph_file);
				if (loaded) {
					d.graph = std::move(*loaded);
					d.graph_game_gen = game_gen;
					d.graph_load_attempts = 0;
				}
				else if (loaded.error() == graph_load_error::incompatible || d.graph_load_attempts >= system_graph_max_attempts) {
					log::println(log::level::warning, log::category::general, "failed to load system graph '{}': {}", game_graph_file, loaded.error());
					d.graph_game_gen = game_gen;
					d.graph_load_attempts = 0;
				}
			}
		}
	}
	if (!d.graph.built && sched.all_settled()) {
		d.graph = build_graph(sched);
	}

	workspace::data* ws = &d.ws;
	const auto config = config_d;
	const input::state& input = input::current_state(input_d);
	const input::state input_snapshot = input;
	const bool game_running = build_d.session && build_d.session->status == build_runner::attached_session_status::active;
	if (!workspace::game_active(d.ws) || !game_running || d.ws.game_view != game_view_kind::game || !window_d.focused) {
		d.ws.game_captured = false;
	}
	if (d.ws.game_captured) {
		if (d.ws.game_capture_settle_frames > 0) {
			--d.ws.game_capture_settle_frames;
		}
		else {
			const vec2f delta = input.mouse_delta();
			d.ws.game_cursor += vec2f{ delta.x() * d.ws.game_input_scale.x(), -delta.y() * d.ws.game_input_scale.y() };
		}
		const bool release_chord = input.key_held(key::left_shift) || input.key_held(key::right_shift);
		const bool releasing = release_chord && input.key_pressed(key::escape);
		forward_game_input(input, ui_out, d.ws.game_cursor, releasing);
		if (releasing) {
			d.ws.game_captured = false;
		}
	}
	else if (d.game_input_forwarding) {
		forward_game_input(input, ui_out, d.ws.game_cursor, true);
	}
	d.game_input_forwarding = d.ws.game_captured;
	workspace::update_explorer(d.ws);
	for (const git::status_updated& update : requests_in.of<git::status_updated>()) {
		d.git_status = update.status;
		d.git_rootless = update.rootless;
	}
	update_diagnostics(ctx, requests_in, ui_out, d.ws, config_d, build_d.building);
	d.ws.watcher.poll();
	if (input.mouse_button_pressed(mouse_button::button_4)) {
		workspace::go_back(d.ws);
	}
	if (input.mouse_button_pressed(mouse_button::button_5)) {
		workspace::go_forward(d.ws);
	}

	ui_out.push<gui::menu_content>({
		.menu = std::string(explorer_panel_name),
		.layer = render_layer::content,
		.build = [ws, search = &d.search, index = search_d.index, channels = ui_out, input_snapshot, git_status = d.git_status, git_rootless = &d.git_rootless](gui::builder& b) {
			draw_explorer_panel(b, input_snapshot, *ws, *search, index, channels, git_status.get(), *git_rootless);
		},
	});

	const gpu::bindless_slot viewport_slot = viewport_d.ready ? viewport_d.display_slot : gpu::bindless_slot{};
	const bool building = build_d.building;
	const vec2u game_extent = viewport_d.extent;

	if (d.cursor_capture_sent != d.ws.game_captured) {
		d.cursor_capture_sent = d.ws.game_captured;
		ui_out.push<cursor_capture_request>({ .capture = d.ws.game_captured });
	}

	const bool profile_enabled = d.ws.profile.enabled;
	if (d.profile_capture_sent != profile_enabled || d.profile_source_sent != d.ws.profile.source) {
		d.profile_capture_sent = profile_enabled;
		d.profile_source_sent = d.ws.profile.source;
		ui_out.push<profile_capture_request>({
			.enabled = profile_enabled,
			.source = d.ws.profile.source,
		});
	}

	if (d.game_was_running && !game_running && d.ws.profile.source == profile_source::game) {
		const std::filesystem::path game_exe = config::game_executable();
		const std::filesystem::path run = gse::profile::latest_run_dir(game_exe.stem().native_encoded_string());
		ui_out.push<profile_report_request>({
			.path = run.empty() ? run : run / gse::profile::report_name,
		});
	}
	d.game_was_running = game_running;

	ui_out.push<gui::menu_content>({
		.menu = std::string(code_panel_name),
		.layer = render_layer::content,
		.build = [ws, graph = &d.graph, index = search_d.index, config, channels = ui_out, input_snapshot, viewport_slot, game_running, building, game_extent, profile_d](gui::builder& b) {
			draw_code_panel(b, input_snapshot, *ws, *graph, channels, {
				.config = config,
				.profile = profile_d,
				.index = index,
				.viewport_slot = viewport_slot,
				.game_extent = game_extent,
				.game_running = game_running,
				.building = building,
			});
		},
	});

	for (const auto& req : requests_in.of<jump_to_request>()) {
		workspace::jump_to(d.ws, req.path, req.line, req.column);
	}

	for (const auto& res : requests_in.of<gui::context_menu_result>()) {
		if (res.tag == explorer_context_tag()) {
			const std::uint64_t* target = std::get_if<std::uint64_t>(&res.target);
			fs_node* node = target ? workspace::find_node(d.ws.fs_root, *target) : nullptr;
			const auto& table = explorer_actions();
			if (node && res.action_id < table.size() && table[res.action_id].run) {
				table[res.action_id].run(d.ws, *node);
			}
		}
		else if (res.tag == tab_context_tag()) {
			const auto& table = tab_actions();
			const gse::id* target = std::get_if<gse::id>(&res.target);
			if (target && res.action_id < table.size() && table[res.action_id].run) {
				table[res.action_id].run(d.ws, *target);
			}
		}
		else if (res.tag == editor_text_context_tag()) {
			const std::optional<gse::id> active_document_id = workspace::active_document_id(d.ws);
			if (const auto doc = active_document_id ? d.ws.documents.find(*active_document_id) : d.ws.documents.end(); doc != d.ws.documents.end()) {
				doc->second.view.pending_action = static_cast<gui::text_edit_action>(res.action_id);
			}
		}
	}

	if (d.save_clock.elapsed() > editor_layout_save_interval) {
		save_workspace_layout(d.ws);
		d.save_clock.reset();
	}

	return {};
}

auto gse::ide::editor_app::shutdown(data& d) -> void {
	save_editor_layout(d);
}

auto gse::ide::workspace_system::shutdown(data& d) -> void {
	workspace::save_dirty_documents(d.ws);
	save_workspace_layout(d.ws);
}

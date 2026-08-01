export module gse.ide.app:editor_app;

import std;
import gse;
import gse.gpu;

import gse.ide.workspace;
import gse.ide.git;
import gse.ide.terminal;
import gse.ide.build;
import gse.ide.config;
import gse.ide.project;
import gse.ide.search;
import gse.ide.graph;
import gse.ide.docs;
import gse.ide.viewport;

import :chrome;
import :code_panel;
import :layout;
import :project_screen;

namespace gse::ide {
	constexpr std::string_view explorer_panel_name = "Explorer";
	constexpr std::string_view code_panel_name = "Code";
	constexpr time editor_layout_save_interval = seconds(30.f);
}

export namespace gse::ide::editor_app {
	struct [[= system_state<"Editor">{}]] data {
		bool initialized = false;
		bool screen_pushed = false;
		float explorer_ratio = 0.22f;
		float terminal_ratio = 0.22f;
		bool resizing_explorer = false;
		bool resizing_terminal = false;
		cursor_shape frame_cursor = cursor_shape::arrow;
		clock save_clock;
	};

	[[= system_run<>{}]]
	auto run(
		context& ctx,
		data& d,
		shared_view<search_system::data> search_d,
		shared_view<input::data> input_d,
		const save::registry& save_reg
	) -> async::task<>;

	[[= system_shutdown{}]]
	auto shutdown(
		data& d
	) -> void;
}

export namespace gse::ide::workspace_system {
	struct [[= system_state<"Workspace">{}]] data {
		workspace::data ws;
		graph_data graph;
		bool graph_from_game = false;
		std::uint32_t graph_game_gen = 0;
		quick_search_state search;
		git::status_snapshot git_status;
		std::vector<std::filesystem::path> git_rootless;
		bool initialized = false;
		clock save_clock;
	};

	[[= system_run<>{}]]
	auto run(
		context& ctx,
		data& d,
		const scheduler& sched,
		shared_view<config_system::data> config_d,
		shared_view<search_system::data> search_d,
		shared_view<input::data> input_d,
		shared_view<viewport::data> viewport_d,
		shared_view<build_runner::data> build_d
	) -> async::task<>;

	[[= system_shutdown{}]]
	auto shutdown(
		data& d
	) -> void;
}

namespace gse::ide {
	auto load_editor_layout(
		editor_app::data& d
	) -> void;

	auto save_editor_layout(
		const editor_app::data& d
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
	replace_layout_sections(editor_layout_owner(), out);
}

auto gse::ide::editor_app::run(
	context& ctx,
	data& d,
	const shared_view<search_system::data> search_d,
	const shared_view<input::data> input_d,
	const save::registry& save_reg
) -> async::task<> {
	if (!d.initialized) {
		load_editor_layout(d);
		d.save_clock.reset();
		d.initialized = true;
	}

	if (!d.screen_pushed && search_d.index) {
		ctx.channels.push<gui::push_screen_request>({
			.factory = [channels = ctx.channels, index = search_d.index] {
				return std::make_unique<editor_screen>(channels, index);
			},
		});
		const project::manifest& active = project::current();
		if (!active.valid || !active.requested || !active.engine_problem.empty()) {
			ctx.channels.push<gui::push_screen_request>({
				.factory = [channels = ctx.channels] {
					return std::make_unique<project_screen>(channels, true);
				},
			});
		}
		d.screen_pushed = true;
	}
	for (const auto& res : ctx.read_channel<window_open_file_result>()) {
		if (res.path.empty() || res.path.extension() != ".gseproj") {
			continue;
		}
		gse::app::relaunch_on_exit(gse::config::executable_file(), res.path.parent_path(), { res.path });
		gse::shutdown();
	}

	for ([[maybe_unused]] const auto& req : ctx.read_channel<toggle_project_switcher_request>()) {
		ctx.channels.push<gui::push_screen_request>({
			.factory = [channels = ctx.channels] {
				return std::make_unique<project_screen>(channels);
			},
		});
	}
	for ([[maybe_unused]] const auto& req : ctx.read_channel<toggle_settings_request>()) {
		ctx.channels.push<gui::push_screen_request>({
			.factory = [save = &save_reg, channels = ctx.channels] {
				return std::make_unique<gui::settings_screen>(*save, channels, gui::settings_screen_config{ .opaque = true });
			},
		});
	}

	const input::state& input = input::current_state(input_d);
	const vec2f mouse = input.mouse_position();
	const bool pressed = input.mouse_button_pressed(mouse_button::button_1);
	const bool held = input.mouse_button_held(mouse_button::button_1);

	ctx.channels.push<settings::annotated_change_request<gui::data>>({
		.apply = [&d, mouse, pressed, held](gui::data& s) {
			gui::menu* explorer = editor_menu(s, explorer_panel_name);
			gui::menu* code = editor_menu(s, code_panel_name);
			gui::menu* term = editor_menu(s, terminal::panel_name);
			if (!explorer || !code || !term || explorer == code || explorer == term || code == term) {
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
			const float min_code_width = 320.f * sty.scale_factor;
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
				{ .mouse = mouse, .pressed = pressed, .held = held, .blocked = blocked },
				d.resizing_explorer
			);
			d.explorer_ratio = columns.ratio;

			const gui::layout::split_result rows = gui::layout::update_split(
				vertical_split,
				{ .mouse = mouse, .pressed = pressed, .held = held, .blocked = blocked || d.resizing_explorer },
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

			cursor_shape want_cursor = cursor_shape::arrow;
			if (!blocked) {
				if (final_columns.divider.contains(mouse) || d.resizing_explorer) {
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

			code->rect = final_columns.second;
			code->swap_parent(id());
			code->docked_to = gui::dock::location::none;
			code->fixed = true;
			code->bare = true;

			term->rect = final_rows.second;
			term->swap_parent(id());
			term->docked_to = gui::dock::location::none;
			term->fixed = true;
			term->bare = true;

			gui::clear_menu_interaction(s);
		},
	});

	if (d.frame_cursor != cursor_shape::arrow) {
		ctx.channels.push<set_cursor_shape_request>({ .shape = d.frame_cursor });
	}

	if (d.save_clock.elapsed() > editor_layout_save_interval) {
		save_editor_layout(d);
		d.save_clock.reset();
	}

	return {};
}

auto gse::ide::workspace_system::run(
	context& ctx,
	data& d,
	const scheduler& sched,
	const shared_view<config_system::data> config_d,
	const shared_view<search_system::data> search_d,
	const shared_view<input::data> input_d,
	const shared_view<viewport::data> viewport_d,
	const shared_view<build_runner::data> build_d
) -> async::task<> {
	if (!d.initialized) {
		d.ws.root = config::project_root();
		d.ws.fs_root.is_dir = true;
		d.ws.fs_root.children.clear();
		for (const config::browse_root& browse : config::browse_roots()) {
			d.ws.fs_root.children.push_back(workspace::make_root(browse.path, browse.name, browse.is_project ? gui::symbol::project : nullptr));
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
			graph_data loaded = build_graph_from_file(game_graph_file);
			if (loaded.built) {
				d.graph = std::move(loaded);
				d.graph_from_game = true;
				d.graph_game_gen = game_gen;
			}
		}
	}
	if (!d.graph.built && sched.all_settled()) {
		d.graph = build_graph(sched);
	}

	workspace::data* ws = &d.ws;
	const auto config = config_d;
	const input::state& input = input::current_state(input_d);
	for (const git::status_updated& update : ctx.read_channel<git::status_updated>()) {
		d.git_status = update.status;
		d.git_rootless = update.rootless;
	}
	update_diagnostics(ctx, d.ws, config_d, build_d.building);
	d.ws.watcher.poll();
	if (input.mouse_button_pressed(mouse_button::button_4)) {
		workspace::go_back(d.ws);
	}
	if (input.mouse_button_pressed(mouse_button::button_5)) {
		workspace::go_forward(d.ws);
	}

	ctx.channels.push<gui::menu_content>({
		.menu = std::string(explorer_panel_name),
		.layer = render_layer::content,
		.build = [ws, search = &d.search, index = search_d.index, channels = ctx.channels, git_status = d.git_status, git_rootless = &d.git_rootless](gui::builder& b) {
			draw_explorer_panel(b, *ws, *search, index, channels, git_status.get(), *git_rootless);
		},
	});

	const gpu::bindless_slot viewport_slot = viewport_d.ready ? viewport_d.display_slot : gpu::bindless_slot{};
	const bool game_running = viewport_d.imported_ready;
	const bool building = build_d.building;

	ctx.channels.push<gui::menu_content>({
		.menu = std::string(code_panel_name),
		.layer = render_layer::content,
		.build = [ws, graph = &d.graph, index = search_d.index, config, channels = ctx.channels, viewport_slot, game_running, building](gui::builder& b) {
			draw_code_panel(b, *ws, *graph, index, config, channels, viewport_slot, game_running, building);
		},
	});

	for (const auto& req : ctx.read_channel<jump_to_request>()) {
		workspace::jump_to(d.ws, req.path, req.line, req.column);
	}

	for (const auto& res : ctx.read_channel<gui::context_menu_result>()) {
		if (res.tag == explorer_context_tag()) {
			const fs_node* node = workspace::find_node(d.ws.fs_root, res.target);
			const auto& table = explorer_actions();
			if (node && res.action_id < table.size() && table[res.action_id].run) {
				table[res.action_id].run(d.ws, *node);
			}
		}
		else if (res.tag == tab_context_tag()) {
			const auto& table = tab_actions();
			if (res.action_id < table.size() && table[res.action_id].run) {
				table[res.action_id].run(d.ws, static_cast<std::uint32_t>(res.target));
			}
		}
		else if (res.tag == editor_text_context_tag()) {
			if (const auto doc = d.ws.documents.find(d.ws.active_document_id); doc != d.ws.documents.end()) {
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
	save_workspace_layout(d.ws);
}
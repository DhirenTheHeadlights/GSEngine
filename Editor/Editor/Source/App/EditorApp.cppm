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
import gse.ide.alloc;
import gse.ide.problems;
import gse.ide.search_panel;
import gse.ide.docs;
import gse.ide.viewport;
import gse.ide.profile;

import :chrome;
import :code_panel;
import :dock;
import :layout;
import :project_screen;

export namespace gse::ide {
	namespace editor_app {
		struct [[= system_state<"Editor">{}]] data {
			bool initialized = false;
			bool screen_pushed = false;
			dock_tree tree;
			std::optional<dock_drag> drag;
			std::optional<dock_drop> drop;
			rectf frame;
			dock_layout layout;
			std::optional<vec2f> pending_panels_menu;
			cursor_shape frame_cursor = cursor_shape::arrow;
			bool layout_dirty = false;
			clock save_clock;
		};

		[[= system_run<>{}]]
		auto run(
			context& ctx,
			data& d,
			channel_read<window_open_file_result, toggle_project_switcher_request, toggle_settings_request, open_panels_menu_request, gui::context_menu_result> requests_in,
			channel_write<gui::push_screen_request, settings::change_request, gui::popout_toggle, set_cursor_shape_request, jump_to_request, window_launcher_mode_request, window_open_file_request, build_runner::build_request, toggle_project_switcher_request, toggle_settings_request, open_panels_menu_request> ui_out,
			shared_view<search_system::data> search_d,
			shared_view<input::data> input_d,
			shared_view<window::data> window_d,
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
			problems_view_state problems;
			search_panel_state search_panel;
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
	constexpr std::string_view graph_panel_name = "Graph";
	constexpr std::string_view profile_panel_name = "Profile";
	constexpr std::string_view alloc_panel_name = "Alloc";
	constexpr std::string_view problems_panel_name = "Problems";
	constexpr std::string_view search_panel_name = "Search";
	constexpr time editor_layout_save_interval = seconds(30.f);
	constexpr time system_graph_retry_interval = milliseconds(100.f);
	constexpr std::uint32_t system_graph_max_attempts = 100;

	constexpr std::uint32_t reset_layout_action = 0xFFFFFFFF;

	struct dock_input {
		vec2f mouse;
		bool pressed = false;
		bool held = false;
		bool blocked = false;
		bool context_pressed = false;
		bool toggle_maximize = false;
	};

	[[nodiscard]] auto panels_context_tag() -> gse::id;

	[[nodiscard]] auto panels_menu_items(
		const dock_tree& tree,
		std::span<const panel_desc> panels
	) -> std::vector<gui::menu_item>;

	auto toggle_panel(
		editor_app::data& d,
		gse::id panel
	) -> void;

	auto apply_pending_panel_close(
		gui::data& s,
		editor_app::data& d
	) -> void;

	[[nodiscard]] auto editor_panels() -> std::span<const panel_desc>;

	[[nodiscard]] auto editor_dock_metrics(
		const gui::style& sty
	) -> dock_metrics;

	[[nodiscard]] auto default_editor_tree() -> dock_tree;

	auto load_editor_layout(
		editor_app::data& d
	) -> void;

	auto save_editor_layout(
		const editor_app::data& d
	) -> void;

	auto sync_dock_menus(
		gui::data& s,
		editor_app::data& d
	) -> void;

	auto update_dock_interaction(
		gui::data& s,
		editor_app::data& d,
		const dock_input& in
	) -> void;

	auto forward_game_input(
		const input::state& input,
		channel_write<build_runner::attached_input> channels,
		vec2f game_cursor,
		bool release_held
	) -> void;
}

auto gse::ide::editor_panels() -> std::span<const panel_desc> {
	static const std::array table = {
		panel_desc{
			.id = find_or_generate_id(explorer_panel_name),
			.name = explorer_panel_name,
			.min_size = { 180.f, 160.f },
			.accent_edge = gui::panel_edge::right,
		},
		panel_desc{
			.id = find_or_generate_id(code_panel_name),
			.name = code_panel_name,
			.min_size = { 320.f, 200.f },
		},
		panel_desc{
			.id = find_or_generate_id(agent::panel_name),
			.name = agent::panel_name,
			.min_size = { 260.f, 160.f },
			.accent_edge = gui::panel_edge::left,
		},
		panel_desc{
			.id = find_or_generate_id(terminal::panel_name),
			.name = terminal::panel_name,
			.min_size = { 260.f, 120.f },
		},
		panel_desc{
			.id = find_or_generate_id(graph_panel_name),
			.name = graph_panel_name,
			.min_size = { 320.f, 200.f },
			.start_hidden = true,
		},
		panel_desc{
			.id = find_or_generate_id(profile_panel_name),
			.name = profile_panel_name,
			.min_size = { 360.f, 200.f },
			.start_hidden = true,
		},
		panel_desc{
			.id = find_or_generate_id(alloc_panel_name),
			.name = alloc_panel_name,
			.min_size = { 320.f, 200.f },
			.start_hidden = true,
		},
		panel_desc{
			.id = find_or_generate_id(problems_panel_name),
			.name = problems_panel_name,
			.min_size = { 320.f, 120.f },
			.start_hidden = true,
		},
		panel_desc{
			.id = find_or_generate_id(search_panel_name),
			.name = search_panel_name,
			.min_size = { 300.f, 160.f },
			.start_hidden = true,
		},
	};
	return table;
}

auto gse::ide::editor_dock_metrics(const gui::style& sty) -> dock_metrics {
	return {
		.scale = sty.scale_factor,
		.divider_thickness = 16.f,
	};
}

auto gse::ide::default_editor_tree() -> dock_tree {
	const gse::id code = find_or_generate_id(code_panel_name);
	dock_tree tree;
	insert_panel(tree, {
		.panel = code,
	});
	insert_panel(tree, {
		.panel = find_or_generate_id(explorer_panel_name),
		.target = find_leaf(tree, code),
		.location = gui::dock::location::left,
		.ratio = 0.22f,
	});
	insert_panel(tree, {
		.panel = find_or_generate_id(agent::panel_name),
		.target = find_leaf(tree, code),
		.location = gui::dock::location::right,
		.ratio = 0.28f,
	});
	insert_panel(tree, {
		.panel = find_or_generate_id(terminal::panel_name),
		.target = tree.root,
		.location = gui::dock::location::bottom,
		.ratio = 0.22f,
	});
	return tree;
}

auto gse::ide::load_editor_layout(editor_app::data& d) -> void {
	const std::vector<layout_store::section> sections = layout_store::parse_sections(layout_store::read(editor_layout_path()));
	std::optional<dock_tree> restored = deserialize_tree(sections, editor_panels());
	d.tree = restored ? std::move(*restored) : default_editor_tree();
}

auto gse::ide::save_editor_layout(const editor_app::data& d) -> void {
	replace_layout_sections(editor_layout_owner(), serialize_tree(d.tree, editor_panels()));
}

auto gse::ide::panels_context_tag() -> gse::id {
	return find_or_generate_id("panels_context");
}

auto gse::ide::panels_menu_items(const dock_tree& tree, const std::span<const panel_desc> panels) -> std::vector<gui::menu_item> {
	std::vector<gui::menu_item> items;
	items.reserve(panels.size() + 1);

	for (std::size_t i = 0; i < panels.size(); ++i) {
		if (contains_panel(tree, panels[i].id)) {
			items.push_back({
				.label = std::format("Hide {}", panels[i].name),
				.action_id = static_cast<std::uint32_t>(i),
				.enabled = panel_count(tree) > 1,
			});
		}
	}

	bool first_hidden = true;
	for (std::size_t i = 0; i < panels.size(); ++i) {
		if (!contains_panel(tree, panels[i].id)) {
			items.push_back({
				.label = std::format("Show {}", panels[i].name),
				.action_id = static_cast<std::uint32_t>(i),
				.separator_before = first_hidden,
			});
			first_hidden = false;
		}
	}

	items.push_back({
		.label = "Reset Layout",
		.action_id = reset_layout_action,
		.separator_before = true,
	});
	return items;
}

auto gse::ide::toggle_panel(editor_app::data& d, const gse::id panel) -> void {
	if (contains_panel(d.tree, panel)) {
		if (panel_count(d.tree) <= 1) {
			return;
		}
		remove_panel(d.tree, panel);
	}
	else {
		insert_panel(d.tree, {
			.panel = panel,
			.target = any_leaf(d.tree),
			.location = gui::dock::location::center,
		});
		activate_panel(d.tree, panel);
	}
	d.layout_dirty = true;
}

auto gse::ide::apply_pending_panel_close(gui::data& s, editor_app::data& d) -> void {
	if (!s.primary.pending_tab_close) {
		return;
	}

	const auto [host_id, tab_index] = *s.primary.pending_tab_close;
	const gui::menu* host = s.primary.menus.try_get(host_id);
	if (!host || tab_index >= host->tab_contents.size()) {
		return;
	}

	const std::optional<gse::id> panel = try_find(host->tab_contents[tab_index]);
	const std::span<const panel_desc> panels = editor_panels();
	if (!panel || std::ranges::find(panels, *panel, &panel_desc::id) == panels.end()) {
		return;
	}

	s.primary.pending_tab_close.reset();
	if (panel_count(d.tree) <= 1) {
		return;
	}

	remove_panel(d.tree, *panel);
	d.layout_dirty = true;
}

auto gse::ide::sync_dock_menus(gui::data& s, editor_app::data& d) -> void {
	const std::span<const panel_desc> panels = editor_panels();
	std::vector<gse::id> live;
	std::vector<gse::id> shown;
	live.reserve(d.layout.leaves.size());

	for (const dock_placement& leaf : d.layout.leaves) {
		dock_node* node = d.tree.nodes.try_get(leaf.node);
		gui::menu& host = editor_menu(s, node->panels.front().tag());

		if (host.tab_contents.size() == node->panels.size()) {
			std::vector<gse::id> reordered;
			reordered.reserve(node->panels.size());
			for (const std::string& tab : host.tab_contents) {
				const std::optional<gse::id> panel = try_find(tab);
				if (panel && std::ranges::find(node->panels, *panel) != node->panels.end()) {
					reordered.push_back(*panel);
				}
			}
			if (reordered.size() == node->panels.size()) {
				node->panels = std::move(reordered);
			}
			if (host.active_tab_index < node->panels.size()) {
				node->active_panel = host.active_tab_index;
			}
		}

		host.rect = leaf.rect;
		host.swap_parent(gse::id());
		host.docked_to = gui::dock::location::none;
		host.fixed = true;
		host.bare = node->panels.size() == 1;
		host.tab_contents.clear();
		host.tab_contents.reserve(node->panels.size());
		for (const gse::id panel : node->panels) {
			host.tab_contents.emplace_back(panel.tag());
			shown.push_back(panel);
		}
		host.active_tab_index = node->active_panel;
		host.tabs_closeable = true;
		host.accent_edge.reset();
		for (const gse::id panel : node->panels) {
			const auto desc = std::ranges::find(panels, panel, &panel_desc::id);
			if (desc != panels.end() && desc->accent_edge) {
				host.accent_edge = desc->accent_edge;
				break;
			}
		}
		live.push_back(host.id());
	}

	s.primary.suppressed_menus.clear();
	for (const panel_desc& desc : panels) {
		if (std::ranges::find(shown, desc.id) == shown.end()) {
			s.primary.suppressed_menus.insert(stable_id(desc.name));
		}
	}

	std::vector<gse::id> stale;
	for (const gui::menu& m : s.primary.menus.items()) {
		const std::string_view tag = m.id().tag();
		const bool owned = std::ranges::any_of(panels, [tag](const panel_desc& desc) {
			return desc.name == tag;
		});
		if (owned && std::ranges::find(live, m.id()) == live.end()) {
			stale.push_back(m.id());
		}
	}
	for (const gse::id menu_id : stale) {
		s.primary.menus.remove(menu_id);
	}
}

auto gse::ide::update_dock_interaction(gui::data& s, editor_app::data& d, const dock_input& in) -> void {
	const vec2f viewport_size = s.primary.previous_viewport_size;
	if (viewport_size.x() <= 0.f || viewport_size.y() <= 0.f) {
		return;
	}

	const gui::style sty = gui::apply_scale(s, s.primary, gui::style::from_theme(s.current_theme), viewport_size.y());
	const float inset = s.reserve_top_bar ? sty.title_bar_height : 0.f;
	const float top = viewport_size.y() - inset;
	if (top <= 0.f) {
		return;
	}

	const dock_metrics metrics = editor_dock_metrics(sty);
	const std::span<const panel_desc> panels = editor_panels();
	d.frame = rectf::from_position_size({ 0.f, top }, { viewport_size.x(), top });

	apply_pending_panel_close(s, d);
	d.layout = resolve(d.tree, d.frame, metrics, panels);

	if (in.toggle_maximize && !d.drag) {
		if (d.tree.maximized.exists()) {
			d.tree.maximized.reset();
		}
		else if (const auto hit = std::ranges::find_if(d.layout.leaves, [&in](const dock_placement& leaf) {
			return leaf.rect.contains(in.mouse);
		}); hit != d.layout.leaves.end()) {
			const dock_node* node = d.tree.nodes.try_get(hit->node);
			d.tree.maximized = node->panels[node->active_panel];
		}
		d.layout = resolve(d.tree, d.frame, metrics, panels);
		d.layout_dirty = true;
	}

	if (d.pending_panels_menu) {
		s.primary.context_menu = {
			.open = true,
			.just_opened = true,
			.position = *d.pending_panels_menu,
			.items = panels_menu_items(d.tree, panels),
			.tag = panels_context_tag(),
		};
		d.pending_panels_menu.reset();
	}

	if (d.drag) {
		if (in.held) {
			if (!d.drag->torn && !d.drag->header.contains(in.mouse) && distance(in.mouse, d.drag->start) > metrics.tear_threshold * metrics.scale) {
				d.drag->torn = true;
			}
			d.drop = d.drag->torn ? drop_target(d.tree, d.layout, metrics, d.drag->panel, in.mouse) : std::nullopt;
			if (d.drop) {
				s.primary.active_dock_space = d.drop->space;
			}
			else {
				s.primary.active_dock_space.reset();
			}
			if (d.drag->torn) {
				s.primary.active_drag_ghost = gui::drag_ghost{
					.label = std::string(d.drag->panel.tag()),
					.position = in.mouse,
				};
			}
			d.frame_cursor = d.drag->torn ? cursor_shape::hand : cursor_shape::arrow;
			return;
		}

		s.primary.active_dock_space.reset();
		s.primary.active_drag_ghost.reset();
		if (d.drag->torn && d.drop && d.drop->location != gui::dock::location::none) {
			insert_panel(d.tree, {
				.panel = d.drag->panel,
				.target = d.drop->node,
				.location = d.drop->location,
			});
			activate_panel(d.tree, d.drag->panel);
			d.layout = resolve(d.tree, d.frame, metrics, panels);
			d.layout_dirty = true;
		}
		d.drag.reset();
		d.drop.reset();
		d.frame_cursor = cursor_shape::arrow;
		return;
	}

	const std::optional<gui::layout::split_axis> held_axis = dragging_axis(d.tree);

	if (in.context_pressed && !in.blocked) {
		for (const dock_placement& leaf : d.layout.leaves) {
			const dock_node* node = d.tree.nodes.try_get(leaf.node);
			const gui::menu& host = editor_menu(s, node->panels.front().tag());
			const rectf header = rectf::from_position_size(leaf.rect.top_left(), { leaf.rect.width(), gui::menu_chrome_height(s.fonts, host, sty, leaf.rect.width()) });
			if (header.contains(in.mouse)) {
				d.pending_panels_menu = in.mouse;
				break;
			}
		}
	}

	if (in.pressed && !in.blocked && !held_axis) {
		for (const dock_placement& leaf : d.layout.leaves) {
			const dock_node* node = d.tree.nodes.try_get(leaf.node);
			const gui::menu& host = editor_menu(s, node->panels.front().tag());
			const rectf header = rectf::from_position_size(leaf.rect.top_left(), { leaf.rect.width(), gui::menu_chrome_height(s.fonts, host, sty, leaf.rect.width()) });
			if (!header.contains(in.mouse)) {
				continue;
			}
			const std::optional<std::uint32_t> tab = gui::tab_index_at(s.fonts, host, sty, header, in.mouse);
			d.drag = dock_drag{
				.panel = node->panels[tab.value_or(node->active_panel)],
				.start = in.mouse,
				.header = header,
			};
			break;
		}
	}

	d.layout = update_dividers(d.tree, d.frame, metrics, panels, {
		.mouse = in.mouse,
		.pressed = in.pressed,
		.held = in.held,
		.blocked = in.blocked || d.drag.has_value(),
	});

	d.frame_cursor = cursor_shape::arrow;
	if (in.blocked || d.drag) {
		return;
	}

	const dock_divider* hovered = divider_at(d.layout, in.mouse);
	if (const std::optional<gui::layout::split_axis> axis = dragging_axis(d.tree); axis || hovered) {
		const gui::layout::split_axis shape = axis ? *axis : hovered->axis;
		d.frame_cursor = shape == gui::layout::split_axis::columns ? cursor_shape::resize_ew : cursor_shape::resize_ns;
	}
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

auto gse::ide::editor_app::run(context& ctx, data& d, const channel_read<window_open_file_result, toggle_project_switcher_request, toggle_settings_request, open_panels_menu_request, gui::context_menu_result> requests_in, const channel_write<gui::push_screen_request, settings::change_request, gui::popout_toggle, set_cursor_shape_request, jump_to_request, window_launcher_mode_request, window_open_file_request, build_runner::build_request, toggle_project_switcher_request, toggle_settings_request, open_panels_menu_request> ui_out, const shared_view<search_system::data> search_d, const shared_view<input::data> input_d, const shared_view<window::data> window_d, const save::registry& save_reg) -> async::task<> {
	if (!d.initialized) {
		load_editor_layout(d);
		d.save_clock.reset();
		d.initialized = true;
	}

	if (!d.screen_pushed && search_d.index) {
		ui_out.push<gui::push_screen_request>({
			.factory = [channels = channel_write<build_runner::build_request, jump_to_request, toggle_project_switcher_request, toggle_settings_request, open_panels_menu_request>(ui_out), index = search_d.index, input_d] {
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

	for (const auto& req : requests_in.of<open_panels_menu_request>()) {
		d.pending_panels_menu = req.position;
	}

	const std::span<const panel_desc> registry = editor_panels();
	for (const auto& res : requests_in.of<gui::context_menu_result>()) {
		if (res.tag != panels_context_tag()) {
			continue;
		}
		if (res.action_id == reset_layout_action) {
			d.tree = default_editor_tree();
			d.layout_dirty = true;
		}
		else if (res.action_id < registry.size()) {
			toggle_panel(d, registry[res.action_id].id);
		}
	}

	const input::state& input = input::current_state(input_d);
	const vec2f mouse = input.mouse_position();
	const bool pressed = input.mouse_button_pressed(mouse_button::button_1);
	const bool held = input.mouse_button_held(mouse_button::button_1);
	const bool context_pressed = input.mouse_button_pressed(mouse_button::button_2);
	const bool ctrl = input.key_held(key::left_control) || input.key_held(key::right_control);
	const bool toggle_maximize = ctrl && input.key_pressed(key::m);

	if (window_d.shown) {
		ui_out.push<settings::change_request>({
			.state_type = id_of<gui::data>(),
			.apply = [&d, mouse, pressed, held, context_pressed, toggle_maximize](void* p) {
				gui::data& s = *static_cast<gui::data*>(p);
				update_dock_interaction(s, d, {
					.mouse = mouse,
					.pressed = pressed,
					.held = held,
					.blocked = s.primary.menu_stack.captures_input() || s.primary.context_menu.open || s.primary.input_layers_data.is_resize_blocked(mouse),
					.context_pressed = context_pressed,
					.toggle_maximize = toggle_maximize,
				});
				sync_dock_menus(s, d);
			},
		});
	}

	if (d.frame_cursor != cursor_shape::arrow) {
		ui_out.push<set_cursor_shape_request>({ .shape = d.frame_cursor });
	}

	if (d.layout_dirty || d.save_clock.elapsed() > editor_layout_save_interval) {
		save_editor_layout(d);
		d.layout_dirty = false;
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
	if (!workspace::game_active(d.ws) || !game_running || !window_d.focused) {
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
		.build = [ws, index = search_d.index, config, channels = ui_out, input_snapshot, viewport_slot, game_running, building, game_extent](gui::builder& b) {
			draw_code_panel(b, input_snapshot, *ws, channels, {
				.config = config,
				.index = index,
				.viewport_slot = viewport_slot,
				.game_extent = game_extent,
				.game_running = game_running,
				.building = building,
			});
		},
	});

	ui_out.push<gui::menu_content>({
		.menu = std::string(graph_panel_name),
		.layer = render_layer::content,
		.build = [graph = &d.graph, index = search_d.index, channels = ui_out, input_snapshot](gui::builder& b) {
			draw_graph(b, input_snapshot, b.ctx.clip_stack.back(), *graph, index, channels);
		},
	});

	ui_out.push<gui::menu_content>({
		.menu = std::string(profile_panel_name),
		.layer = render_layer::content,
		.build = [state = &d.ws.profile, channels = ui_out, profile_d](gui::builder& b) {
			draw_profile_panel(b, b.ctx.clip_stack.back(), *state, profile_d.frames, profile_d.report, profile_d.report_loaded, channels);
		},
	});

	ui_out.push<gui::menu_content>({
		.menu = std::string(alloc_panel_name),
		.layer = render_layer::content,
		.build = [state = &d.ws.alloc](gui::builder& b) {
			draw_alloc_panel(b, b.ctx.clip_stack.back(), *state);
		},
	});

	ui_out.push<gui::menu_content>({
		.menu = std::string(problems_panel_name),
		.layer = render_layer::content,
		.build = [ws, state = &d.problems, channels = ui_out](gui::builder& b) {
			draw_problems_panel(b, b.ctx.clip_stack.back(), *state, *ws, channels);
		},
	});

	ui_out.push<gui::menu_content>({
		.menu = std::string(search_panel_name),
		.layer = render_layer::content,
		.build = [state = &d.search_panel, index = search_d.index, channels = ui_out, input_snapshot](gui::builder& b) {
			draw_search_panel(b, b.ctx.clip_stack.back(), *state, input_snapshot, index, channels);
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

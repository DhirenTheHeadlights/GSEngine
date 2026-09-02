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
import gse.ide.lint_panel;
import gse.ide.docs;
import gse.ide.viewport;
import gse.ide.profile;

import :chrome;
import :code_panel;
import :dock;
import :layout;
import :project_screen;

export namespace gse::ide {
	struct game_capture_request {
		std::uint32_t instance = 0;
		vec2f cursor;
		vec2f scale{ 1.f, 1.f };
	};

	struct session_view_state {
		dock_tree tree;
		dock_layout layout;
		bool built = false;
		build_runner::play_session shape;
		std::optional<game_capture_request> capture;
		std::optional<std::uint32_t> captured;
	};

	struct pending_popout {
		id lead;
		dock_tree tree;
		clock since;
	};

	namespace editor_app {
		struct [[= system_state<"Editor">{}]] data {
			bool initialized = false;
			bool screen_pushed = false;
			std::vector<dock_view> views;
			std::optional<dock_drag> drag;
			id drag_window;
			std::optional<id> cursor_window;
			vec2f cursor_client;
			vec2i cursor_screen;
			std::vector<dock_migration> pending_migrations;
			std::optional<vec2f> pending_panels_menu;
			id panels_menu_window;
			cursor_shape frame_cursor = cursor_shape::arrow;
			bool layout_dirty = false;
			bool game_panel_open = false;
			std::string session_error;
			bool session_dismissed = false;
			std::vector<dock_popout> popout_queue;
			std::vector<dock_window_layout> pending_restores;
			std::vector<pending_popout> pending_popouts;
			std::vector<id> pending_window_closes;
			clock save_clock;
		};

		[[= system_run<>{}]]
		auto run(
			context& ctx,
			data& d,
			channel_read<window_open_file_result, toggle_project_switcher_request, toggle_settings_request, open_panels_menu_request, gui::context_menu_result, window_opened, window_closed, window_resized, window_moved, window_cursor_located> requests_in,
			channel_write<gui::push_screen_request, settings::change_request, settings::override_request, gui::popout_toggle, set_cursor_shape_request, jump_to_request, window_launcher_mode_request, window_open_file_request, build_runner::build_request, toggle_project_switcher_request, toggle_settings_request, open_panels_menu_request, window_popout_request, window_close_request, window_locate_cursor_request, gui::menu_migrate_request, build_runner::stop_session_request> ui_out,
			shared_view<search_system::data> search_d,
			shared_view<input::data> input_d,
			shared_view<window::data> window_d,
			shared_view<build_runner::data> build_d,
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
			lint_panel_state lint_panel;
			git::status_snapshot git_status;
			std::vector<std::filesystem::path> git_rootless;
			bool initialized = false;
			bool cursor_capture_sent = false;
			bool profile_capture_sent = false;
			bool game_was_running = false;
			profile_source profile_source_sent = profile_source::editor;
			bool game_input_forwarding = false;
			session_view_state session_view;
			clock save_clock;
		};

		[[= system_run<>{}]]
		auto run(
			context& ctx,
			data& d,
			channel_read<git::status_updated, jump_to_request, apply_lint_request, gui::context_menu_result, analysis::diagnostics_completed, build_runner::build_finished> requests_in,
			channel_write<gui::menu_content, cursor_capture_request, profile_capture_request, profile_report_request, build_runner::attached_input, build_runner::build_request, git_system::init_request, jump_to_request, apply_lint_request, toggle_project_switcher_request, toggle_settings_request, analysis::diagnostics_request, git_system::refresh_request, set_cursor_shape_request, search::index_merge_request> ui_out,
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
	constexpr std::string_view lint_panel_name = "Lints";
	constexpr std::string_view game_panel_name = "Game";
	constexpr std::string_view server_quadrant_name = "Server";
	constexpr std::array<std::string_view, 3> client_quadrant_names{ "Client 1", "Client 2", "Client 3" };
	constexpr float server_strip_ratio = 0.25f;
	constexpr time editor_layout_save_interval = seconds(30.f);
	constexpr time popout_open_timeout = seconds(2.f);
	constexpr time system_graph_retry_interval = milliseconds(100.f);
	constexpr std::uint32_t system_graph_max_attempts = 100;

	constexpr std::uint32_t reset_layout_action = 0xFFFFFFFF;

	struct dock_input {
		vec2f mouse;
		bool pressed = false;
		bool held = false;
		bool context_pressed = false;
		bool toggle_maximize = false;
		std::span<const id> carried;
		std::optional<dock_landing> landing;
	};

	[[nodiscard]] auto panels_context_tag() -> id;

	[[nodiscard]] auto panels_menu_items(
		const dock_tree& tree,
		std::span<const panel_desc> panels,
		bool resettable
	) -> std::vector<gui::menu_item>;

	auto toggle_panel(
		editor_app::data& d,
		dock_view& v,
		id panel
	) -> void;

	auto apply_pending_panel_close(
		gui::viewport_state& vp,
		editor_app::data& d,
		dock_view& v
	) -> void;

	[[nodiscard]] auto editor_panels() -> std::span<const panel_desc>;

	[[nodiscard]] auto editor_dock_metrics(
		const gui::style& sty
	) -> dock_metrics;

	[[nodiscard]] auto default_editor_tree() -> dock_tree;

	[[nodiscard]] auto client_tile_count(
		const build_runner::play_session& session
	) -> std::size_t;

	[[nodiscard]] auto session_quadrants(
		const build_runner::play_session& session
	) -> std::vector<panel_desc>;

	[[nodiscard]] auto build_session_tree(
		const build_runner::play_session& session
	) -> dock_tree;

	auto open_session_layout(
		editor_app::data& d
	) -> void;

	auto close_session_layout(
		editor_app::data& d
	) -> void;

	auto load_editor_layout(
		editor_app::data& d
	) -> void;

	auto save_editor_layout(
		const editor_app::data& d
	) -> void;

	[[nodiscard]] auto primary_view(
		editor_app::data& d
	) -> dock_view&;

	[[nodiscard]] auto primary_view(
		const editor_app::data& d
	) -> const dock_view&;

	[[nodiscard]] auto find_view(
		editor_app::data& d,
		id window
	) -> dock_view*;

	struct dock_frame {
		gui::style sty;
		dock_metrics metrics;
	};

	[[nodiscard]] auto resolve_view_layout(
		gui::data& s,
		gui::viewport_state& vp,
		editor_app::data& d,
		dock_view& v
	) -> std::optional<dock_frame>;

	auto detach_panel_to_window(
		gui::viewport_state& vp,
		editor_app::data& d,
		dock_popout where
	) -> void;

	[[nodiscard]] auto dragged_panels(
		editor_app::data& d,
		const dock_drag& drag
	) -> std::vector<id>;

	[[nodiscard]] auto resolve_drop(
		editor_app::data& d,
		const dock_drag& drag
	) -> std::optional<dock_landing>;

	auto apply_dock_landing(
		editor_app::data& d,
		dock_view& from,
		const dock_drag& drag,
		std::span<const id> carried,
		const dock_landing& landing
	) -> void;

	auto sync_dock_menus(
		gui::viewport_state& vp,
		editor_app::data& d,
		dock_view& v
	) -> void;

	auto update_dock_interaction(
		gui::data& s,
		gui::viewport_state& vp,
		editor_app::data& d,
		dock_view& v,
		const dock_input& in
	) -> bool;

	auto forward_game_input(
		const input::state& input,
		channel_write<build_runner::attached_input> channels,
		std::uint32_t instance,
		vec2f game_cursor,
		bool release_held
	) -> void;

	[[nodiscard]] auto draw_quadrant(
		gui::builder& ui,
		const rectf& area,
		std::string_view title,
		std::uint32_t instance,
		bool building,
		gpu::bindless_slot slot,
		vec2u extent,
		bool live,
		const build_runner::server_status& server,
		std::optional<std::uint32_t> captured
	) -> std::optional<game_capture_request>;

	auto draw_session_failure(
		const gui::draw_context& ctx,
		const rectf& area,
		std::string_view message
	) -> void;

	auto draw_game_panel(
		gui::builder& ui,
		const rectf& area,
		session_view_state& state,
		bool building,
		const std::array<gpu::bindless_slot, build_runner::max_attached_instances>& slots,
		const std::array<vec2u, build_runner::max_attached_instances>& extents,
		const std::array<bool, build_runner::max_attached_instances>& live,
		const build_runner::play_session& session,
		const build_runner::server_status& server,
		std::string_view error
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
		panel_desc{
			.id = find_or_generate_id(lint_panel_name),
			.name = lint_panel_name,
			.min_size = { 340.f, 160.f },
			.start_hidden = true,
		},
		panel_desc{
			.id = find_or_generate_id(game_panel_name),
			.name = game_panel_name,
			.min_size = { 360.f, 240.f },
			.start_hidden = true,
			.menu_hidden = true,
		},
	};
	return table;
}

auto gse::ide::draw_quadrant(gui::builder& ui, const rectf& area, const std::string_view title, const std::uint32_t instance, const bool building, const gpu::bindless_slot slot, const vec2u extent, const bool live, const build_runner::server_status& server, const std::optional<std::uint32_t> captured) -> std::optional<game_capture_request> {
	const auto& ctx = ui.ctx;
	const auto text_view = ctx.fonts.text.resolve();
	const float font_sz = ctx.style.font_size;
	const float pad = ctx.style.padding;
	const float header_h = text_view->line_height(font_sz) + pad;

	const rectf header = rectf::from_position_size(area.top_left(), { area.width(), std::min(header_h, area.height()) });
	ctx.queue_sprite({
		.rect = header,
		.color = ctx.style.color_tab_background,
		.texture = ctx.blank_texture,
		.clip_rect = area,
	});
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = title,
		.position = { header.left() + pad, header.center().y() + text_view->vertical_center_offset(font_sz) },
		.scale = font_sz,
		.color = ctx.style.color_text_secondary,
		.clip_rect = header,
	});

	if (captured == instance) {
		constexpr std::string_view release_hint = "Alt+Tab to release";
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = release_hint,
			.position = { header.right() - pad - text_view->width(release_hint, font_sz), header.center().y() + text_view->vertical_center_offset(font_sz) },
			.scale = font_sz,
			.color = ctx.style.color_accent,
			.clip_rect = header,
		});
	}

	const rectf body = rectf::from_position_size(
		{ area.left(), area.top() - header.height() },
		{ area.width(), std::max(0.f, area.height() - header.height()) }
	);
	if (body.width() <= 0.f || body.height() <= 0.f) {
		return std::nullopt;
	}

	if (building) {
		draw_game_placeholder(ctx, body, "Building...");
		return std::nullopt;
	}

	const bool is_server = instance >= build_runner::max_attached_instances;
	if (is_server) {
		if (!server.running) {
			draw_game_placeholder(ctx, body, "Server exited");
			return std::nullopt;
		}
		const std::array<std::string, 2> rows{
			"Headless - no view",
			std::format("Listening on port {}", server.port),
		};
		float y = body.top() - pad - text_view->line_height(font_sz);
		for (const std::string& row : rows) {
			ctx.queue_text({
				.font = ctx.fonts.text,
				.text = row,
				.position = { body.left() + pad, y },
				.scale = font_sz,
				.color = ctx.style.color_text_secondary,
				.clip_rect = body,
			});
			y -= text_view->line_height(font_sz);
		}
		return std::nullopt;
	}

	if (!live || !slot.valid()) {
		draw_game_placeholder(ctx, body, std::format("{} not running", title));
		return std::nullopt;
	}

	const float aspect = extent.y() > 0 ? static_cast<float>(extent.x()) / static_cast<float>(extent.y()) : 1.f;
	const float fitted_h = std::min(body.height(), body.width() / std::max(aspect, 0.0001f));
	const float fitted_w = fitted_h * aspect;
	const rectf fitted = rectf::from_position_size(
		{ body.center().x() - fitted_w * 0.5f, body.center().y() + fitted_h * 0.5f },
		{ fitted_w, fitted_h }
	);
	ctx.queue_sprite({
		.rect = fitted,
		.color = { 1.f, 1.f, 1.f, 1.f },
		.clip_rect = body,
		.image_slot = slot,
	});

	if (fitted_w <= 0.f || fitted_h <= 0.f || !ctx.mouse_pressed_for(fitted)) {
		return std::nullopt;
	}

	const vec2f scale{
		static_cast<float>(extent.x()) / fitted_w,
		static_cast<float>(extent.y()) / fitted_h,
	};
	const vec2f mouse = ctx.mouse_position();
	return game_capture_request{
		.instance = instance,
		.cursor = {
			(mouse.x() - fitted.left()) * scale.x(),
			(fitted.top() - mouse.y()) * scale.y(),
		},
		.scale = scale,
	};
}

auto gse::ide::draw_session_failure(const gui::draw_context& ctx, const rectf& area, const std::string_view message) -> void {
	const auto text_view = ctx.fonts.text.resolve();
	const float font_sz = ctx.style.font_size;
	const float pad = ctx.style.padding;
	const float line_h = text_view->line_height(font_sz);
	const std::vector<std::string_view> lines = text_view->wrap(message, std::max(0.f, area.width() - pad * 4.f), font_sz);

	float y = area.center().y() + static_cast<float>(lines.size()) * line_h * 0.5f - line_h + text_view->vertical_center_offset(font_sz);
	for (const std::string_view line : lines) {
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = line,
			.position = { area.center().x() - text_view->width(line, font_sz) * 0.5f, y },
			.scale = font_sz,
			.color = ctx.style.color_error,
			.clip_rect = area,
		});
		y -= line_h;
	}
}

auto gse::ide::draw_game_panel(gui::builder& ui, const rectf& area, session_view_state& state, const bool building, const std::array<gpu::bindless_slot, build_runner::max_attached_instances>& slots, const std::array<vec2u, build_runner::max_attached_instances>& extents, const std::array<bool, build_runner::max_attached_instances>& live, const build_runner::play_session& session, const build_runner::server_status& server, const std::string_view error) -> void {
	const auto& ctx = ui.ctx;
	if (area.width() <= 0.f || area.height() <= 0.f) {
		return;
	}
	if (!error.empty()) {
		draw_session_failure(ctx, area, error);
		return;
	}
	if (!state.built || state.shape.clients != session.clients || state.shape.dedicated_server != session.dedicated_server) {
		state.tree = build_session_tree(session);
		state.shape = session;
		state.built = true;
	}

	const std::vector<panel_desc> quadrants = session_quadrants(session);
	const dock_metrics metrics{
		.scale = ctx.style.scale_factor,
		.divider_thickness = ctx.style.accent_bar_width * 2.f,
		.header_height = 0.f,
	};
	state.layout = resolve(state.tree, area, metrics, quadrants);

	for (const dock_placement& leaf : state.layout.leaves) {
		const dock_node* node = state.tree.nodes.try_get(leaf.node);
		if (!node || node->panels.empty()) {
			continue;
		}
		const id panel = node->panels[node->active_panel];
		const auto desc = std::ranges::find(quadrants, panel, &panel_desc::id);
		if (desc == quadrants.end()) {
			continue;
		}
		std::uint32_t instance = build_runner::max_attached_instances;
		for (std::uint32_t i = 0; i < client_quadrant_names.size(); ++i) {
			if (desc->name == client_quadrant_names[i]) {
				instance = i;
				break;
			}
		}
		if (std::optional<game_capture_request> pressed = draw_quadrant(
			ui,
			leaf.rect,
			desc->name,
			instance,
			building,
			instance < build_runner::max_attached_instances ? slots[instance] : gpu::bindless_slot{},
			instance < build_runner::max_attached_instances ? extents[instance] : vec2u{},
			instance < build_runner::max_attached_instances ? live[instance] : false,
			server,
			state.captured
		)) {
			state.capture = *pressed;
		}
	}

	for (const dock_divider& divider : state.layout.dividers) {
		ctx.queue_sprite({
			.rect = divider.rect,
			.color = ctx.style.color_accent,
			.texture = ctx.blank_texture,
			.clip_rect = area,
		});
	}
}

auto gse::ide::editor_dock_metrics(const gui::style& sty) -> dock_metrics {
	return {
		.scale = sty.scale_factor,
		.divider_thickness = 16.f,
	};
}

auto gse::ide::client_tile_count(const build_runner::play_session& session) -> std::size_t {
	return std::clamp<std::size_t>(session.clients, 1, client_quadrant_names.size());
}

auto gse::ide::session_quadrants(const build_runner::play_session& session) -> std::vector<panel_desc> {
	const std::size_t clients = client_tile_count(session);
	std::vector<panel_desc> table;
	table.reserve(clients + 1);

	for (std::size_t i = 0; i < clients; ++i) {
		table.push_back({
			.id = find_or_generate_id(client_quadrant_names[i]),
			.name = client_quadrant_names[i],
			.min_size = { 160.f, 120.f },
		});
	}
	if (session.dedicated_server) {
		table.push_back({
			.id = find_or_generate_id(server_quadrant_name),
			.name = server_quadrant_name,
			.min_size = { 160.f, 120.f },
		});
	}
	return table;
}

auto gse::ide::build_session_tree(const build_runner::play_session& session) -> dock_tree {
	const std::size_t clients = client_tile_count(session);
	const id first = find_or_generate_id(client_quadrant_names[0]);

	dock_tree tree;
	insert_panel(tree, {
		.panel = first,
	});

	if (session.dedicated_server) {
		insert_panel(tree, {
			.panel = find_or_generate_id(server_quadrant_name),
			.target = find_leaf(tree, first),
			.location = gui::dock::location::bottom,
			.ratio = server_strip_ratio,
		});
	}

	for (std::size_t i = 1; i < clients; ++i) {
		const float remaining = static_cast<float>(clients - i);
		insert_panel(tree, {
			.panel = find_or_generate_id(client_quadrant_names[i]),
			.target = find_leaf(tree, find_or_generate_id(client_quadrant_names[i - 1])),
			.location = gui::dock::location::right,
			.ratio = remaining / (remaining + 1.f),
		});
	}
	return tree;
}

auto gse::ide::open_session_layout(editor_app::data& d) -> void {
	dock_tree& tree = primary_view(d).tree;
	const id game = find_or_generate_id(game_panel_name);
	if (contains_panel(tree, game)) {
		return;
	}

	insert_panel(tree, {
		.panel = game,
		.target = any_leaf(tree),
		.location = gui::dock::location::right,
		.ratio = 0.5f,
	});
	activate_panel(tree, game);
	d.layout_dirty = true;
}

auto gse::ide::close_session_layout(editor_app::data& d) -> void {
	dock_tree& tree = primary_view(d).tree;
	const id game = find_or_generate_id(game_panel_name);
	if (!contains_panel(tree, game)) {
		return;
	}
	remove_panel(tree, game);
	d.layout_dirty = true;
}


auto gse::ide::default_editor_tree() -> dock_tree {
	const id code = find_or_generate_id(code_panel_name);
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
	std::optional<dock_tree> restored = deserialize_tree(sections, editor_panels(), primary_tree_sections());
	d.views.clear();
	d.views.push_back(dock_view{
		.tree = restored ? std::move(*restored) : default_editor_tree(),
	});

	d.pending_restores.clear();
	if (!restored) {
		return;
	}

	std::vector<id> claimed = panels_of(d.views.front().tree);
	for (dock_window_layout& window : deserialize_windows(sections, editor_panels())) {
		const std::vector<id> wanted = panels_of(window.tree);
		if (std::ranges::any_of(wanted, [&claimed](const id panel) {
			return std::ranges::find(claimed, panel) != claimed.end();
		})) {
			continue;
		}
		claimed.insert(claimed.end(), wanted.begin(), wanted.end());
		d.pending_restores.push_back(std::move(window));
	}
}

auto gse::ide::save_editor_layout(const editor_app::data& d) -> void {
	if (d.views.empty()) {
		return;
	}

	std::vector<dock_window_layout> windows;
	windows.reserve(d.views.size());
	for (const dock_view& view : d.views | std::views::drop(1)) {
		if (view.window_size.x() <= 0 || view.window_size.y() <= 0 || panel_count(view.tree) == 0) {
			continue;
		}
		windows.push_back({
			.tree = view.tree,
			.position = view.window_position,
			.size = view.window_size,
		});
	}
	for (const dock_window_layout& pending : d.pending_restores) {
		windows.push_back(pending);
	}

	replace_layout_sections(
		editor_layout_owner(),
		serialize_tree(primary_view(d).tree, editor_panels(), primary_tree_sections()) + serialize_windows(windows, editor_panels())
	);
}

auto gse::ide::primary_view(editor_app::data& d) -> dock_view& {
	return d.views.front();
}

auto gse::ide::find_view(editor_app::data& d, const id window) -> dock_view* {
	const auto found = std::ranges::find(d.views, window, &dock_view::window);
	return found == d.views.end() ? nullptr : &*found;
}

auto gse::ide::primary_view(const editor_app::data& d) -> const dock_view& {
	return d.views.front();
}

auto gse::ide::panels_context_tag() -> id {
	return find_or_generate_id("panels_context");
}

auto gse::ide::panels_menu_items(const dock_tree& tree, const std::span<const panel_desc> panels, const bool resettable) -> std::vector<gui::menu_item> {
	std::vector<gui::menu_item> items;
	items.reserve(panels.size() + 1);

	for (std::size_t i = 0; i < panels.size(); ++i) {
		if (panels[i].menu_hidden) {
			continue;
		}
		const bool visible = contains_panel(tree, panels[i].id);
		items.push_back({
			.label = std::string(panels[i].name),
			.action_id = static_cast<std::uint32_t>(i),
			.enabled = !visible || panel_count(tree) > 1 || !resettable,
			.checkable = true,
			.checked = visible,
		});
	}

	if (resettable) {
		items.push_back({
			.label = "Reset Layout",
			.action_id = reset_layout_action,
			.separator_before = true,
		});
	}
	return items;
}

auto gse::ide::toggle_panel(editor_app::data& d, dock_view& v, const id panel) -> void {
	if (contains_panel(v.tree, panel)) {
		if (panel_count(v.tree) <= 1 && !is_popout(v)) {
			return;
		}
		remove_panel(v.tree, panel);
		if (is_popout(v) && panel_count(v.tree) == 0) {
			d.pending_window_closes.push_back(v.window);
		}
	}
	else {
		insert_panel(v.tree, {
			.panel = panel,
			.target = any_leaf(v.tree),
			.location = gui::dock::location::center,
		});
		activate_panel(v.tree, panel);
	}
	d.layout_dirty = true;
}

auto gse::ide::apply_pending_panel_close(gui::viewport_state& vp, editor_app::data& d, dock_view& v) -> void {
	if (!vp.pending_tab_close) {
		return;
	}

	const auto [host_id, tab_index] = *vp.pending_tab_close;
	const gui::menu* host = vp.menus.try_get(host_id);
	if (!host || tab_index >= host->tab_contents.size()) {
		return;
	}

	const std::optional<id> panel = try_find(host->tab_contents[tab_index]);
	const std::span<const panel_desc> panels = editor_panels();
	if (!panel || std::ranges::find(panels, *panel, &panel_desc::id) == panels.end()) {
		return;
	}

	vp.pending_tab_close.reset();
	if (panel_count(v.tree) <= 1 && !is_popout(v)) {
		return;
	}

	remove_panel(v.tree, *panel);
	if (is_popout(v) && panel_count(v.tree) == 0) {
		d.pending_window_closes.push_back(v.window);
	}
	d.layout_dirty = true;
}

auto gse::ide::detach_panel_to_window(gui::viewport_state& vp, editor_app::data& d, dock_popout where) -> void {
	gui::menu& detached = editor_menu(vp, where.lead.tag());
	detached.swap_parent(id());
	detached.docked_to = gui::dock::location::none;
	detached.fixed = false;
	detached.bare = false;
	detached.tab_contents.assign(1, std::string(where.lead.tag()));
	detached.active_tab_index = 0;
	detached.tabs_closeable = true;
	detached.accent_edge.reset();

	const std::span<const panel_desc> panels = editor_panels();
	if (const auto desc = std::ranges::find(panels, where.lead, &panel_desc::id); desc != panels.end()) {
		detached.accent_edge = desc->accent_edge;
	}

	d.pending_popouts.push_back({ .lead = where.lead, .tree = where.tree });
	d.popout_queue.push_back(std::move(where));
}

auto gse::ide::dragged_panels(editor_app::data& d, const dock_drag& drag) -> std::vector<id> {
	if (drag.group.exists()) {
		if (const dock_view* origin = find_view(d, d.drag_window)) {
			if (const dock_node* leaf = origin->tree.nodes.try_get(drag.group)) {
				return leaf->panels;
			}
		}
	}
	return { drag.panel };
}

auto gse::ide::resolve_drop(editor_app::data& d, const dock_drag& drag) -> std::optional<dock_landing> {
	if (!drag.torn || !d.cursor_window) {
		return std::nullopt;
	}

	dock_view* target = find_view(d, *d.cursor_window);
	if (!target) {
		return std::nullopt;
	}

	const dock_drag local = target->window == d.drag_window
		? drag
		: dock_drag{ .panel = drag.panel, .start = drag.start, .header = drag.header, .torn = drag.torn };

	const std::optional<dock_drop> hit = drop_target(target->tree, target->layout, target->metrics, local, d.cursor_client);
	if (!hit) {
		return std::nullopt;
	}

	return dock_landing{
		.window = target->window,
		.drop = *hit,
	};
}

auto gse::ide::apply_dock_landing(editor_app::data& d, dock_view& from, const dock_drag& drag, const std::span<const id> carried, const dock_landing& landing) -> void {
	dock_view* to = find_view(d, landing.window);
	if (!to) {
		return;
	}

	if (drag.group.exists()) {
		insert_group(from.tree, to->tree, drag.group, landing.drop);
	}
	else {
		remove_panel(from.tree, drag.panel);
		insert_panel(to->tree, {
			.panel = drag.panel,
			.target = landing.drop.node,
			.location = landing.drop.space.hot,
		});
		activate_panel(to->tree, drag.panel);
	}

	if (to != &from) {
		for (const id panel : carried) {
			d.pending_migrations.push_back({ .panel = panel, .window = to->window });
		}
		if (is_popout(from) && panel_count(from.tree) == 0) {
			d.pending_window_closes.push_back(from.window);
		}
	}

	d.layout_dirty = true;
}

auto gse::ide::sync_dock_menus(gui::viewport_state& vp, editor_app::data& d, dock_view& v) -> void {
	const std::span<const panel_desc> panels = editor_panels();
	std::vector<id> live;
	std::vector<id> shown;
	live.reserve(v.layout.leaves.size());

	for (const dock_placement& leaf : v.layout.leaves) {
		dock_node* node = v.tree.nodes.try_get(leaf.node);
		gui::menu& host = editor_menu(vp, node->panels.front().tag());

		if (host.tab_contents.size() == node->panels.size()) {
			std::vector<id> reordered;
			reordered.reserve(node->panels.size());
			for (const std::string& tab : host.tab_contents) {
				const std::optional<id> panel = try_find(tab);
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
		host.swap_parent(id());
		host.docked_to = gui::dock::location::none;
		host.fixed = true;
		host.bare = node->panels.size() == 1;
		host.tab_contents.clear();
		host.tab_contents.reserve(node->panels.size());
		for (const id panel : node->panels) {
			host.tab_contents.emplace_back(panel.tag());
			shown.push_back(panel);
		}
		host.active_tab_index = node->active_panel;
		host.tabs_closeable = true;
		host.accent_edge.reset();
		for (const id panel : node->panels) {
			const auto desc = std::ranges::find(panels, panel, &panel_desc::id);
			if (desc != panels.end() && desc->accent_edge) {
				host.accent_edge = desc->accent_edge;
				break;
			}
		}
		live.push_back(host.id());
	}

	vp.suppressed_menus.clear();
	for (const panel_desc& desc : panels) {
		if (std::ranges::find(shown, desc.id) == shown.end()) {
			vp.suppressed_menus.insert(stable_id(desc.name));
		}
	}

	auto awaiting_popout = [&d](const std::string_view tag) {
		return std::ranges::any_of(d.pending_popouts, [tag](const pending_popout& p) { return p.lead.tag() == tag; });
	};

	std::vector<id> stale;
	for (const gui::menu& m : vp.menus.items()) {
		const std::string_view tag = m.id().tag();
		const bool owned = std::ranges::any_of(panels, [tag](const panel_desc& desc) {
			return desc.name == tag;
		});
		if (owned && !awaiting_popout(tag) && std::ranges::find(live, m.id()) == live.end()) {
			stale.push_back(m.id());
		}
	}
	for (const id menu_id : stale) {
		vp.menus.remove(menu_id);
	}
}

auto gse::ide::resolve_view_layout(gui::data& s, gui::viewport_state& vp, editor_app::data& d, dock_view& v) -> std::optional<dock_frame> {
	const vec2f viewport_size = vp.previous_viewport_size;
	if (viewport_size.x() <= 0.f || viewport_size.y() <= 0.f) {
		return std::nullopt;
	}

	const gui::style sty = gui::apply_scale(s, vp, gui::style::from_theme(s.current_theme), viewport_size.y());
	const float inset = s.reserve_top_bar ? sty.title_bar_height : 0.f;
	const float top = viewport_size.y() - inset;
	if (top <= 0.f) {
		return std::nullopt;
	}

	const dock_metrics metrics = editor_dock_metrics(sty);
	v.metrics = metrics;
	v.frame = rectf::from_position_size({ 0.f, top }, { viewport_size.x(), top });

	apply_pending_panel_close(vp, d, v);
	v.layout = resolve(v.tree, v.frame, metrics, editor_panels());

	return dock_frame{
		.sty = sty,
		.metrics = metrics,
	};
}

auto gse::ide::update_dock_interaction(gui::data& s, gui::viewport_state& vp, editor_app::data& d, dock_view& v, const dock_input& in) -> bool {
	const std::optional<dock_frame> frame = resolve_view_layout(s, vp, d, v);
	if (!frame) {
		return false;
	}

	const gui::style sty = frame->sty;
	const dock_metrics metrics = frame->metrics;
	const std::span<const panel_desc> panels = editor_panels();
	const bool blocked = !vp.owns_cursor
		|| vp.menu_stack.captures_input()
		|| vp.context_menu.open
		|| vp.input_layers_data.is_resize_blocked(in.mouse);

	if (in.toggle_maximize && !d.drag && vp.owns_cursor) {
		if (v.tree.maximized.exists()) {
			v.tree.maximized.reset();
		}
		else if (const auto hit = std::ranges::find_if(v.layout.leaves, [&in](const dock_placement& leaf) {
			return leaf.rect.contains(in.mouse);
		}); hit != v.layout.leaves.end()) {
			const dock_node* node = v.tree.nodes.try_get(hit->node);
			v.tree.maximized = node->panels[node->active_panel];
		}
		v.layout = resolve(v.tree, v.frame, metrics, panels);
		d.layout_dirty = true;
	}

	if (d.pending_panels_menu && d.panels_menu_window == v.window) {
		vp.context_menu = {
			.open = true,
			.just_opened = true,
			.position = *d.pending_panels_menu,
			.items = panels_menu_items(v.tree, panels, !is_popout(v)),
			.tag = panels_context_tag(),
		};
		d.pending_panels_menu.reset();
	}

	vp.active_dock_space.reset();
	vp.active_drag_ghost.reset();

	if (d.drag) {
		const bool source = d.drag_window == v.window;

		if (source && in.held && !d.drag->torn && !d.drag->header.contains(in.mouse) && distance(in.mouse, d.drag->start) > metrics.tear_threshold * metrics.scale) {
			d.drag->torn = true;
		}

		const std::span<const id> carried = in.carried;
		const std::optional<dock_landing>& landing = in.landing;
		const bool hovered = d.cursor_window && *d.cursor_window == v.window;

		if (in.held) {
			if (landing && landing->window == v.window) {
				vp.active_dock_space = landing->drop.space;
			}

			if (d.drag->torn && (hovered || (!d.cursor_window && source))) {
				vp.active_drag_ghost = gui::drag_ghost{
					.label = carried.size() > 1
						? std::format("{} +{}", d.drag->panel.tag(), carried.size() - 1)
						: std::string(d.drag->panel.tag()),
					.position = hovered ? d.cursor_client : in.mouse,
					.detaching = !landing && !d.cursor_window && panel_count(v.tree) > carried.size(),
				};
			}

			if (source) {
				d.frame_cursor = d.drag->torn ? cursor_shape::hand : cursor_shape::arrow;
			}
			return true;
		}

		if (!source) {
			return true;
		}

		if (landing && landing->drop.space.hot != gui::dock::location::none) {
			apply_dock_landing(d, v, *d.drag, carried, *landing);
		}
		else if (d.drag->torn && !d.cursor_window && panel_count(v.tree) > carried.size()) {
			dock_tree detached;
			for (const id panel : carried) {
				remove_panel(v.tree, panel);
				insert_panel(detached, { .panel = panel });
			}
			activate_panel(detached, d.drag->panel);
			detach_panel_to_window(vp, d, {
				.tree = std::move(detached),
				.lead = carried.front(),
				.screen_position = d.cursor_screen,
				.size = vec2i{
					static_cast<int>(std::max(d.drag->header.width(), 480.f)),
					static_cast<int>(std::max(v.frame.height() * 0.5f, 320.f)),
				},
			});
			d.layout_dirty = true;
		}

		v.layout = resolve(v.tree, v.frame, metrics, panels);
		d.drag.reset();
		d.frame_cursor = cursor_shape::arrow;
		return true;
	}

	const std::optional<gui::layout::split_axis> held_axis = dragging_axis(v.tree);

	if (in.context_pressed && !blocked) {
		for (const dock_placement& leaf : v.layout.leaves) {
			const dock_node* node = v.tree.nodes.try_get(leaf.node);
			const gui::menu& host = editor_menu(vp, node->panels.front().tag());
			const rectf header = rectf::from_position_size(leaf.rect.top_left(), { leaf.rect.width(), gui::menu_chrome_height(s.fonts, host, sty, leaf.rect.width()) });
			if (header.contains(in.mouse)) {
				d.pending_panels_menu = in.mouse;
				d.panels_menu_window = v.window;
				break;
			}
		}
	}

	if (in.pressed && !blocked && !held_axis) {
		for (const dock_placement& leaf : v.layout.leaves) {
			const dock_node* node = v.tree.nodes.try_get(leaf.node);
			const gui::menu& host = editor_menu(vp, node->panels.front().tag());
			const rectf header = rectf::from_position_size(leaf.rect.top_left(), { leaf.rect.width(), gui::menu_chrome_height(s.fonts, host, sty, leaf.rect.width()) });
			if (!header.contains(in.mouse)) {
				continue;
			}
			const std::optional<std::uint32_t> tab = gui::tab_index_at(s.fonts, host, sty, header, in.mouse);
			d.drag = dock_drag{
				.panel = node->panels[tab.value_or(node->active_panel)],
				.group = !tab && node->panels.size() > 1 ? leaf.node : id{},
				.start = in.mouse,
				.header = header,
			};
			d.drag_window = v.window;
			break;
		}
	}

	v.layout = update_dividers(v.tree, v.frame, metrics, panels, {
		.mouse = in.mouse,
		.pressed = in.pressed,
		.held = in.held,
		.blocked = blocked || d.drag.has_value(),
	});

	if (blocked || d.drag) {
		return true;
	}

	d.frame_cursor = cursor_shape::arrow;

	const dock_divider* hovered = divider_at(v.layout, in.mouse);
	if (const std::optional<gui::layout::split_axis> axis = dragging_axis(v.tree); axis || hovered) {
		const gui::layout::split_axis shape = axis ? *axis : hovered->axis;
		d.frame_cursor = shape == gui::layout::split_axis::columns ? cursor_shape::resize_ew : cursor_shape::resize_ns;
	}
	return true;
}

auto gse::ide::forward_game_input(const input::state& input, const channel_write<build_runner::attached_input> channels, const std::uint32_t instance, const vec2f game_cursor, const bool release_held) -> void {
	const auto game_x = static_cast<double>(game_cursor.x());
	const auto game_y = static_cast<double>(game_cursor.y());

	if (release_held) {
		for (const key held : input.keys_held()) {
			if (held != key::escape) {
				channels.push<build_runner::attached_input>({ .instance = instance, .event = gse::input::key_released{ .key_code = held } });
			}
		}
		for (const mouse_button held : input.mouse_buttons_held()) {
			channels.push<build_runner::attached_input>({ .instance = instance, .event = gse::input::mouse_button_released{ .button = held, .x_pos = game_x, .y_pos = game_y } });
		}
		return;
	}

	channels.push<build_runner::attached_input>({ .instance = instance, .event = gse::input::mouse_moved{ .x_pos = game_x, .y_pos = game_y } });
	for (const key pressed : input.keys_pressed()) {
		channels.push<build_runner::attached_input>({ .instance = instance, .event = gse::input::key_pressed{ .key_code = pressed } });
	}
	for (const key released : input.keys_released()) {
		channels.push<build_runner::attached_input>({ .instance = instance, .event = gse::input::key_released{ .key_code = released } });
	}
	for (const mouse_button pressed : input.mouse_buttons_pressed()) {
		channels.push<build_runner::attached_input>({ .instance = instance, .event = gse::input::mouse_button_pressed{ .button = pressed, .x_pos = game_x, .y_pos = game_y } });
	}
	for (const mouse_button released : input.mouse_buttons_released()) {
		channels.push<build_runner::attached_input>({ .instance = instance, .event = gse::input::mouse_button_released{ .button = released, .x_pos = game_x, .y_pos = game_y } });
	}
	if (const vec2f scroll = input.scroll_delta(); scroll.x() != 0.f || scroll.y() != 0.f) {
		channels.push<build_runner::attached_input>({ .instance = instance, .event = gse::input::mouse_scrolled{ .x_offset = scroll.x(), .y_offset = scroll.y() } });
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
		channels.push<build_runner::attached_input>({ .instance = instance, .event = gse::input::text_entered{ .codepoint = codepoint } });
		i += length;
	}
}

auto gse::ide::editor_app::run(context& ctx, data& d, const channel_read<window_open_file_result, toggle_project_switcher_request, toggle_settings_request, open_panels_menu_request, gui::context_menu_result, window_opened, window_closed, window_resized, window_moved, window_cursor_located> requests_in, const channel_write<gui::push_screen_request, settings::change_request, settings::override_request, gui::popout_toggle, set_cursor_shape_request, jump_to_request, window_launcher_mode_request, window_open_file_request, build_runner::build_request, toggle_project_switcher_request, toggle_settings_request, open_panels_menu_request, window_popout_request, window_close_request, window_locate_cursor_request, gui::menu_migrate_request, build_runner::stop_session_request> ui_out, const shared_view<search_system::data> search_d, const shared_view<input::data> input_d, const shared_view<window::data> window_d, const shared_view<build_runner::data> build_d, const save::registry& save_reg) -> async::task<> {
	if (!d.screen_pushed && search_d.index) {
		ui_out.push<gui::push_screen_request>({
			.factory = [channels = channel_write<build_runner::build_request, jump_to_request, toggle_project_switcher_request, toggle_settings_request, open_panels_menu_request>(ui_out), index = search_d.index] {
				return std::make_unique<editor_screen>(channels, index, id());
			},
		});
		if (!project::opened()) {
			ui_out.push<gui::push_screen_request>({
				.factory = [channels = channel_write<window_launcher_mode_request, window_open_file_request>(ui_out)] {
					return std::make_unique<project_screen>(channels, true);
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
			.factory = [channels = channel_write<window_launcher_mode_request, window_open_file_request>(ui_out)] {
				return std::make_unique<project_screen>(channels);
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

	if (!d.initialized) {
		if (!project::opened()) {
			return {};
		}
		load_editor_layout(d);
		d.save_clock.reset();
		d.initialized = true;
	}

	const bool session_live = std::ranges::any_of(build_d.sessions, [](const build_runner::attached_session& session) {
		return session.generation != 0;
	});
	if (build_d.building_session || (!session_live && build_d.session_error.empty())) {
		d.session_dismissed = false;
	}
	if (build_d.session_error.empty()) {
		d.session_error.clear();
	}
	else if (!d.session_dismissed) {
		d.session_error = build_d.session_error;
	}

	const bool game_panel_open = contains_panel(primary_view(d).tree, find_or_generate_id(game_panel_name));
	if (d.game_panel_open && !game_panel_open && (session_live || !d.session_error.empty())) {
		d.session_dismissed = true;
		d.session_error.clear();
		ui_out.push<build_runner::stop_session_request>({});
	}

	if (!d.session_dismissed && (session_live || build_d.building_session || !d.session_error.empty())) {
		open_session_layout(d);
	}
	d.game_panel_open = contains_panel(primary_view(d).tree, find_or_generate_id(game_panel_name));

	if (!session_live && !build_d.building_session && d.session_error.empty()) {
		close_session_layout(d);
	}

	for (const dock_popout& queued : d.popout_queue) {
		ui_out.push<window_popout_request>({
			.menu_name = std::string(queued.lead.tag()),
			.title = std::string(queued.lead.tag()),
			.screen_position = queued.screen_position,
			.size = queued.size,
		});
	}
	d.popout_queue.clear();

	for (const auto& req : requests_in.of<window_opened>()) {
		if (req.for_menu.empty()) {
			continue;
		}
		const auto queued = std::ranges::find_if(d.pending_popouts, [&](const pending_popout& p) {
			return p.lead.tag() == req.for_menu;
		});
		if (queued == d.pending_popouts.end()) {
			continue;
		}

		const std::vector<id> opened = panels_of(queued->tree);
		d.views.push_back(dock_view{
			.window = req.id,
			.tree = std::move(queued->tree),
			.window_position = req.position,
			.window_size = req.size,
		});
		d.pending_popouts.erase(queued);
		ui_out.push<gui::push_screen_request>({
			.factory = [channels = channel_write<build_runner::build_request, jump_to_request, toggle_project_switcher_request, toggle_settings_request, open_panels_menu_request>(ui_out), index = search_d.index, window = req.id] {
				return std::make_unique<editor_screen>(channels, index, window);
			},
			.window = req.id,
		});
		std::erase_if(d.pending_restores, [&opened](const dock_window_layout& pending) {
			const std::vector<id> restored = panels_of(pending.tree);
			return std::ranges::any_of(restored, [&opened](const id panel) {
				return std::ranges::find(opened, panel) != opened.end();
			});
		});
		d.layout_dirty = true;
	}

	for (const pending_popout& stalled : d.pending_popouts) {
		if (stalled.since.elapsed() < popout_open_timeout) {
			continue;
		}
		for (const id panel : panels_of(stalled.tree)) {
			std::erase_if(d.pending_restores, [panel](const dock_window_layout& pending) {
				return contains_panel(pending.tree, panel);
			});
			if (contains_panel(primary_view(d).tree, panel)) {
				continue;
			}
			insert_panel(primary_view(d).tree, {
				.panel = panel,
				.target = {},
				.location = gui::dock::location::center,
			});
			activate_panel(primary_view(d).tree, panel);
		}
		d.layout_dirty = true;
	}
	std::erase_if(d.pending_popouts, [](const pending_popout& p) {
		return p.since.elapsed() >= popout_open_timeout;
	});

	for (const auto& req : requests_in.of<window_resized>()) {
		if (dock_view* view = find_view(d, req.id)) {
			view->window_size = req.size;
		}
	}

	for (const auto& req : requests_in.of<window_moved>()) {
		if (dock_view* view = find_view(d, req.id)) {
			view->window_position = req.position;
		}
	}

	for (const auto& req : requests_in.of<window_cursor_located>()) {
		if (req.source != d.drag_window) {
			continue;
		}
		d.cursor_window = req.window;
		d.cursor_client = req.client_cursor;
		d.cursor_screen = req.screen_cursor;
	}

	for (const dock_migration& migration : d.pending_migrations) {
		ui_out.push<gui::menu_migrate_request>({
			.menu_name = std::string(migration.panel.tag()),
			.target_window = migration.window,
		});
	}
	d.pending_migrations.clear();

	for (const id window : d.pending_window_closes) {
		ui_out.push<window_close_request>({ .window = window });
	}
	d.pending_window_closes.clear();

	for (const auto& req : requests_in.of<window_closed>()) {
		const auto view = std::ranges::find(d.views, req.id, &dock_view::window);
		if (view == d.views.end()) {
			continue;
		}

		const std::vector<id> orphans = panels_of(view->tree);
		d.views.erase(view);
		for (const id panel : orphans) {
			insert_panel(primary_view(d).tree, {
				.panel = panel,
				.target = {},
				.location = gui::dock::location::center,
			});
			activate_panel(primary_view(d).tree, panel);
		}
		d.layout_dirty = true;
	}

	for (const auto& req : requests_in.of<open_panels_menu_request>()) {
		d.pending_panels_menu = req.position;
		d.panels_menu_window = req.window;
	}

	const std::span<const panel_desc> registry = editor_panels();
	for (const auto& res : requests_in.of<gui::context_menu_result>()) {
		if (res.tag != panels_context_tag()) {
			continue;
		}
		dock_view* scope = find_view(d, d.panels_menu_window);
		if (!scope) {
			continue;
		}
		if (res.action_id == reset_layout_action) {
			scope->tree = default_editor_tree();
			d.layout_dirty = true;
		}
		else if (res.action_id < registry.size()) {
			toggle_panel(d, *scope, registry[res.action_id].id);
		}
	}

	const input::state& input = input::current_state(input_d);
	const vec2f mouse = input.mouse_position();
	const bool pressed = input.mouse_button_pressed(mouse_button::button_1);
	const bool held = input.mouse_button_held(mouse_button::button_1);
	const bool context_pressed = input.mouse_button_pressed(mouse_button::button_2);
	const bool ctrl = input.key_held(key::left_control) || input.key_held(key::right_control);
	const bool toggle_maximize = ctrl && input.key_pressed(key::m);

	if (d.drag && d.drag->torn) {
		ui_out.push<window_locate_cursor_request>({
			.source = d.drag_window,
			.client_cursor = mouse,
		});
	}
	else {
		d.cursor_window.reset();
	}

	if (window_d.primary.shown) {
		ui_out.push<settings::change_request>({
			.state_type = id_of<gui::data>(),
			.apply = [&d, mouse, pressed, held, context_pressed, toggle_maximize](void* p) {
				gui::data& s = *static_cast<gui::data*>(p);

				std::vector<id> carried;
				std::optional<dock_landing> landing;
				if (d.drag) {
					carried = dragged_panels(d, *d.drag);
					landing = resolve_drop(d, *d.drag);
				}

				const dock_input in{
					.mouse = mouse,
					.pressed = pressed,
					.held = held,
					.context_pressed = context_pressed,
					.toggle_maximize = toggle_maximize,
					.carried = carried,
					.landing = landing,
				};

				for (const dock_window_layout& pending : d.pending_restores) {
					const std::vector<id> restored = panels_of(pending.tree);
					if (restored.empty() || std::ranges::any_of(d.pending_popouts, [&restored](const pending_popout& queued) {
						return std::ranges::find(restored, queued.lead) != restored.end();
					})) {
						continue;
					}
					detach_panel_to_window(s.primary, d, {
						.tree = pending.tree,
						.lead = restored.front(),
						.screen_position = pending.position,
						.size = pending.size,
					});
				}

				if (update_dock_interaction(s, s.primary, d, primary_view(d), in)) {
					sync_dock_menus(s.primary, d, primary_view(d));
				}

				for (const auto& secondary : s.secondaries) {
					dock_view* sv = find_view(d, secondary->window);
					if (!sv) {
						continue;
					}
					if (update_dock_interaction(s, *secondary, d, *sv, in)) {
						sync_dock_menus(*secondary, d, *sv);
					}
				}
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

auto gse::ide::workspace_system::run(context& ctx, data& d, const channel_read<git::status_updated, jump_to_request, apply_lint_request, gui::context_menu_result, analysis::diagnostics_completed, build_runner::build_finished> requests_in, const channel_write<gui::menu_content, cursor_capture_request, profile_capture_request, profile_report_request, build_runner::attached_input, build_runner::build_request, git_system::init_request, jump_to_request, apply_lint_request, toggle_project_switcher_request, toggle_settings_request, analysis::diagnostics_request, git_system::refresh_request, set_cursor_shape_request, search::index_merge_request> ui_out, const scheduler& sched, const shared_view<config_system::data> config_d, const shared_view<search_system::data> search_d, const shared_view<input::data> input_d, const shared_view<viewport::data> viewport_d, const shared_view<build_runner::data> build_d, const shared_view<window::data> window_d, const shared_view<profile_system::data> profile_d) -> async::task<> {
	if (!d.initialized) {
		if (!project::opened()) {
			return {};
		}
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
	const auto session_active = [&build_d](const std::uint32_t instance) {
		return instance < build_runner::max_attached_instances
			&& build_d.sessions[instance].generation != 0
			&& build_d.sessions[instance].status == build_runner::attached_session_status::active;
	};
	const bool game_running = std::ranges::any_of(build_d.sessions, [](const build_runner::attached_session& session) {
		return session.generation != 0 && session.status == build_runner::attached_session_status::active;
	});

	if (!session_active(d.ws.game_instance) || !window_d.primary.focused) {
		d.ws.game_captured = false;
	}
	if (d.session_view.capture) {
		const game_capture_request request = *d.session_view.capture;
		d.session_view.capture.reset();
		if (session_active(request.instance) && window_d.primary.focused) {
			d.ws.game_captured = true;
			d.ws.game_instance = request.instance;
			d.ws.game_capture_settle_frames = 3;
			d.ws.game_cursor = request.cursor;
			d.ws.game_input_scale = request.scale;
		}
	}
	if (d.ws.game_captured) {
		if (d.ws.game_capture_settle_frames > 0) {
			--d.ws.game_capture_settle_frames;
		}
		else {
			const vec2f delta = input.mouse_delta();
			d.ws.game_cursor += vec2f{ delta.x() * d.ws.game_input_scale.x(), delta.y() * d.ws.game_input_scale.y() };
		}
		const bool release_chord = input.key_held(key::left_alt) || input.key_held(key::right_alt);
		const bool releasing = release_chord && input.key_pressed(key::tab);
		forward_game_input(input, ui_out, d.ws.game_instance, d.ws.game_cursor, releasing);
		if (releasing) {
			d.ws.game_captured = false;
		}
	}
	else if (d.game_input_forwarding) {
		forward_game_input(input, ui_out, d.ws.game_instance, d.ws.game_cursor, true);
	}
	d.game_input_forwarding = d.ws.game_captured;
	d.session_view.captured = d.ws.game_captured ? std::optional<std::uint32_t>{ d.ws.game_instance } : std::nullopt;
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
		.build = [ws, search = &d.search, index = search_d.index, channels = ui_out, git_status = d.git_status, git_rootless = &d.git_rootless](gui::builder& b) {
			draw_explorer_panel(b, *ws, *search, index, channels, git_status.get(), *git_rootless);
		},
	});


	const bool building = build_d.building;


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
		const std::filesystem::path run = profile::latest_run_dir(game_exe.stem().native_encoded_string());
		ui_out.push<profile_report_request>({
			.path = run.empty() ? run : run / profile::report_name,
		});
	}
	d.game_was_running = game_running;

	ui_out.push<gui::menu_content>({
		.menu = std::string(code_panel_name),
		.layer = render_layer::content,
		.build = [ws, index = search_d.index, config, channels = ui_out, building](gui::builder& b) {
			draw_code_panel(b, *ws, channels, {
				.config = config,
				.index = index,
				.building = building,
			});
		},
	});

	ui_out.push<gui::menu_content>({
		.menu = std::string(graph_panel_name),
		.layer = render_layer::content,
		.build = [graph = &d.graph, index = search_d.index, channels = ui_out](gui::builder& b) {
			draw_graph(b, b.ctx.clip_stack.back(), *graph, index, channels);
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
		.build = [state = &d.search_panel, index = search_d.index, channels = ui_out](gui::builder& b) {
			draw_search_panel(b, b.ctx.clip_stack.back(), *state, index, channels);
		},
	});

	ui_out.push<gui::menu_content>({
		.menu = std::string(lint_panel_name),
		.layer = render_layer::content,
		.build = [state = &d.lint_panel, index = search_d.index, channels = ui_out](gui::builder& b) {
			draw_lint_panel(b, b.ctx.clip_stack.back(), *state, index, channels);
		},
	});

	ui_out.push<gui::menu_content>({
		.menu = std::string(game_panel_name),
		.layer = render_layer::content,
		.build = [state = &d.session_view, building = build_d.building_session, slots = viewport_d.instance_slots, extents = viewport_d.instance_extents, live = viewport_d.instance_live, session = build_d.session, server = build_d.server, error = build_d.session_error](gui::builder& b) {
			draw_game_panel(b, b.ctx.clip_stack.back(), *state, building, slots, extents, live, session, server, error);
		},
	});

	for (const auto& req : requests_in.of<jump_to_request>()) {
		workspace::jump_to(d.ws, {
			.path = req.path,
			.line = req.line,
			.column = req.column,
			.end_line = req.end_line,
			.end_column = req.end_column,
			.highlight = req.highlight,
		});
	}

	for (const auto& req : requests_in.of<apply_lint_request>()) {
		apply_lint_edits(d.ws, req.files);
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
			const id* target = std::get_if<id>(&res.target);
			if (target && res.action_id < table.size() && table[res.action_id].run) {
				table[res.action_id].run(d.ws, *target);
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
	if (!d.initialized) {
		return;
	}
	save_editor_layout(d);
}

auto gse::ide::workspace_system::shutdown(data& d) -> void {
	workspace::save_dirty_documents(d.ws);
	if (!d.initialized) {
		return;
	}
	save_workspace_layout(d.ws);
}
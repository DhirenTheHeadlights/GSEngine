module gse.runtime:engine_impl;

import gse.assert;
import gse.assets;
import gse.audio;
import gse.concurrency;
import gse.config;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.fs;
import gse.gpu;
import gse.gpu_record;
import gse.graphics;
import gse.introspection;
import gse.log;
import gse.meta;
import gse.network;
import gse.os;
import gse.physics;
import gse.save;
import gse.system_manifest;
import gse.time;
import gse.win32;
import std;

import :engine;
import :log_settings;
import :profile_settings;
import :world_system;

gse::engine::engine(const engine_config& config)
	: identifiable(config.title), m_config(config) {
}

auto gse::engine::add_system_node(system_node node) -> void {
	m_scheduler.add_system_node(std::move(node));
}

auto gse::engine::snapshot_graph() const -> introspection::system_graph {
	return m_scheduler.snapshot_graph();
}

auto gse::engine::all_settled() const -> bool {
	return m_scheduler.all_settled();
}

auto gse::engine::create_attached_surface(gpu::context::data& gpu_state, const vec2u extent) -> void {
	close_attached_handles();
	auto& graph = *gpu_state.render_graph;
	const auto created = graph.set_offscreen_output({ .extent = extent, .slots = attached_ring_size, .exportable = true });
	assert(created.has_value(), "attached: offscreen ring creation failed: {}", created ? std::string() : created.error());

	const auto timelines = graph.offscreen_timelines();
	const auto produced_semaphore_handle = gpu_state.device->export_semaphore_handle(timelines.produced);
	const auto consumed_semaphore_handle = gpu_state.device->export_semaphore_handle(timelines.consumed);
	assert(produced_semaphore_handle.has_value(), "attached: produced semaphore export failed: {}", produced_semaphore_handle ? std::string() : produced_semaphore_handle.error());
	assert(consumed_semaphore_handle.has_value(), "attached: consumed semaphore export failed: {}", consumed_semaphore_handle ? std::string() : consumed_semaphore_handle.error());

	const auto surfaces = graph.offscreen_surfaces();
	m_attached_message = {
		.magic = attached_surface_magic,
		.revision = ++m_attached_revision,
		.extent = extent,
		.format = gpu_state.device->surface_format(),
		.backend = gpu::active_backend,
		.produced_semaphore_handle = *produced_semaphore_handle,
		.consumed_semaphore_handle = *consumed_semaphore_handle,
	};
	for (std::size_t i = 0; i < attached_ring_size; ++i) {
		m_attached_message.surface_handles[i] = surfaces[i].handle;
	}
	m_attached_surface_ready = true;
	log::println(log::category::vulkan, "attached: created {}-surface ring at {}x{} on {} (revision {})", attached_ring_size, extent.x(), extent.y(), gpu::active_backend, m_attached_revision);
}

auto gse::engine::close_attached_handles() -> void {
	if (win32::valid_handle(m_attached_message.produced_semaphore_handle)) {
		win32::CloseHandle(m_attached_message.produced_semaphore_handle);
	}
	if (win32::valid_handle(m_attached_message.consumed_semaphore_handle)) {
		win32::CloseHandle(m_attached_message.consumed_semaphore_handle);
	}
	m_attached_message = {};
	m_attached_surface_ready = false;
}

auto gse::engine::install_actions() -> void {
	if (m_actions_revision == m_actions.revision()) {
		return;
	}

	auto* actions_state = m_scheduler.try_state_of<actions::data>();
	if (!actions_state) {
		return;
	}

	m_actions_revision = m_actions.revision();
	actions::adopt_declarations(*actions_state, m_actions);
	m_save.add(actions::settings_record(*actions_state, &settings::draw_controls_page));
}

auto gse::engine::initialize(const setup_fn& app_setup) -> void {
	config::warm_up();

	trace::start();

	m_scheduler.set_registry(m_registry);

	m_save.set_paths({
		.user = config::user_config_dir() / std::format("{}.ini", config::executable_stem()),
		.project = m_config.project_settings_path,
	});
	log::println(
		log::category::save_system,
		"Settings paths: user={} project={}",
		m_save.user_path().generic_display_string(),
		m_save.project_path().empty() ? std::string("<none>") : m_save.project_path().generic_display_string()
	);
	if (m_config.load_settings) {
		m_save.load();
	}
	m_save.set_auto_save(m_config.persist_settings);

	m_save.pin<window::data, "title">(id().tag());
	m_save.pin<gpu::context::data, "dark_background">(m_config.dark_background);
	m_save.pin<gpu::context::data, "device_settings.video_encode">(m_config.video_encode);
	m_save.pin<gui::data, "scale_with_resolution">(m_config.scale_ui_with_resolution);
	m_save.pin<gui::data, "reserve_top_bar">(m_config.custom_chrome);

	if (m_config.custom_chrome) {
		m_save.pin<window::data, "native_frame">(true);
		m_save.pin<window::data, "mouse_visible">(true);
	}
	if (m_config.attached) {
		m_save.pin<window::data, "cursor_suppressed">(true);
		m_save.pin<window::data, "attached">(true);
	}
	if (!m_config.gui_layout_path.empty()) {
		m_save.pin<gui::data, "file_path">(m_config.gui_layout_path);
	}
	if (m_config.use_gpu_solver) {
		m_save.pin<physics::data, "use_gpu_solver">(true);
	}
	if (windowless_render()) {
		m_save.pin<gpu::context::data, "offscreen">(true);
	}

	m_save.set_overrides(m_config.setting);
	m_save.set_on_restart([this] {
		app::relaunch_self_on_exit();
		m_scheduler.make_channel_writer().push(window_close_request{});
	});
	m_scheduler.set_settings_register_hook([this](settings::register_settings_type entry) {
		m_save.add(std::move(entry));
	});
	m_scheduler.set_actions_register_hook([this](std::vector<actions::registration> entries, std::vector<actions::axis_registration> axes) {
		m_actions.add(std::move(entries), std::move(axes));
	});
	m_scheduler.register_external_resource<save::registry>(&m_save);
	m_scheduler.register_external_resource<primitives::data>(&m_primitives);
	m_scheduler.register_external_resource<engine_config>(&m_config);
	m_scheduler.register_external_resource<network::config>(&m_config.net);
	m_scheduler.register_external_resource<scheduler>(&m_scheduler);
	network::endpoint::set_simulation(m_config.net.simulated_latency_ms, m_config.net.simulated_loss_permille);
	m_scheduler.set_stall_probe([this] {
		const auto late = m_scheduler.drain_channel<gpu::render_pass_request>();
		if (late.empty()) {
			return;
		}
		std::string names;
		for (const auto& req : late) {
			names += std::format("{}#{} ", req.desc.pass_name, req.desc.chain_index);
		}
		log::println(
			log::level::error,
			log::category::runtime,
			"{} render pass request(s) were pushed after this frame's record round closed and can never record: {}",
			late.size(),
			names
		);
	});

	m_scheduler.begin_staging();
	system_manifest<^^save::override_system::run>{}.register_with(*this);
	register_systems<^^input>(*this);
	register_systems<^^actions>(*this);
	system_manifest<^^log_settings::data, ^^log_settings::run>{}.register_with(*this);
	system_manifest<^^profile_settings::data, ^^profile_settings::run>{}.register_with(*this);
	system_manifest<^^world_system::data, ^^world_system::init, ^^world_system::run, ^^world_system::shutdown>{}.register_with(*this);
	register_systems<^^window>(*this);
	register_systems<^^gpu::context>(*this);
	register_systems<^^asset>(*this);
	register_systems<^^animation>(*this);
	register_systems<^^physics>(*this);
	register_systems<^^camera>(*this);
	register_systems<^^audio>(*this);
	if (m_config.render) {
		register_systems<^^renderer>(*this);
		register_systems<^^primitive_resolver>(*this);
		register_systems<^^gui>(*this);
	}

	std::unordered_set<gse::id> disabled;
	if (!m_config.render || windowless_render()) {
		disabled.insert(id_of<window::data>());
		disabled.insert(id_of<audio::data>());
		system_clock::set_display_snapping(false);
	}
	if (!m_config.gpu) {
		disabled.insert(id_of<gpu::context::data>());
		log::println(log::category::runtime, "gpu disabled by config: no device will be created, graphics assets load CPU-side only, and every system that requires a device is dropped");
	}
	if (!m_config.simulate_world) {
		disabled.insert(id_of<world_system::data>());
		disabled.insert(id_of<physics::data>());
		disabled.insert(id_of<camera::data>());
		disabled.insert(id_of<audio::data>());
		disabled.insert(id_of<renderer::scene_snapshot::data>());
	}
	m_scheduler.resolve_activation(disabled);

	auto& asset_state = m_scheduler.state<asset::data>();

	using game_assets = assets::append<graphics::asset_types, audio::asset_types>;

	if (m_config.render) {
		if (auto* window_state = m_scheduler.try_state_of<window::data>()) {
			window_state->launch_launcher_size = m_config.launcher_size;
		}
		tick_window();

		asset::system_for<game_assets> assets{ asset_state };
		assets.register_loaders();
		assets.install_recompile_fns();
		if (auto discovered = assets.discover_baked(); !discovered) {
			assert(false, "Asset discovery failed: {}", discovered.error().detail);
		}
		primitives::initialize(m_primitives, asset_state);
		assets.install_hot_reload_fns();

		assets.verify_built_ins();

		if (windowless_render()) {
			m_scheduler.register_deferred();
			if (const auto* shadow = m_scheduler.try_state_of<physics::shadow_step::data>()) {
				m_headless_gpu = shadow->enabled;
			}
			boot_immediately(app_setup);
			return;
		}

		log::println(log::category::runtime, "boot: scheduler.initialize begin");
		m_scheduler.initialize();
		m_scheduler.enter_running();
		log::println(log::category::runtime, "boot: scheduler.initialize end");

		m_loading.set_phase("Initializing");
		m_loading.set_progress(0, 0);

		auto& gui_data = m_scheduler.state<gui::data>();
		gui_data.primary.menu_stack.push<gui::loading_screen>(m_loading);
		log::println(log::category::runtime, "boot: loading_screen pushed to menu stack");

		m_deferred_boot = [this, app_setup] {
			log::println(log::category::runtime, "boot: deferred boot begin (loading screen rendered)");

			m_scheduler.register_deferred();

			if (const auto* shadow = m_scheduler.try_state_of<physics::shadow_step::data>()) {
				m_headless_gpu = shadow->enabled;
			}

			task::post_io([this, app_setup] {
				if (app_setup) {
					log::println(log::category::runtime, "boot: app_setup begin");
					app_setup(
						*this
					);
					log::println(log::category::runtime, "boot: app_setup end");
				}

				m_boot_tasks_done.store(true, std::memory_order_release);
				log::println(log::category::runtime, "boot: task::post complete, waiting for systems to settle");
			});
		};
	}
	else {
		m_scheduler.register_deferred();

		if (const auto* shadow = m_scheduler.try_state_of<physics::shadow_step::data>()) {
			m_headless_gpu = shadow->enabled;
		}

		asset::system_for<game_assets> assets{ asset_state };
		assets.register_loaders();
		if (m_config.author_baked_assets) {
			assets.install_recompile_fns();
		}
		else {
			assets.install_stale_checks();
		}
		if (auto discovered = assets.discover_baked(); !discovered) {
			assert(false, "Asset discovery failed: {}", discovered.error().detail);
		}

		boot_immediately(app_setup);
	}
}

auto gse::engine::boot_immediately(const setup_fn& app_setup) -> void {
	if (app_setup) {
		app_setup(
			*this
		);
	}

	install_actions();

	m_scheduler.initialize();
	m_scheduler.enter_running();
	m_boot_tasks_done.store(true, std::memory_order_release);
	m_loading.mark_finished();
}

auto gse::engine::windowless_render() const -> bool {
	return m_config.render && !m_config.create_window;
}

auto gse::engine::update() -> void {
	system_clock::update();

	if (m_config.attached && m_attached_requested_extent.x() > 0 && m_attached_requested_extent.y() > 0) {
		m_scheduler.make_channel_writer().push<window_resize_request>({
			.size = { static_cast<int>(m_attached_requested_extent.x()), static_cast<int>(m_attached_requested_extent.y()) },
		});
		m_attached_requested_extent = {};
	}

	m_scheduler.update();

	if (const time persist_interval = seconds(1.f); m_persist_clock.elapsed() > persist_interval) {
		m_scheduler.persist();
		m_save.save_now();
		m_persist_clock.reset();
	}

	const auto* physics_state = m_scheduler.try_state_of<physics::data>();
	const bool gpu_solver = physics_state && physics_state->use_gpu_solver;
	if (!m_config.render && (gpu_solver || m_headless_gpu)) {
		if (auto* gpu_state = m_scheduler.try_state_of<gpu::context::data>()) {
			if (gpu::context::begin_frame(*gpu_state, nullptr)) {
				m_scheduler.render(
					true,
					[this, gpu_state] {
						gpu_state->scheduler.flush();
						gpu::context::execute_frame(*gpu_state, m_scheduler);
					}
				);
				gpu::context::end_frame(*gpu_state);
			}
		}
	}

	if (m_deferred_boot && m_loading.rendered_once()) {
		auto deferred = std::move(m_deferred_boot);
		m_deferred_boot = {};
		deferred();
	}

	if (!m_loading.finished() && m_loading.rendered_once() && !m_deferred_boot) {
		const auto stats = m_scheduler.settle_progress();
		if (!m_boot_init_baseline_captured) {
			m_boot_init_baseline_settled = stats.settled;
			m_boot_init_baseline_captured = true;
			log::println(
				log::category::runtime,
				"boot: init baseline captured settled={} total={}",
				stats.settled,
				stats.total
			);
		}
		if (stats.total > m_boot_init_baseline_settled) {
			const std::uint32_t done = stats.settled >= m_boot_init_baseline_settled
				? stats.settled - m_boot_init_baseline_settled
				: 0;
			const std::uint32_t total = stats.total - m_boot_init_baseline_settled;
			m_loading.set_progress(done, total);
		}
	}

	install_actions();

	if (!m_actions_logged && m_boot_tasks_done.load(std::memory_order_acquire) && m_scheduler.all_settled()) {
		m_actions_logged = true;
		if (auto* actions_state = m_scheduler.try_state_of<actions::data>()) {
			actions::log_declared_bindings(*actions_state);
		}
	}

	if (!m_settings_audited && m_boot_tasks_done.load(std::memory_order_acquire) && m_scheduler.all_settled()) {
		m_settings_audited = true;
		m_save.audit_overrides();
		if (m_config.render && m_config.simulate_world) {
			m_save.audit_files();
		}
		if (m_config.dump_settings) {
			std::cout << m_save.dump();
		}
	}

	if (!m_loading.finished() && m_boot_tasks_done.load(std::memory_order_acquire) && m_scheduler.all_settled() && m_loading.rendered_once()) {
		m_loading.mark_finished();
		log::println(log::category::runtime, "boot: loading.mark_finished (all settled + rendered)");
	}
}

auto gse::engine::render() -> void {
	bool frame_ok = false;
	auto* gpu_state = m_scheduler.try_state_of<gpu::context::data>();
	auto* asset_state = m_scheduler.try_state_of<asset::data>();

	if (m_config.attached && gpu_state && gpu_state->device && gpu_state->render_graph && gpu_state->swapchain) {
		if (const auto ext = gpu_state->swapchain->extent(); ext.x() > 0 && ext.y() > 0) {
			if (!m_attached_surface_attempted) {
				m_attached_surface_attempted = true;
				create_attached_surface(*gpu_state, ext);
			}
			else if (m_attached_surface_ready && ext != m_attached_message.extent) {
				create_attached_surface(*gpu_state, ext);
			}
		}
	}

	bool attached_slot_starved = false;

	if (m_attached_surface_ready && gpu_state && gpu_state->render_graph) {
		attached_slot_starved = !gpu_state->render_graph->output_ready();
		if (attached_slot_starved) {
			++m_attached_frames_skipped;
		}
		else {
			++m_attached_frames_presented;
		}
	}

	if (m_attached_report.tick() && m_attached_frames_presented + m_attached_frames_skipped > 0) {
		log::println(
			log::category::runtime,
			"attached surface: {} frames handed to the editor, {} skipped waiting on a free slot in the last window",
			m_attached_frames_presented,
			m_attached_frames_skipped
		);
		m_attached_frames_presented = 0;
		m_attached_frames_skipped = 0;
	}

	if (gpu_state && !attached_slot_starved) {
		auto* window_state = m_scheduler.try_state_of<window::data>();
		const clock fence_timer;
		std::expected<gpu::frame_token, gpu::frame_status> result;
		{
			trace::scope_guard _{ trace_id<"render::begin_frame">() };
			if (window_state) {
				gpu::context::sync_present_targets(*gpu_state, *window_state);
			}
			result = gpu::context::begin_frame(*gpu_state, window_state ? &window_state->primary : nullptr);
		}
		const auto fence_wait = fence_timer.elapsed();

		// Convert gse::time -> raw seconds at the boundary; the scheduler keeps
		// quantity off its module surface (GCC PR c++/122785 workaround).
		gpu_state->scheduler.report_frame_time(fence_wait.as<seconds>());
		frame_ok = result.has_value();

		if (!result && result.error() == gpu::frame_status::device_lost) {
			log::println(
				log::level::error,
				log::category::vulkan,
				"Device lost during begin_frame Ã¢â‚¬â€ terminating"
			);
			fatal_exit(3);
		}

	}

	m_scheduler.render(
		frame_ok,
		[this, gpu_state] {
			if (gpu_state) {
				gpu_state->scheduler.flush();
				{
					trace::scope_guard _{ trace_id<"render::graph_execute">() };
					gpu::context::execute_frame(*gpu_state, m_scheduler);
				}
			}
		}
	);

	if (frame_ok && gpu_state) {
		{
			trace::scope_guard _{ trace_id<"render::end_frame">() };
			gpu::context::end_frame(*gpu_state);
			if (asset_state) {
				trace::scope_guard _{ trace_id<"end_frame::finalize_reloads">() };
				for (const auto& l : std::views::values(asset_state->resource_loaders)) {
					l->finalize_reloads();
				}
			}
			if (auto* window_state = m_scheduler.try_state_of<window::data>()) {
				reveal_window(*window_state);
			}
		}
	}
}

auto gse::engine::reveal_window(window::data& window_state) -> void {
	const bool attach_abandoned = m_attached_surface_attempted && !m_attached_surface_ready;
	if (attach_abandoned && window_state.attached) {
		window_state.attached = false;
		window_state.cursor_suppressed = false;
		window_state.primary.framebuffer_resized = true;
	}
	if (!m_window_shown && m_config.bench.enabled) {
		m_window_shown = true;
		log::println(log::category::runtime, "boot: window kept hidden (bench run)");
	}
	else if (!m_window_shown && (!m_config.attached || attach_abandoned)) {
		if (m_loading.finished()) {
			window::show(window_state);
			m_window_shown = true;
			log::println(log::category::runtime, "boot: window shown (loading finished)");
		}
		else if (m_loading.rendered_once()) {
			++m_frames_since_rendered;
			if (m_frames_since_rendered >= 2) {
				window::show(window_state);
				m_window_shown = true;
				log::println(log::category::runtime, "boot: window shown (loading screen on swapchain)");
			}
		}
	}
}

auto gse::engine::attached_surface_ready() const -> bool {
	return m_attached_surface_ready;
}

auto gse::engine::abandon_attach() -> void {
	m_attached_surface_attempted = true;
}

auto gse::engine::attached_message() const -> const attached_surface_message& {
	return m_attached_message;
}

auto gse::engine::push_attached_input(const input::event& event) -> void {
	auto* window_state = m_scheduler.try_state_of<window::data>();
	if (!window_state) {
		return;
	}
	if (window_state->primary.ui_focus) {
		const auto dims = window::viewport(*window_state);
		auto to_surface = [dims](const double x, const double y) {
			return std::pair{
				std::clamp(x, 0.0, static_cast<double>(dims.x())),
				static_cast<double>(dims.y()) - std::clamp(y, 0.0, static_cast<double>(dims.y())),
			};
		};

		if (const auto* moved = std::get_if<input::mouse_moved>(&event)) {
			const auto [x, y] = to_surface(moved->x_pos, moved->y_pos);
			window_state->primary.input_events.push(input::mouse_moved{ .x_pos = x, .y_pos = y });
			return;
		}
		if (const auto* pressed = std::get_if<input::mouse_button_pressed>(&event)) {
			const auto [x, y] = to_surface(pressed->x_pos, pressed->y_pos);
			window_state->primary.input_events.push(input::mouse_button_pressed{ .button = pressed->button, .x_pos = x, .y_pos = y });
			return;
		}
		if (const auto* released = std::get_if<input::mouse_button_released>(&event)) {
			const auto [x, y] = to_surface(released->x_pos, released->y_pos);
			window_state->primary.input_events.push(input::mouse_button_released{ .button = released->button, .x_pos = x, .y_pos = y });
			return;
		}
	}
	window_state->primary.input_events.push(event);
}

auto gse::engine::push_attached_resize(const vec2u extent) -> void {
	m_attached_requested_extent = extent;
}

auto gse::engine::shutdown() -> void {
	const time phase_budget = seconds(10.f);

	{
		watchdog::section _{ trace_id<"shutdown::profile_dump">(), phase_budget };
		profile::dump();
		profile::dump_chrome_trace();
		profile::dump_report();
	}

	{
		watchdog::section _{ trace_id<"shutdown::save">(), phase_budget };
		m_save.save_now();
		m_save.set_auto_save(false);
	}

	if (auto* gpu_state = m_scheduler.try_state_of<gpu::context::data>()) {
		watchdog::section _{ trace_id<"shutdown::gpu_wait_idle">(), phase_budget };
		gpu::context::wait_idle(*gpu_state);
		close_attached_handles();
	}

	{
		watchdog::section _{ trace_id<"shutdown::systems">(), phase_budget };
		m_scheduler.enter_shutdown();
		m_scheduler.shutdown();
	}

	{
		watchdog::section _{ trace_id<"shutdown::layout_flush">(), phase_budget };
		layout_store::flush();
	}

	{
		watchdog::section _{ trace_id<"shutdown::scheduler_clear">(), phase_budget };
		m_scheduler.clear();
	}
}

auto gse::engine::make_channel_writer() -> channel_writer {
	return m_scheduler.make_channel_writer();
}

auto gse::engine::registry() -> gse::registry& {
	return m_registry;
}

auto gse::engine::world() -> world_system::data& {
	return m_scheduler.state<world_system::data>();
}

auto gse::engine::window_should_close() -> bool {
	auto* window_state = m_scheduler.try_state_of<window::data>();
	return window_state && !window::is_open(*window_state);
}

auto gse::engine::tick_window() -> void {
	if (auto* window_state = m_scheduler.try_state_of<window::data>()) {
		window::tick(m_scheduler, *window_state);
	}
}
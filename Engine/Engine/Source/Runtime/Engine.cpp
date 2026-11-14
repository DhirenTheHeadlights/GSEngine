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
import std;

import :engine;
import :log_settings;
import :profile_settings;
import :restart_policy;
import :settings_setup;
import :world_system;

namespace gse {
	using game_assets = assets::append<graphics::asset_types, audio::asset_types>;
}

auto gse::bake_game_assets() -> int {
	config::warm_up();
	const auto baked = asset::system_for<game_assets>::bake_all();
	if (!baked) {
		log::println(log::level::error, log::category::assets, "bake failed: {}: {}", baked.error().path.generic_display_string(), baked.error().detail);
		log::flush();
		return 1;
	}
	log::println(log::category::assets, "bake: every catalog asset is current under {}", config::baked_resource_path().generic_display_string());
	log::flush();
	return 0;
}

gse::engine::engine(const engine_config& config)
	: identifiable(config.title), m_config(config), m_simulate_device_lost(device_lost_simulation_requested()) {
	install_restart_policy(m_config.restart_on_crash);
}

auto gse::engine::handle_device_lost(const std::string_view cause) -> void {
	if (!m_config.restart_on_device_lost) {
		log::println(log::level::error, log::category::render, "Device lost during {} - terminating", cause);
		log::flush();
		fatal_exit(3);
	}

	if (app::relaunch_pending()) {
		return;
	}

	if (!arm_restart(std::format("Device lost during {}", cause))) {
		fatal_exit(3);
	}
	m_scheduler.make_channel_writer().push(window_close_request{});
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

auto gse::engine::frame_presented() const -> bool {
	return m_frame_presented;
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

	configure_settings(m_save, m_config, id().tag());
	m_save.set_on_restart([this] {
		app::relaunch_self_on_exit();
		m_scheduler.make_channel_writer().push(window_close_request{});
	});
	m_scheduler.set_settings_register_hook([this](settings::register_settings_type entry) {
		m_save.add(std::move(entry));
	});
	m_scheduler.set_settings_retire_hook([this](const void* settings_ptr) {
		m_save.retire(settings_ptr);
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
	m_scheduler.set_init_stall_dump(m_config.init_stall_dump);
	m_scheduler.set_stall_probe([this] {
		gpu::report_late_pass_requests(m_scheduler);
	});

	system_manifest<^^launch::data>{}.register_with(*this);
	m_scheduler.state<launch::data>() = {
		.presents = m_config.render && m_config.create_window,
		.render = m_config.render,
		.gpu = m_config.gpu,
		.simulate_world = m_config.simulate_world,
	};
	register_systems<^^window>(*this);
	system_manifest<^^gpu::offscreen::data>{}.register_with(*this);
	register_systems<^^gpu::context>(*this);

	if (auto* gpu_state = m_scheduler.try_state_of<gpu::context::data>()) {
		std::optional<shared_view<window::data>> window_view;
		if (auto* window_state = m_scheduler.try_state_of<window::data>()) {
			tick_window();
			window_view.emplace(make_shared_view_live<window::data>(*window_state));
		}
		gpu::context::create_device(*gpu_state, window_view, &m_save);
		if (m_config.attached) {
			gpu::context::request_shared_output(*gpu_state);
		}
	}
	system_manifest<^^lighting::data>{}.register_with(*this);

	m_scheduler.begin_staging();
	system_manifest<^^save::override_system::run>{}.register_with(*this);
	register_systems<^^input>(*this);
	register_systems<^^actions>(*this);
	system_manifest<^^log_settings::data, ^^log_settings::run>{}.register_with(*this);
	system_manifest<^^profile_settings::data, ^^profile_settings::run>{}.register_with(*this);
	system_manifest<^^world_system::data, ^^world_system::init, ^^world_system::run, ^^world_system::shutdown>{}.register_with(*this);

	log::println(log::category::render, "lighting technique: {}", m_scheduler.state<lighting::data>().lighting_technique);

	register_systems<^^asset>(*this);
	register_systems<^^animation>(*this);
	register_systems<^^physics>(*this);
	register_systems<^^camera>(*this);
	register_systems<^^audio>(*this);
	register_systems<^^renderer>(*this);
	register_systems<^^primitive_resolver>(*this);
	register_systems<^^gui>(*this);

	m_scheduler.resolve_activation();

	tick_window();

	auto& asset_state = m_scheduler.state<asset::data>();
	asset::system_for<game_assets> assets{ asset_state };
	assets.register_loaders();
	if (m_config.render || m_config.author_baked_assets) {
		assets.install_recompile_fns();
	}
	else {
		assets.install_stale_checks();
	}
	if (auto discovered = assets.discover_baked(); !discovered) {
		assert(false, "Asset discovery failed: {}", discovered.error().detail);
	}
	if (m_scheduler.try_state_of<primitive_resolver::data>()) {
		primitives::initialize(m_primitives, asset_state);
	}
	assets.install_hot_reload_fns();
	assets.verify_built_ins();

	if (!presents()) {
		m_scheduler.register_deferred();
		boot_immediately(app_setup);
		return;
	}

	m_boot.enter(loading::boot_stage::initializing);
	m_scheduler.initialize();
	m_scheduler.enter_running();
	m_boot.enter(loading::boot_stage::presenting);

	if (auto* gui_data = m_scheduler.try_state_of<gui::data>(); gui_data && gui_data->visible) {
		m_loading.awaiting_first_draw = true;
		gui_data->primary.menu_stack.push<gui::loading_screen>(m_boot, m_loading);
	}
	else {
		log::println(log::category::runtime, "loading: no visible gui, app setup starts without a loading screen");
	}

	m_app_setup = app_setup;
}

auto gse::engine::boot_immediately(const setup_fn& app_setup) -> void {
	if (app_setup) {
		app_setup(
			*this
		);
	}

	install_actions();

	m_boot.enter(loading::boot_stage::initializing);
	m_scheduler.initialize();
	m_scheduler.enter_running();
	m_boot.enter(loading::boot_stage::settling);
}

auto gse::engine::begin_app_setup() -> void {
	m_boot.enter(loading::boot_stage::app_setup);

	m_scheduler.register_deferred();

	m_app_setup_job.start(
		[this, setup = std::move(m_app_setup)] {
			if (setup) {
				setup(*this);
			}
			return std::monostate{};
		},
		trace_id<"boot::app_setup">(),
		task::lane::io
	);

	const auto stats = m_scheduler.settle_progress();
	m_progress_baseline = stats.settled;
	log::println(log::category::runtime, "loading: progress baseline settled={} total={}", stats.settled, stats.total);
}

auto gse::engine::update() -> void {
	system_clock::update();

	if (m_attached_requested_extent.x() > 0 && m_attached_requested_extent.y() > 0) {
		m_scheduler.make_channel_writer().push<window_resize_request>({
			.size = { static_cast<int>(m_attached_requested_extent.x()), static_cast<int>(m_attached_requested_extent.y()) },
		});
		m_attached_requested_extent = {};
	}

	m_scheduler.update();

	if (m_persist_timer.tick()) {
		m_scheduler.persist();
		m_save.save_now();
	}

	if (m_boot.state() == loading::boot_stage::presenting && !m_loading.awaiting_first_draw) {
		begin_app_setup();
	}

	if (m_progress_baseline && m_boot.state() != loading::boot_stage::ready) {
		const auto stats = m_scheduler.settle_progress();
		if (stats.total > *m_progress_baseline) {
			const std::uint32_t done = stats.settled >= *m_progress_baseline
				? stats.settled - *m_progress_baseline
				: 0;
			const std::uint32_t total = stats.total - *m_progress_baseline;
			m_loading.set_progress(done, total);
		}
	}

	install_actions();

	if (m_boot.state() == loading::boot_stage::app_setup && m_app_setup_job.take()) {
		m_boot.enter(loading::boot_stage::settling);
	}

	if (m_boot.state() == loading::boot_stage::settling && m_scheduler.all_settled()) {
		if (auto* actions_state = m_scheduler.try_state_of<actions::data>()) {
			actions::log_declared_bindings(*actions_state);
		}

		m_save.audit_overrides();
		m_save.audit_files();
		if (m_config.dump_settings) {
			std::cout << m_save.dump();
		}

		m_boot.enter(loading::boot_stage::ready);
	}
}

auto gse::engine::apply_pending_resizes() -> void {
	auto* gpu_state = m_scheduler.try_state_of<gpu::context::data>();
	if (!gpu_state || !gpu_state->frame) {
		return;
	}
	gpu::context::apply_pending_resizes(*gpu_state);
}

auto gse::engine::render() -> void {
	auto* gpu_state = m_scheduler.try_state_of<gpu::context::data>();
	if (!gpu_state || (!presents() && !m_scheduler.has_frame_work())) {
		m_frame_presented = false;
		return;
	}

	auto* asset_state = m_scheduler.try_state_of<asset::data>();

	if (m_config.bench.enabled && m_config.bench.pacing == scenario::pacing::lockstep && m_scheduler.all_settled()) {
		gpu::context::await_shared_output_slot(*gpu_state);
	}

	auto* window_state = m_scheduler.try_state_of<window::data>();
	std::expected<gpu::frame_token, gpu::frame_status> result;
	{
		trace::scope_guard _{ trace_id<"render::begin_frame">() };
		if (window_state) {
			gpu::context::sync_present_targets(*gpu_state, *window_state);
		}
		result = gpu::context::begin_frame(*gpu_state, window_state ? &window_state->primary : nullptr);
	}
	const bool frame_ok = result.has_value();

	if (m_simulate_device_lost) {
		handle_device_lost("--engine-simulate-device-lost");
	}
	else if (!result && result.error() == gpu::frame_status::device_lost) {
		handle_device_lost("begin_frame");
	}

	m_frame_presented = frame_ok;

	m_scheduler.render(
		frame_ok,
		[this, gpu_state] {
			trace::scope_guard _{ trace_id<"render::graph_execute">() };
			gpu::context::execute_frame(*gpu_state, m_scheduler);
		}
	);

	if (frame_ok) {
		{
			trace::scope_guard _{ trace_id<"render::end_frame">() };
			gpu::context::end_frame(*gpu_state);
			if (asset_state) {
				trace::scope_guard _{ trace_id<"end_frame::finalize_reloads">() };
				asset::finalize_reloads(*asset_state);
			}
			if (window_state) {
				reveal_window(*window_state, *gpu_state);
			}
		}
	}
}

auto gse::engine::reveal_window(window::data& window_state, const gpu::context::data& gpu_state) -> void {
	const bool attach_abandoned = gpu_state.shared_output.stage.state() == gpu::shared_output_stage::abandoned;
	if (attach_abandoned && window_state.attached) {
		window_state.attached = false;
		window_state.cursor_suppressed = false;
		window_state.primary.framebuffer_resized = true;
	}

	const window_reveal_stage reveal = m_window_reveal.state();
	if (reveal == window_reveal_stage::shown || reveal == window_reveal_stage::hidden_for_bench) {
		return;
	}
	if (m_config.bench.enabled) {
		m_window_reveal.enter(window_reveal_stage::hidden_for_bench);
		return;
	}
	if (m_config.attached && !attach_abandoned) {
		return;
	}

	const loading::boot_stage boot = m_boot.state();
	const bool loading_on_screen = boot == loading::boot_stage::app_setup || boot == loading::boot_stage::settling;
	if (boot == loading::boot_stage::ready || (loading_on_screen && reveal == window_reveal_stage::loading_frame_presented)) {
		window::show(window_state);
		m_window_reveal.enter(window_reveal_stage::shown);
	}
	else if (loading_on_screen) {
		m_window_reveal.enter(window_reveal_stage::loading_frame_presented);
	}
}

auto gse::engine::abandon_attach() -> void {
	if (auto* gpu_state = m_scheduler.try_state_of<gpu::context::data>()) {
		gpu::context::abandon_shared_output(*gpu_state);
	}
}

auto gse::engine::shared_output() -> std::optional<gpu::shared_output_announcement> {
	auto* gpu_state = m_scheduler.try_state_of<gpu::context::data>();
	if (!gpu_state) {
		return std::nullopt;
	}
	return gpu::context::shared_output(*gpu_state);
}

auto gse::engine::push_attached_input(const input::event& event) -> void {
	auto* window_state = m_scheduler.try_state_of<window::data>();
	if (!window_state) {
		return;
	}
	if (window_state->primary.ui_focus) {
		const auto dims = window::viewport(window_state->primary.handle);
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

auto gse::engine::flip_channels() -> void {
	m_scheduler.flip_channels();
}

auto gse::engine::registry() -> gse::registry& {
	return m_registry;
}

auto gse::engine::world() -> world_system::data& {
	return m_scheduler.state<world_system::data>();
}

auto gse::engine::presents() -> bool {
	return m_scheduler.try_state_of<window::data>() != nullptr;
}

auto gse::engine::window_should_close() -> bool {
	auto* window_state = m_scheduler.try_state_of<window::data>();
	return window_state && window_state->primary.should_close;
}

auto gse::engine::tick_window() -> void {
	if (auto* window_state = m_scheduler.try_state_of<window::data>()) {
		window::tick(m_scheduler, *window_state);
	}
}
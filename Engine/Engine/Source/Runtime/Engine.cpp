module gse.runtime:engine_impl;

import std;

import :engine;
import :log_settings;
import :scene;
import :world_system;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.meta;
import gse.ecs;
import gse.introspection;
import gse.system_manifest;
import gse.network;
import gse.graphics;
import gse.audio;
import gse.physics;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.gpu_record;
import gse.log;
import gse.save;
import gse.config;
import gse.fs;
import gse.win32;

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

auto gse::engine::destroy_attached_surface(gpu::device& device) -> void {
	for (std::size_t i = 0; i < attached_ring_size; ++i) {
		m_attached_surface_images[i] = {};
		if (m_attached_surfaces[i].image) {
			device.destroy_shared_surface(m_attached_surfaces[i]);
			m_attached_surfaces[i] = {};
		}
	}
	if (m_attached_produced_semaphore) {
		device.retire(m_attached_produced_semaphore);
		m_attached_produced_semaphore = {};
	}
	if (m_attached_consumed_semaphore) {
		device.retire(m_attached_consumed_semaphore);
		m_attached_consumed_semaphore = {};
	}
	if (win32::valid_handle(m_attached_message.produced_semaphore_handle)) {
		win32::CloseHandle(m_attached_message.produced_semaphore_handle);
	}
	if (win32::valid_handle(m_attached_message.consumed_semaphore_handle)) {
		win32::CloseHandle(m_attached_message.consumed_semaphore_handle);
	}
	m_attached_message = {};
	m_attached_surface_ready = false;
}

auto gse::engine::initialize(const setup_fn& app_setup) -> void {
	config::warm_up();

	trace::start();

	m_scheduler.set_registry(m_registry);

	m_save.set_project_path(m_config.project_settings_path);
	if (m_config.persist_settings) {
		m_save.set_auto_save(true, config::user_config_dir() / std::format("{}.ini", config::executable_stem()));
	}
	m_save.set_on_restart([] {
		app::restart();
	});
	m_scheduler.set_settings_register_hook([this](settings::register_settings_type entry) {
		m_save.add(std::move(entry));
	});
	m_scheduler.register_external_resource<save::registry>(&m_save);
	m_scheduler.register_external_resource<primitives::data>(&m_primitives);
	m_scheduler.register_external_resource<engine_config>(&m_config);
	m_scheduler.register_external_resource<scheduler>(&m_scheduler);

	m_scheduler.begin_staging();
	register_systems<^^input>(*this);
	register_systems<^^actions>(*this);
	system_manifest<^^log_settings::data, ^^log_settings::run>{}.register_with(*this);
	system_manifest<^^world_system::data, ^^world_system::run, ^^world_system::shutdown>{}.register_with(*this);
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
	if (!m_config.render) {
		disabled.insert(id_of<window::data>());
		disabled.insert(id_of<audio::data>());
	}
	if (!m_config.simulate_world) {
		disabled.insert(id_of<world_system::data>());
		disabled.insert(id_of<physics::data>());
		disabled.insert(id_of<camera::data>());
		disabled.insert(id_of<audio::data>());
	}
	m_scheduler.resolve_activation(disabled);

	if (auto* win = m_scheduler.try_state_of<window::data>()) {
		win->title = std::string(id().tag());
		if (m_config.custom_chrome) {
			win->native_frame = true;
			win->mouse_visible = true;
		}
		if (m_config.attached) {
			win->cursor_suppressed = true;
			win->attached = true;
		}
	}
	if (auto* gpu_state = m_scheduler.try_state_of<gpu::context::data>()) {
		gpu_state->swapchain_clear = m_config.render_world ? gpu::color_clear{} : gpu::color_clear{ 0.05f, 0.05f, 0.06f, 1.0f };
	}
	if (auto* renderer_state = m_scheduler.try_state_of<renderer::data>()) {
		renderer_state->render_world = m_config.render_world;
	}
	if (auto* gui_state = m_scheduler.try_state_of<gui::data>()) {
		gui_state->scale_with_resolution = m_config.scale_ui_with_resolution;
		gui_state->reserve_top_bar = m_config.custom_chrome;
		if (!m_config.gui_layout_path.empty()) {
			gui_state->file_path = m_config.gui_layout_path;
		}
	}

	auto& asset_state = m_scheduler.state<asset::data>();

	using game_assets = gse::assets::append<graphics::asset_types, audio::asset_types>;

	if (m_config.render) {
		tick_window();

		gse::asset::system_for<game_assets> assets{ asset_state };
		assets.register_loaders();
		assets.install_recompile_fns();
		if (auto discovered = assets.discover_baked(); !discovered) {
			assert(false, "Asset discovery failed: {}", discovered.error().detail);
		}
		primitives::initialize(m_primitives, asset_state);
		assets.install_hot_reload_fns();

		assets.verify_built_ins();

		log::println(log::category::runtime, "boot: compile_boot_critical begin");
		if (const auto result = assets.compile_boot_critical(); result.success_count > 0 || result.failure_count > 0) {
			log::println(
				result.failure_count > 0 ? log::level::warning : log::level::info,
				log::category::assets,
				"Compiled {} boot assets ({} skipped, {} failed)",
				result.success_count,
				result.skipped_count,
				result.failure_count
			);
		}
		log::println(log::category::runtime, "boot: compile_boot_critical end");

		log::println(log::category::runtime, "boot: scheduler.initialize begin");
		m_scheduler.initialize();
		m_scheduler.enter_running();
		log::println(log::category::runtime, "boot: scheduler.initialize end");

		m_loading.set_phase("Initializing");
		m_loading.set_progress(0, 0);

		auto& gui_data = m_scheduler.state<gse::gui::data>();
		gui_data.menu_stack.push<gse::gui::loading_screen>(m_loading);
		log::println(log::category::runtime, "boot: loading_screen pushed to menu stack");

		m_deferred_boot = [this, app_setup] {
			log::println(log::category::runtime, "boot: deferred boot begin (loading screen rendered)");

			m_scheduler.register_deferred();

			if (m_config.use_gpu_solver) {
				if (auto* phys = m_scheduler.try_state_of<physics::data>()) {
					phys->use_gpu_solver = true;
				}
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

		if (m_config.use_gpu_solver) {
			if (auto* phys = m_scheduler.try_state_of<physics::data>()) {
				phys->use_gpu_solver = true;
			}
		}

		gse::asset::system_for<game_assets> assets{ asset_state };
		assets.register_loaders();
		assets.install_stale_checks();
		if (auto discovered = assets.discover_baked(); !discovered) {
			assert(false, "Asset discovery failed: {}", discovered.error().detail);
		}

		if (app_setup) {
			app_setup(
				*this
			);
		}

		m_scheduler.initialize();
		m_scheduler.enter_running();
		m_loading.mark_finished();
	}
}

auto gse::engine::update() -> void {
	system_clock::update();
	m_scheduler.update();

	if (!m_config.render && m_config.use_gpu_solver) {
		if (auto* gpu_state = m_scheduler.try_state_of<gpu::context::data>()) {
			if (gpu::context::begin_frame(*gpu_state, nullptr)) {
				m_scheduler.render(
					true,
					[this, gpu_state] {
						gpu_state->scheduler.flush();
						gpu::context::execute_frame(*gpu_state, m_scheduler);
					}
				);
				gpu::context::end_frame(*gpu_state, nullptr);
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

	if (!m_loading.finished() && m_boot_tasks_done.load(std::memory_order_acquire) && m_scheduler.all_settled() && m_loading.rendered_once()) {
		m_loading.mark_finished();
		log::println(log::category::runtime, "boot: loading.mark_finished (all settled + rendered)");
	}
}

auto gse::engine::render() -> void {
	bool frame_ok = false;
	auto* gpu_state = m_scheduler.try_state_of<gpu::context::data>();
	auto* asset_state = m_scheduler.try_state_of<asset::data>();

	if (m_config.attached && !m_attached_surface_attempted && gpu_state && gpu_state->device && gpu_state->render_graph && gpu_state->swapchain) {
		if (const auto ext = gpu_state->render_graph->extent(); ext.x() > 0 && ext.y() > 0) {
			m_attached_surface_attempted = true;
			m_attached_produced_semaphore = gpu_state->device->create_exportable_semaphore();
			m_attached_consumed_semaphore = gpu_state->device->create_exportable_semaphore();
			const auto produced_semaphore_handle = gpu_state->device->export_semaphore_handle(m_attached_produced_semaphore);
			const auto consumed_semaphore_handle = gpu_state->device->export_semaphore_handle(m_attached_consumed_semaphore);
			const gpu::image_format format = gpu_state->swapchain->format();
			bool ok = produced_semaphore_handle.has_value() && consumed_semaphore_handle.has_value();
			for (std::size_t i = 0; ok && i < attached_ring_size; ++i) {
				auto surface = gpu_state->device->create_shared_surface({
					.extent = ext,
					.format = format,
					.usage = { gpu::image_flag::color_attachment, gpu::image_flag::sampled },
				});
				if (!surface) {
					log::println(log::level::error, log::category::vulkan, "attached: create_shared_surface[{}] failed: {}", i, surface.error());
					ok = false;
					break;
				}
				m_attached_surfaces[i] = *surface;
				const gpu::image_view_create_info view_info{
					.format = format,
					.view_type = gpu::image_view_type::e2d,
					.aspects = gpu::image_aspect_flags(gpu::image_aspect_flag::color),
					.level_count = 1,
					.layer_count = 1,
				};
				m_attached_surface_images[i] = gpu::image(
					m_attached_surfaces[i].image,
					m_attached_surfaces[i].view,
					format,
					{ ext.x(), ext.y(), 1 },
					view_info
				);
			}
			if (ok) {
				m_attached_message = {
					.magic = attached_surface_magic,
					.extent = ext,
					.format = format,
					.backend = gpu::active_backend,
					.surface_handles = { m_attached_surfaces[0].handle, m_attached_surfaces[1].handle, m_attached_surfaces[2].handle },
					.produced_semaphore_handle = *produced_semaphore_handle,
					.consumed_semaphore_handle = *consumed_semaphore_handle,
				};
				m_attached_surface_ready = true;
				log::println(log::category::vulkan, "attached: created {}-surface ring at {}x{} on {}", attached_ring_size, ext.x(), ext.y(), gpu::active_backend);
			}
			else {
				if (!produced_semaphore_handle) {
					log::println(log::level::error, log::category::vulkan, "attached: export produced semaphore failed: {}", produced_semaphore_handle.error());
				}
				else if (!consumed_semaphore_handle) {
					log::println(log::level::error, log::category::vulkan, "attached: export consumed semaphore failed: {}", consumed_semaphore_handle.error());
				}
				if (produced_semaphore_handle) {
					win32::CloseHandle(*produced_semaphore_handle);
				}
				if (consumed_semaphore_handle) {
					win32::CloseHandle(*consumed_semaphore_handle);
				}
				destroy_attached_surface(*gpu_state->device);
			}
		}
	}

	if (m_attached_surface_ready && gpu_state && gpu_state->render_graph) {
		const std::uint64_t counter = ++m_attached_counter;
		const std::size_t slot = static_cast<std::size_t>(counter % attached_ring_size);
		if (counter > attached_ring_size) {
			gpu_state->render_graph->add_graphics_wait({
				.semaphore = m_attached_consumed_semaphore,
				.value = counter - attached_ring_size,
				.stages = gpu::pipeline_stage_flag::all_commands,
			});
		}
		gpu_state->render_graph->set_offscreen_target(&m_attached_surface_images[slot]);
		gpu_state->render_graph->add_graphics_signal({
			.semaphore = m_attached_produced_semaphore,
			.value = counter,
			.stages = gpu::pipeline_stage_flag::all_commands,
		});
	}

	if (gpu_state) {
		auto& window_state = m_scheduler.state<window::data>();
		const clock fence_timer;
		std::expected<gpu::frame_token, gpu::frame_status> result;
		{
			trace::scope_guard sg{ trace_id<"render::begin_frame">() };
			result = gpu::context::begin_frame(*gpu_state, &window_state);
		}
		const auto fence_wait = fence_timer.elapsed();

		// Convert gse::time -> raw seconds at the boundary; the scheduler keeps
		// quantity off its module surface (GCC PR c++/122785 workaround).
		gpu_state->scheduler.report_frame_time(fence_wait.as<gse::seconds>());
		frame_ok = result.has_value();

		if (!result && result.error() == gpu::frame_status::device_lost) {
			log::println(
				log::level::error,
				log::category::vulkan,
				"Device lost during begin_frame Ã¢â‚¬â€ terminating"
			);
			std::abort();
		}
	}

	m_scheduler.render(
		frame_ok,
		[this, gpu_state] {
			if (gpu_state) {
				gpu_state->scheduler.flush();
				{
					trace::scope_guard sg{ trace_id<"render::graph_execute">() };
					gpu::context::execute_frame(*gpu_state, m_scheduler);
				}
			}
		}
	);

	if (frame_ok && gpu_state) {
		{
			trace::scope_guard sg{ trace_id<"render::end_frame">() };
			auto& window_state = m_scheduler.state<window::data>();
			gpu::context::end_frame(*gpu_state, &window_state);
			if (asset_state) {
				trace::scope_guard sg{ trace_id<"end_frame::finalize_reloads">() };
				for (const auto& l : std::views::values(asset_state->resource_loaders)) {
					l->finalize_reloads();
				}
			}
			const bool attach_failed = m_attached_surface_attempted && !m_attached_surface_ready;
			if (attach_failed && window_state.attached) {
				window_state.attached = false;
				window_state.cursor_suppressed = false;
				window_state.framebuffer_resized = true;
			}
			if (!m_window_shown && (!m_config.attached || attach_failed)) {
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
	if (const auto* moved = std::get_if<input::mouse_moved>(&event); moved && window_state->ui_focus) {
		const auto dims = window::viewport(*window_state);
		const double clamped_x = std::clamp(moved->x_pos, 0.0, static_cast<double>(dims.x()));
		const double clamped_y = std::clamp(moved->y_pos, 0.0, static_cast<double>(dims.y()));
		window_state->input_events.push(input::mouse_moved{
			.x_pos = clamped_x,
			.y_pos = static_cast<double>(dims.y()) - clamped_y,
		});
		return;
	}
	window_state->input_events.push(event);
}

auto gse::engine::shutdown() -> void {
	profile::dump();
	profile::dump_chrome_trace();
	profile::dump_report();

	m_save.save_now();
	m_save.set_auto_save(false);

	if (auto* gpu_state = m_scheduler.try_state_of<gpu::context::data>()) {
		gpu::context::wait_idle(*gpu_state);
		destroy_attached_surface(*gpu_state->device);
	}

	m_scheduler.enter_shutdown();
	m_scheduler.shutdown();

	layout_store::flush();

	m_scheduler.clear();
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

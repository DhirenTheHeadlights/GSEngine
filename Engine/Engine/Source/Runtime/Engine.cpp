module gse.runtime;

import std;

import :engine;
import :scene;
import :world_system;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.network;
import gse.graphics;
import gse.audio;
import gse.physics;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.log;
import gse.save;
import gse.config;

gse::engine::engine(const std::string& name, const flags<engine_flag> engine_flags)
	: identifiable(name),
	  m_flags(engine_flags) {
}

auto gse::engine::initialize(const setup_fn& app_setup) -> void {
	trace::start({ .per_thread_event_cap = static_cast<std::size_t>(1e6) });

	m_scheduler.set_registry(m_registry);

	m_save.set_auto_save(true, config::resource_path / "Misc/settings.ini");
	m_save.set_on_restart([] {
		app::restart();
	});
	m_scheduler.set_settings_register_hook([this](settings::register_settings_type entry) {
		m_save.add(std::move(entry));
	});
	m_scheduler.register_external_resource<save::registry>(&m_save);
	m_scheduler.register_external_resource<primitives::data>(&m_primitives);

	if (m_flags.test(engine_flag::render)) {
		add_system<input::system>();
		add_system<actions::system>();
		add_system<world_system>();
		auto win = add_system<window>();
		win->title = std::string(id().tag());
		add_system<gpu::context>();
		add_system<asset::registry>();
		add_system<renderer::system>();
		add_system<renderer::scene_snapshot::system>();
		add_system<renderer::ui::system>();
		add_system<gui::system>();

		auto& asset_state = m_scheduler.state<asset::data>();
		using game_assets = gse::assets::append<graphics::asset_types, audio::asset_types>;
		gse::asset::system_for<game_assets> assets{ asset_state };
		assets.register_loaders();
		primitives::initialize(m_primitives, asset_state);
		assets.install_hot_reload_fns();

		m_loading.set_phase("Compiling boot assets");
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

		m_scheduler.initialize();
		m_scheduler.enter_running();

		auto& gui_data = m_scheduler.state<gse::gui::system::data>();
		gui_data.menu_stack.push<gse::gui::loading_screen>(m_loading);

		auto* asset_state_ptr = &asset_state;

		task::post([this, app_setup, asset_state_ptr] {
			add_system<physics::system>();
			add_system<camera::system>();
			add_system<primitive_resolver::system>();
			add_system<renderer::geometry_collector::system>();
			add_system<renderer::cull_compute::system>();
			add_system<renderer::physics_transform::system>();
			add_system<renderer::depth_prepass::system>();
			add_system<renderer::rt_shadow::system>();
			add_system<renderer::light_culling::system>();
			add_system<renderer::forward::system>();
			add_system<renderer::sdf_grid::system>();
			add_system<renderer::world_text::system>();
			add_system<renderer::physics_debug::system>();
			add_system<renderer::capture::system>();
			add_system<audio::system>();

			using game_assets = gse::assets::append<graphics::asset_types, audio::asset_types>;
			gse::asset::system_for<game_assets> assets{ *asset_state_ptr };

			m_loading.set_phase("Compiling assets");
			if (const auto result = assets.compile_non_boot_critical();
				result.success_count > 0 || result.failure_count > 0) {
				log::println(
					result.failure_count > 0 ? log::level::warning : log::level::info,
					log::category::assets,
					"Compiled {} assets ({} skipped, {} failed)",
					result.success_count,
					result.skipped_count,
					result.failure_count
				);
			}

			if (app_setup) {
				m_loading.set_phase("Initializing game");
				app_setup(*this);
			}

			m_loading.mark_finished();
		});
	}
	else {
		add_system<input::system>();
		add_system<actions::system>();
		add_system<world_system>();
		add_system<asset::registry>();

		auto& asset_state = m_scheduler.state<asset::data>();
		asset::add_loader<model>(asset_state);

		add_system<physics::system>();

		if (app_setup) {
			app_setup(*this);
		}

		m_scheduler.initialize();
		m_scheduler.enter_running();
		m_loading.mark_finished();
	}
}

auto gse::engine::update() -> void {
	system_clock::update();
	m_scheduler.update();
}

auto gse::engine::render() -> void {
	bool frame_ok = false;
	auto* gpu_state = m_scheduler.try_state_of<gpu::context::data>();
	auto* asset_state = m_scheduler.try_state_of<asset::data>();

	if (gpu_state) {
		auto& window_state = m_scheduler.state<window::data>();
		const clock fence_timer;
		std::expected<gpu::frame_token, gpu::frame_status> result;
		{
			trace::scope_guard sg{ trace_id<"render::begin_frame">() };
			result = gpu::context::begin_frame(*gpu_state, window_state);
		}
		const auto fence_wait = fence_timer.elapsed();

		gpu_state->scheduler.report_frame_time(fence_wait);
		frame_ok = result.has_value();

		if (!result && result.error() == gpu::frame_status::device_lost) {
			log::println(log::level::error, log::category::vulkan, "Device lost during begin_frame — terminating");
			std::abort();
		}
	}

	m_scheduler.render(frame_ok, [this, gpu_state] {
		if (gpu_state) {
			gpu_state->scheduler.flush();
			{
				trace::scope_guard sg{ trace_id<"render::graph_execute">() };
				auto requests = m_scheduler.drain_channel<gpu::render_pass_request>();
				auto transient_images = m_scheduler.drain_channel<gpu::transient_image_request>();
				auto transient_buffers = m_scheduler.drain_channel<gpu::transient_buffer_request>();
				gpu::context::execute_frame(
					*gpu_state,
					std::move(requests),
					std::move(transient_images),
					std::move(transient_buffers)
				);
			}
		}
	});

	if (frame_ok && gpu_state) {
		{
			trace::scope_guard sg{ trace_id<"render::end_frame">() };
			auto& window_state = m_scheduler.state<window::data>();
			gpu::context::end_frame(*gpu_state, window_state);
			if (asset_state) {
				trace::scope_guard sg{ trace_id<"end_frame::finalize_reloads">() };
				for (const auto& l : std::views::values(asset_state->resource_loaders)) {
					l->finalize_reloads();
				}
			}
		}
	}
}

auto gse::engine::shutdown() -> void {
	profile::dump();
	profile::dump_chrome_trace();

	m_save.save_now();
	m_save.set_auto_save(false);

	if (auto* gpu_state = m_scheduler.try_state_of<gpu::context::data>()) {
		gpu::context::wait_idle(*gpu_state);
	}

	m_scheduler.enter_shutdown();
	m_scheduler.shutdown();

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

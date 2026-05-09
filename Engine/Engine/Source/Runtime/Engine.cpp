module gse.runtime;

import std;

import :engine;
import :scene;
import :world;

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

gse::engine::engine(const std::string& name, const flags<engine_flag> engine_flags) : identifiable(name), m_flags(engine_flags), m_world(m_scheduler, "World") {}

auto gse::engine::initialize(const setup_fn& app_setup) -> void {
	trace::start({
		.per_thread_event_cap = static_cast<std::size_t>(1e6)
	});

	m_scheduler.set_registry(m_world.registry());

	m_save.set_auto_save(true, config::resource_path / "Misc/settings.ini");
	m_save.set_on_restart([] { app::restart(); });
	m_scheduler.register_external_resource<save::registry>(&m_save);

	add_system<input::system>();
	add_system<actions::system>();
	add_system<network::system>();

	if (m_flags.test(engine_flag::render)) {
		auto& window_state = add_system<window>();
		window_state.title = std::string(id().tag());
		add_system<gpu::context>();
		add_system<asset::registry>();
		add_system<physics::system>();
		add_system<camera::system>();
		add_system<renderer::system>();
		add_system<renderer::geometry_collector::system>();
		add_system<renderer::skin_compute::system>();
		add_system<renderer::cull_compute::system>();
		add_system<renderer::physics_transform::system>();
		add_system<renderer::depth_prepass::system>();
		add_system<renderer::rt_shadow::system>();
		add_system<renderer::light_culling::system>();
		add_system<renderer::forward::system>();
		add_system<renderer::physics_debug::system>();
		add_system<renderer::ui::system>();
		add_system<renderer::capture::system>();
		add_system<gui::system>();
		add_system<animation::system>();
		add_system<audio::system>();

		auto& asset_state = m_scheduler.state<asset::state>();

		using game_assets = gse::assets::append<graphics::asset_types, audio_clip>;
		gse::asset::system_for<game_assets> assets{ asset_state };
		assets.register_loaders();
		assets.install_hot_reload_fns();
		if (const auto result = assets.compile_all(); result.success_count > 0 || result.failure_count > 0) {
			log::println(
				result.failure_count > 0 ? log::level::warning : log::level::info,
				log::category::assets,
				"Compiled {} assets ({} skipped, {} failed)",
				result.success_count,
				result.skipped_count,
				result.failure_count
			);
		}
	}
	else {
		add_system<asset::registry>();

		auto& asset_state = m_scheduler.state<asset::state>();
		asset::add_loader<model>(asset_state);
		asset::add_loader<skinned_model>(asset_state);

		add_system<physics::system>();
	}

	m_scheduler.initialize();

	if (app_setup) {
		app_setup(*this);
		m_scheduler.initialize();
	}
}

auto gse::engine::update() -> void {
	system_clock::update();
	m_scheduler.update();
	drain_lifecycle_channels();
	m_world.update();
}

auto gse::engine::drain_lifecycle_channels() -> void {
	for (const auto& r : m_scheduler.drain_channel<set_networked_request>()) {
		m_world.set_networked(r.value);
	}
	for (const auto& r : m_scheduler.drain_channel<set_authoritative_request>()) {
		m_world.set_authoritative(r.value);
	}
	for (const auto& r : m_scheduler.drain_channel<set_local_controller_id_request>()) {
		m_world.set_local_controller_id(r.controller_id);
	}
	for (const auto& r : m_scheduler.drain_channel<activate_scene_request>()) {
		m_world.activate(r.scene_id);
	}
	const auto deactivate_requests = m_scheduler.drain_channel<deactivate_active_scene_request>();
	if (!deactivate_requests.empty()) {
		if (auto* active = m_world.current_scene()) {
			m_world.deactivate(active->id());
		}
	}
}

auto gse::engine::render() -> void {
	bool frame_ok = false;
	auto* gpu_state = m_scheduler.try_state_of<gpu::context::state>();
	auto* asset_state = m_scheduler.try_state_of<asset::state>();

	if (gpu_state) {
		auto& window_state = m_scheduler.state<window::state>();
		const clock fence_timer;
		std::expected<gpu::frame_token, gpu::frame_status> result;
		{
			trace::scope_guard sg{trace_id<"render::begin_frame">()};
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
				trace::scope_guard sg{trace_id<"render::graph_execute">()};
				auto requests = m_scheduler.drain_channel<gpu::render_pass_request>();
				gpu::context::execute_frame(*gpu_state, std::move(requests));
			}
		}

		m_world.render();
	});

	if (frame_ok && gpu_state) {
		{
			trace::scope_guard sg{trace_id<"render::end_frame">()};
			auto& window_state = m_scheduler.state<window::state>();
			gpu::context::end_frame(*gpu_state, window_state);
			if (asset_state) {
				trace::scope_guard sg{trace_id<"end_frame::finalize_reloads">()};
				for (const auto& l : std::views::values(asset_state->resource_loaders)) {
					l->finalize_reloads();
				}
			}
		}
	}
}

auto gse::engine::shutdown() -> void {
	profile::dump();

	m_save.save_now();
	m_save.set_auto_save(false);

	m_scheduler.shutdown();
	m_world.shutdown();

	m_scheduler.clear();
}

auto gse::engine::make_channel_writer() -> channel_writer {
	return m_scheduler.make_channel_writer();
}

auto gse::engine::world() -> gse::world& {
	return m_world;
}

auto gse::engine::direct() -> director {
	return m_world.direct();
}

auto gse::engine::triggers() const -> std::span<const trigger> {
	return m_world.triggers();
}

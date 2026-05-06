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
import gse.settings;
import gse.config;

gse::engine::engine(const std::string& name, const flags<engine_flag> engine_flags) : identifiable(name), m_flags(engine_flags), m_world(m_scheduler, "World") {}

auto gse::engine::initialize(const setup_fn& app_setup) -> void {
	trace::start({
		.per_thread_event_cap = static_cast<std::size_t>(1e6)
	});

	m_scheduler.set_registry(m_world.registry());

	auto& save = m_scheduler.add_system<save::system>();
	save::system::set_auto_save(save, true, config::resource_path / "Misc/settings.ini");
	save::system::on_restart(save, [] { app::restart(); });

	m_scheduler.add_system<input::system>();
	m_scheduler.add_system<actions::system>();
	m_scheduler.add_system<network::system>();
	m_scheduler.add_system<asset::registry>();

	if (m_flags.test(engine_flag::render)) {
		auto& window_state = m_scheduler.add_system<window>();
		window_state.title = std::string(id().tag());
		m_scheduler.add_system<gpu::context>();

		m_scheduler.initialize();

		auto& gpu_state = m_scheduler.state<gpu::context::state>();
		auto& asset_state = m_scheduler.state<asset::registry::state>();
		auto& command_channel = m_scheduler.channel<gpu::command_request>();
		auto& finalization_channel = m_scheduler.channel<gpu::pending_finalization>();

		asset_state.async_submit = [&command_channel](std::function<void(void*)> work) {
			command_channel.push(gpu::command_request{
				.work = [work_lambda = std::move(work)](gpu::context::state& s) {
					work_lambda(&s);
				},
			});
		};
		asset_state.sync_submit = [&gpu_state](std::function<void(void*)> work) {
			work(&gpu_state);
		};
		asset_state.finalization_pusher = [&finalization_channel](const gse::id resource_type, const gse::id resource_id) {
			finalization_channel.push(gpu::pending_finalization{
				.resource_type = resource_type,
				.resource_id = resource_id,
			});
		};
		asset_state.gpu_waiter = [&gpu_state] {
			gpu_state.device->wait_idle();
		};

		asset::registry::add_loader<shader>(asset_state);
		asset::registry::add_loader<texture>(asset_state);
		asset::registry::add_loader<model>(asset_state);
		asset::registry::add_loader<skinned_model>(asset_state);
		asset::registry::add_loader<font>(asset_state);
		asset::registry::add_loader<skeleton>(asset_state);
		asset::registry::add_loader<clip_asset>(asset_state);
		asset::registry::add_loader<audio_clip>(asset_state);
		asset::registry::compile<shader_layout>(asset_state);
		asset::registry::compile_all(asset_state);

		m_scheduler.add_system<physics::system>();
		m_scheduler.add_system<camera::system>();
		m_scheduler.add_system<render_init::system>();
		m_scheduler.add_system<renderer::system>();
		m_scheduler.add_system<gui::system>();
		m_scheduler.add_system<animation::system>();
		m_scheduler.add_system<audio::system>();
	}
	else {
		m_scheduler.initialize();

		auto& asset_state = m_scheduler.state<asset::registry::state>();
		asset::registry::add_loader<model>(asset_state);
		asset::registry::add_loader<skinned_model>(asset_state);

		m_scheduler.add_system<physics::system>();
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
	m_world.update();
}

auto gse::engine::render() -> void {
	bool frame_ok = false;
	auto* gpu_state = m_scheduler.try_state_of<gpu::context::state>();
	auto* asset_state = m_scheduler.try_state_of<asset::registry::state>();

	if (gpu_state) {
		if (asset_state) {
			trace::scope_guard sg{trace_id<"render::finalize_pending_loads">()};
			const auto& finalizations = m_scheduler.channel<gpu::pending_finalization>().read_raw();
			std::vector<std::pair<gse::id, gse::id>> snapshot;
			snapshot.reserve(finalizations.size());
			for (const auto& f : finalizations) {
				snapshot.emplace_back(f.resource_type, f.resource_id);
			}
			asset::registry::apply_finalizations(*asset_state, snapshot);
		}

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
		{
			trace::scope_guard sg{trace_id<"render::in_frame">()};
			if (gpu_state) {
				{
					trace::scope_guard sg{trace_id<"render::scheduler_flush">()};
					gpu_state->scheduler.flush();
				}
				{
					trace::scope_guard sg{trace_id<"render::graph_execute">()};
					auto requests = m_scheduler.drain_channel<gpu::render_pass_request>();
					gpu::context::execute_frame(*gpu_state, std::move(requests));
				}
			}

			{
				trace::scope_guard sg{trace_id<"render::world_render">()};
				m_world.render();
			}
		}
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

	m_scheduler.shutdown();
	m_world.shutdown();

	m_scheduler.clear();
}

auto gse::engine::set_networked(const bool networked) -> void {
	m_world.set_networked(networked);
}

auto gse::engine::set_authoritative(const bool authoritative) -> void {
	m_world.set_authoritative(authoritative);
}

auto gse::engine::set_local_controller_id(const gse::id controller_id) -> void {
	m_world.set_local_controller_id(controller_id);
}

auto gse::engine::set_input_sampler(input_sampler_fn fn) -> void {
	m_world.set_input_sampler(std::move(fn));
}

auto gse::engine::activate_scene(const gse::id scene_id) -> void {
	m_world.activate(scene_id);
	profile::reset();
}

auto gse::engine::deactivate_scene(const gse::id scene_id) -> void {
	m_world.deactivate(scene_id);
}

auto gse::engine::current_scene() -> scene* {
	return m_world.current_scene();
}

auto gse::engine::direct() -> director {
	return m_world.direct();
}

auto gse::engine::triggers() const -> std::span<const trigger> {
	return m_world.triggers();
}

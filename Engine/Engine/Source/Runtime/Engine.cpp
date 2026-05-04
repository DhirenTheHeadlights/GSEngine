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

auto gse::engine::initialize() -> void {
	trace::start({
		.per_thread_event_cap = static_cast<std::size_t>(1e6)
	});

	auto& reg = m_world.registry();
	auto& save = m_scheduler.add_system<save::system>(reg);
	save::system::set_auto_save(save, true, config::resource_path / "Misc/settings.ini");
	save::system::on_restart(save, [] { app::restart(); });

	auto& input = m_scheduler.add_system<input::system>(reg);
	m_scheduler.add_system<actions::system>(reg);
	m_scheduler.add_system<network::system>(reg);

	if (m_flags.test(engine_flag::render)) {
		m_render_ctx = std::make_unique<gpu::context>(
			std::string(id().tag()),
			input,
			save
		);

		auto& ctx = *m_render_ctx.get();
		m_assets = std::make_unique<asset::registry>(ctx);
		ctx.set_asset_registry(m_assets.get());
		m_assets->add_loader<shader>(ctx);
		m_assets->compile<shader_layout>();
		m_scheduler.set_gpu_context(&ctx);
		m_scheduler.set_asset_registry(m_assets.get());

		m_scheduler.add_system<physics::system>(reg);
		m_scheduler.add_system<camera::system>(reg);
		m_scheduler.add_system<render_init::system>(reg);
		m_scheduler.add_system<renderer::system>(reg);
		m_scheduler.add_system<gui::system>(reg);
		m_scheduler.add_system<animation::system>(reg);
		m_scheduler.add_system<audio::system>(reg);
	}
	else {
		m_scheduler.add_system<physics::system>(reg);
	}

	m_scheduler.initialize();
}

auto gse::engine::update() -> void {
	static std::uint64_t s_frame = 0;
	const auto fid = ++s_frame;
	log::println(log::category::runtime, "[freeze-trace] update enter frame={}", fid);

	system_clock::update();

	log::println(log::category::runtime, "[freeze-trace] update: scheduler.update enter (frame={})", fid);
	m_scheduler.update();
	log::println(log::category::runtime, "[freeze-trace] update: scheduler.update exit (frame={})", fid);

	log::println(log::category::runtime, "[freeze-trace] update: world.update enter (frame={})", fid);
	m_world.update();
	log::println(log::category::runtime, "[freeze-trace] update: world.update exit (frame={})", fid);
}

auto gse::engine::render() -> void {
	static std::uint64_t s_frame = 0;
	const auto fid = ++s_frame;
	log::println(log::category::runtime, "[freeze-trace] render enter frame={}", fid);

	bool frame_ok = false;

	if (m_render_ctx) {
		{
			trace::scope_guard sg{trace_id<"render::process_gpu_queue">()};
			log::println(log::category::runtime, "[freeze-trace] render: process_gpu_queue enter (frame={})", fid);
			m_render_ctx->process_gpu_queue();
			log::println(log::category::runtime, "[freeze-trace] render: process_gpu_queue exit (frame={})", fid);
		}

		if (m_assets) {
			trace::scope_guard sg{trace_id<"render::finalize_pending_loads">()};
			log::println(log::category::runtime, "[freeze-trace] render: finalize_pending_loads enter (frame={})", fid);
			m_assets->finalize_pending_loads();
			log::println(log::category::runtime, "[freeze-trace] render: finalize_pending_loads exit (frame={})", fid);
		}

		const clock fence_timer;
		std::expected<gpu::frame_token, gpu::frame_status> result;
		{
			trace::scope_guard sg{trace_id<"render::begin_frame">()};
			log::println(log::category::runtime, "[freeze-trace] render: begin_frame enter (frame={})", fid);
			result = m_render_ctx->begin_frame();
			log::println(log::category::runtime, "[freeze-trace] render: begin_frame exit (frame={}, ok={})", fid, result.has_value());
		}
		const auto fence_wait = fence_timer.elapsed();

		m_render_ctx->scheduler().report_frame_time(fence_wait);
		frame_ok = result.has_value();

		if (!result && result.error() == gpu::frame_status::device_lost) {
			log::println(log::level::error, log::category::vulkan, "Device lost during begin_frame — terminating");
			std::abort();
		}
	}

	log::println(log::category::runtime, "[freeze-trace] render: scheduler.render enter (frame={}, ok={})", fid, frame_ok);
	m_scheduler.render(frame_ok, [this, fid] {
		{
			trace::scope_guard sg{trace_id<"render::in_frame">()};
			if (m_render_ctx) {
				{
					trace::scope_guard sg{trace_id<"render::scheduler_flush">()};
					log::println(log::category::runtime, "[freeze-trace] render: gpu_scheduler.flush enter (frame={})", fid);
					m_render_ctx->scheduler().flush();
					log::println(log::category::runtime, "[freeze-trace] render: gpu_scheduler.flush exit (frame={})", fid);
				}
				{
					trace::scope_guard sg{trace_id<"render::graph_execute">()};
					log::println(log::category::runtime, "[freeze-trace] render: graph.execute enter (frame={})", fid);
					m_render_ctx->graph().execute();
					log::println(log::category::runtime, "[freeze-trace] render: graph.execute exit (frame={})", fid);
				}
			}

			{
				trace::scope_guard sg{trace_id<"render::world_render">()};
				log::println(log::category::runtime, "[freeze-trace] render: world.render enter (frame={})", fid);
				m_world.render();
				log::println(log::category::runtime, "[freeze-trace] render: world.render exit (frame={})", fid);
			}
		}
	});
	log::println(log::category::runtime, "[freeze-trace] render: scheduler.render exit (frame={})", fid);

	if (frame_ok && m_render_ctx) {
		{
			trace::scope_guard sg{trace_id<"render::end_frame">()};
			log::println(log::category::runtime, "[freeze-trace] render: end_frame enter (frame={})", fid);
			m_render_ctx->end_frame();
			log::println(log::category::runtime, "[freeze-trace] render: end_frame exit (frame={})", fid);
			if (m_assets) {
				trace::scope_guard sg{trace_id<"end_frame::finalize_reloads">()};
				log::println(log::category::runtime, "[freeze-trace] render: finalize_reloads enter (frame={})", fid);
				m_assets->finalize_reloads();
				log::println(log::category::runtime, "[freeze-trace] render: finalize_reloads exit (frame={})", fid);
			}
		}
	}

	log::println(log::category::runtime, "[freeze-trace] render exit frame={}", fid);
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

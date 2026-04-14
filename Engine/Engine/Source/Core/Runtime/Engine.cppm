export module gse.runtime:engine;

import std;

import gse.utility;
import gse.network;
import gse.graphics;
import gse.audio;
import gse.physics;
import gse.platform;
import gse.log;

import :world;

export namespace gse {
	enum class engine_flag : std::uint8_t {
		create_window = 1 << 0,
		render = 1 << 1,
	};

	class engine : public hookable<engine> {
	public:
		explicit engine(const std::string& name, flags<engine_flag> engine_flags);

		auto initialize() -> void override;
		auto update() -> void override;
		auto render() -> void override;
		auto shutdown() -> void override;

		template <typename State>
		auto state_of() -> const State&;

		template <typename State>
		auto try_state_of() -> const State*;


		template <typename State>
		auto has_state() const -> bool;

		template <typename T>
		auto channel() -> channel<T>&;

		template <typename State, typename F>
		auto defer(F&& fn) -> void;

		template <typename T, class ... Args>
		auto hook_world(
			Args&&... args
		) -> T&;

		auto set_networked(
			bool networked
		) -> void;

		auto set_authoritative(
			bool authoritative
		) -> void;

		auto set_local_controller_id(
			gse::id controller_id
		) -> void;

		template <typename T, typename... Args>
		auto add_scene(
			Args... args
		) -> scene*;

		auto activate_scene(
			gse::id scene_id
		) -> void;

		auto deactivate_scene(
			gse::id scene_id
		) -> void;

		auto current_scene(
		) -> scene*;

		auto direct(
		) -> director;

		template <typename F>
		auto defer(
			F&& action
		) -> void;
	private:
		flags<engine_flag> m_flags;
		scheduler m_scheduler;
		world m_world;
		std::unique_ptr<gpu::context> m_render_ctx;
	};
}

gse::engine::engine(const std::string& name, const flags<engine_flag> engine_flags)
	: hookable(name), m_flags(engine_flags), m_world(m_scheduler, "World") {}

auto gse::engine::initialize() -> void {
	trace::start({
		.per_thread_event_cap = static_cast<std::size_t>(1e6)
	});

	auto& reg = m_world.registry();
	auto& save = m_scheduler.add_system<save::system, save::state>(reg);
	save.set_auto_save(true, config::resource_path / "Misc/settings.toml");
	save.on_restart([] { app::restart(); });

	auto& input = m_scheduler.add_system<input::system, input::system_state>(reg);
	m_scheduler.add_system<actions::system, actions::system_state>(reg);
	m_scheduler.add_system<network::system, network::system_state>(reg);

	if (m_flags.test(engine_flag::render)) {
		m_render_ctx = std::make_unique<gpu::context>(
			std::string(id().tag()),
			input,
			save
		);

		auto& ctx = *m_render_ctx.get();
		ctx.add_loader<shader>();
		ctx.compile();
		m_scheduler.set_gpu_context(&ctx);

		m_scheduler.add_system<physics::system, physics::state>(reg);
		m_scheduler.add_system<camera::system, camera::state>(reg);
		m_scheduler.add_system<renderer::system, renderer::state>(reg);
		m_scheduler.add_system<renderer::rt_shadow::system, renderer::rt_shadow::state>(reg);
		m_scheduler.add_system<renderer::geometry_collector::system, renderer::geometry_collector::state>(reg);
		m_scheduler.add_system<renderer::skin_compute::system, renderer::skin_compute::state>(reg);
		m_scheduler.add_system<renderer::physics_transform::system, renderer::physics_transform::state>(reg);
		m_scheduler.add_system<renderer::cull_compute::system, renderer::cull_compute::state>(reg);
		m_scheduler.add_system<renderer::depth_prepass::system, renderer::depth_prepass::state>(reg);
		m_scheduler.add_system<renderer::light_culling::system, renderer::light_culling::state>(reg);
		m_scheduler.add_system<renderer::forward::system, renderer::forward::state>(reg);
		m_scheduler.add_system<renderer::physics_debug::system, renderer::physics_debug::state>(reg);
		m_scheduler.add_system<renderer::ui::system, renderer::ui::state>(reg);
		m_scheduler.add_system<renderer::capture::system, renderer::capture::state>(reg);
		m_scheduler.add_system<gui::system, gui::system_state>(reg);
		m_scheduler.add_system<animation::system, animation::state>(reg);
		m_scheduler.add_system<audio::system, audio::state>(reg);
	}
	else {
		m_scheduler.add_system<physics::system, physics::state>(reg);
	}

	m_scheduler.initialize();

	for (const auto& h : m_hooks) {
		h->initialize();
	}

	m_world.initialize();
}

auto gse::engine::update() -> void {
	system_clock::update();

	m_world.update();

	m_scheduler.update();

	for (const auto& h : m_hooks) {
		h->update();
	}
}

auto gse::engine::render() -> void {
	bool frame_ok = false;

	if (m_render_ctx) {
		m_render_ctx->process_gpu_queue();

		const clock fence_timer;
		auto result = m_render_ctx->begin_frame();
		const auto fence_wait = fence_timer.elapsed();

		m_render_ctx->scheduler().report_frame_time(fence_wait);
		frame_ok = result.has_value();

		if (!result && result.error() == gpu::frame_status::device_lost) {
			log::println(log::level::error, log::category::vulkan, "Device lost during begin_frame — terminating");
			std::abort();
		}
	}

	m_scheduler.render(frame_ok, [this] {
		if (m_render_ctx) {
			m_render_ctx->scheduler().flush();
			m_render_ctx->graph().execute();
		}

		for (const auto& h : m_hooks) {
			h->render();
		}

		m_world.render();
	});

	if (frame_ok && m_render_ctx) {
		m_render_ctx->end_frame();
		m_render_ctx->finalize_reloads();
	}
}

auto gse::engine::shutdown() -> void {
	m_scheduler.shutdown();
	m_world.shutdown();

	for (const auto& h : m_hooks) {
		h->shutdown();
	}

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

auto gse::engine::activate_scene(const gse::id scene_id) -> void {
	m_world.activate(scene_id);
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

template <typename State>
auto gse::engine::state_of() -> const State& {
	return m_scheduler.state<State>();
}

template <typename State>
auto gse::engine::try_state_of() -> const State* {
	return m_scheduler.try_state_of<State>();
}

template <typename State>
auto gse::engine::has_state() const -> bool {
	return m_scheduler.has<State>();
}

template <typename T>
auto gse::engine::channel() -> gse::channel<T>& {
	return m_scheduler.channel<T>();
}

template <typename State, typename F>
auto gse::engine::defer(F&& fn) -> void {
	m_scheduler.defer<State>(std::forward<F>(fn));
}

template <typename T, typename... Args>
auto gse::engine::hook_world(Args&&... args) -> T& {
	return m_world.add_hook<T>(std::forward<Args>(args)...);
}

template <typename T, typename ... Args>
auto gse::engine::add_scene(Args... args) -> scene* {
	return m_world.add<T>(std::forward<Args>(args)...);
}

template <typename F>
auto gse::engine::defer(F&& action) -> void {
	if (const auto* scene = current_scene()) {
		scene->registry().add_deferred_action(
			{},
			[action = std::forward<F>(action)](registry& reg) {
				action(reg);
				return true;
			}
		);
	}
}

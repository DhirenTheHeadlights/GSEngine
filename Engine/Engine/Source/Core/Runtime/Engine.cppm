export module gse.runtime:engine;

import std;

import gse.utility;
import gse.network;
import gse.graphics;
import gse.platform;

import :world;

export namespace gse {
	class engine : public hookable<engine> {
	public:
		explicit engine(const std::string& name) : hookable(name), m_render_ctx(name) {}

		auto initialize() -> void override;
		auto update() -> void override;
		auto render() -> void override;
		auto shutdown() -> void override;

		template <typename S>
		auto system_of() -> S&;
	private:
		world m_world;
		scheduler m_scheduler;
		renderer::context m_render_ctx;
	};
}

auto gse::engine::initialize() -> void {
	trace::start({
		.per_thread_event_cap = static_cast<std::size_t>(1e6)
	});

	m_scheduler.emplace_system<input::system>();
	m_scheduler.emplace_system<actions::system>();
	m_scheduler.emplace_system<network::system>(m_world.registry());
	m_scheduler.emplace_system<physics::system>(m_world.registry());
	m_scheduler.emplace_system<renderer::system>(m_render_ctx);
	m_scheduler.emplace_system<renderer::shadow>(m_render_ctx);
	m_scheduler.emplace_system<renderer::geometry>(m_render_ctx);
	m_scheduler.emplace_system<renderer::lighting>(m_render_ctx);
	m_scheduler.emplace_system<renderer::physics_debug>(m_render_ctx);
	m_scheduler.emplace_system<renderer::sprite>(m_render_ctx);
	m_scheduler.emplace_system<renderer::text>(m_render_ctx);
	m_scheduler.emplace_system<gui::system>(m_render_ctx);

	m_scheduler.build();

	m_scheduler.run_initialize();

	for (const auto& h : m_hooks) {
		h->initialize();
	}
}

auto gse::engine::update() -> void {
	system_clock::update();

	m_world.update();
	m_scheduler.run_update();

	for (const auto& h : m_hooks) {
		h->update();
	}
}

auto gse::engine::render() -> void {
	m_scheduler.run_render();
}

auto gse::engine::shutdown() -> void {
	m_scheduler.run_shutdown();

	for (const auto& h : m_hooks) {
		h->shutdown();
	}
}

template <typename S>
auto gse::engine::system_of() -> S& {
	return m_scheduler.system<S>();
}



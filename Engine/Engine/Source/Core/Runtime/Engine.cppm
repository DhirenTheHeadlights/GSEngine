export module gse.runtime:engine;

import std;

import gse.utility;
import gse.network;
import gse.graphics;
import gse.physics;
import gse.platform;

import :world;

export namespace gse {
	class engine : public hookable<engine> {
	public:
		explicit engine(const std::string& name);

		auto initialize() -> void override;
		auto update() -> void override;
		auto render() -> void override;
		auto shutdown() -> void override;

		template <typename S>
		auto system_of() -> S&;

		template <typename T>
		auto channel() -> channel<T>&;

		template <typename T, class ... Args>
		auto hook_world(
			Args&&... args
		) -> T&;

		auto set_networked(
			bool networked
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
		scheduler m_scheduler;
		world m_world;
		std::unique_ptr<renderer::context> m_render_ctx;
	};
}

gse::engine::engine(const std::string& name): hookable(name), m_world(m_scheduler, "World") {}

auto gse::engine::initialize() -> void {
	trace::start({
		.per_thread_event_cap = static_cast<std::size_t>(1e6)
	});

	auto& reg = m_world.registry();
	auto& save = m_scheduler.emplace<save::system>(reg);
	save.set_auto_save(true, config::resource_path / "Misc/settings.cfg");

	auto& input = m_scheduler.emplace<input::system>(reg);
	m_scheduler.emplace<actions::system>(reg);
	m_scheduler.emplace<network::system>(reg);
	m_scheduler.emplace<physics::system>(reg);

	m_render_ctx = std::make_unique<renderer::context>(
		std::string(id().tag()),
		input,
		save
	);

	auto& ctx = *m_render_ctx.get();

	m_scheduler.emplace<renderer::system>(reg, ctx);
	m_scheduler.emplace<renderer::shadow>(reg, ctx);
	m_scheduler.emplace<renderer::geometry>(reg, ctx);
	m_scheduler.emplace<renderer::lighting>(reg, ctx);
	m_scheduler.emplace<renderer::physics_debug>(reg, ctx);
	m_scheduler.emplace<renderer::ui>(reg, ctx);
	m_scheduler.emplace<gui::system>(reg, ctx);
	m_scheduler.emplace<animation::system>(reg);

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
	m_scheduler.flush_deferred();

	for (const auto& h : m_hooks) {
		h->update();
	}
}

auto gse::engine::render() -> void {
	m_scheduler.render([this] {
		for (const auto& h : m_hooks) {
			h->render();
		}

		m_world.render();
	});
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

template <typename S>
auto gse::engine::system_of() -> S& {
	return m_scheduler.get<S>();
}

template <typename T>
auto gse::engine::channel() -> gse::channel<T>& {
	return m_scheduler.channel<T>();
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

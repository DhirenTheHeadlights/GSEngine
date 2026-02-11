export module gse.runtime:engine;

import std;

import gse.utility;
import gse.network;
import gse.graphics;
import gse.physics;
import gse.platform;

import :world;

export namespace gse {
	enum struct flags : std::uint8_t {
		none = 0,
		create_window = 1 << 0,
		render = 1 << 1,
	};

	constexpr auto operator|(flags lhs, flags rhs) -> flags {
		return static_cast<flags>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
	}

	constexpr auto has_flag(flags haystack, flags needle) -> bool {
		return (static_cast<std::uint32_t>(haystack) & static_cast<std::uint32_t>(needle)) == static_cast<std::uint32_t>(needle);
	}

	class engine : public hookable<engine> {
	public:
		explicit engine(const std::string& name, flags engine_flags);

		auto initialize() -> void override;
		auto update() -> void override;
		auto render() -> void override;
		auto shutdown() -> void override;

		template <typename State>
		auto state_of() -> State&;

		template <typename State>
		auto has_state() const -> bool;

		template <typename T>
		auto channel() -> channel<T>&;

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
		flags m_flags;
		scheduler m_scheduler;
		world m_world;
		std::unique_ptr<renderer::context> m_render_ctx;
	};
}

gse::engine::engine(const std::string& name, const flags engine_flags)
	: hookable(name), m_flags(engine_flags), m_world(m_scheduler, "World") {}

auto gse::engine::initialize() -> void {
	trace::start({
		.per_thread_event_cap = static_cast<std::size_t>(1e6)
	});

	auto& reg = m_world.registry();
	auto& save = m_scheduler.add_system<save::system, save::state>(reg);
	save.set_auto_save(true, config::resource_path / "Misc/settings.toml");

	auto& input = m_scheduler.add_system<input::system, input::system_state>(reg);
	m_scheduler.add_system<actions::system, actions::system_state>(reg);
	m_scheduler.add_system<network::system, network::system_state>(reg);
	m_scheduler.add_system<physics::system, physics::state>(reg);

	if (has_flag(m_flags, flags::render)) {
		m_render_ctx = std::make_unique<renderer::context>(
			std::string(id().tag()),
			input,
			save
		);

		auto& ctx = *m_render_ctx.get();

		m_scheduler.add_system<camera::system, camera::state>(reg);
		m_scheduler.add_system<renderer::system, renderer::state>(reg, ctx);
		m_scheduler.add_system<renderer::shadow::system, renderer::shadow::state>(reg, ctx);
		m_scheduler.add_system<renderer::geometry::system, renderer::geometry::state>(reg, ctx);
		m_scheduler.add_system<renderer::lighting::system, renderer::lighting::state>(reg, ctx);
		m_scheduler.add_system<renderer::physics_debug::system, renderer::physics_debug::state>(reg, ctx);
		m_scheduler.add_system<renderer::ui::system, renderer::ui::state>(reg, ctx);
		m_scheduler.add_system<gui::system, gui::system_state>(reg, ctx);
		m_scheduler.add_system<animation::system, animation::state>(reg);
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

auto gse::engine::set_authoritative(const bool authoritative) -> void {
	m_world.set_authoritative(authoritative);
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
auto gse::engine::state_of() -> State& {
	return m_scheduler.state<State>();
}

template <typename State>
auto gse::engine::has_state() const -> bool {
	return m_scheduler.has<State>();
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

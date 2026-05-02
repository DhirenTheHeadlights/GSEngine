export module gse.runtime:engine;

import std;

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
import gse.scene;
import gse.save;
import gse.config;

import :world;

export namespace gse {
	enum class engine_flag : std::uint8_t {
		create_window = 1 << 0,
		render = 1 << 1,
	};

	class engine : public identifiable {
	public:
		engine(
			const std::string& name,
			flags<engine_flag> engine_flags
		);

		auto initialize(
		) -> void;

		auto update(
		) -> void;

		auto render(
		) -> void;

		auto shutdown(
		) -> void;

		template <typename State>
		auto state_of(
		) -> const State&;

		template <typename State>
		auto try_state_of(
		) -> const State*;

		template <typename State>
		auto has_state(
		) const -> bool;

		template <typename Resources>
		auto resources_of(
		) const -> const Resources&;

		template <typename T>
		auto channel(
		) -> channel<T>&;

		template <typename State, typename F>
		auto defer(
			F&& fn
		) -> void;

		auto set_networked(
			bool networked
		) -> void;

		auto set_authoritative(
			bool authoritative
		) -> void;

		auto set_local_controller_id(
			gse::id controller_id
		) -> void;

		auto set_input_sampler(
			input_sampler_fn fn
		) -> void;

		auto add_scene(
			std::string_view name,
			scene::setup_fn setup = {}
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

		auto triggers(
		) const -> std::span<const trigger>;

		template <typename S, typename State, typename... Args>
		auto add_system(
			Args&&... args
		) -> State&;
	private:
		flags<engine_flag> m_flags;
		scheduler m_scheduler;
		world m_world;
		std::unique_ptr<gpu::context> m_render_ctx;
		std::unique_ptr<asset::registry> m_assets;
	};
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

template <typename Resources>
auto gse::engine::resources_of() const -> const Resources& {
	return m_scheduler.resources_of<Resources>();
}

template <typename T>
auto gse::engine::channel() -> gse::channel<T>& {
	return m_scheduler.channel<T>();
}

template <typename State, typename F>
auto gse::engine::defer(F&& fn) -> void {
	m_scheduler.defer<State>(std::forward<F>(fn));
}

auto gse::engine::add_scene(std::string_view name, scene::setup_fn setup) -> scene* {
	return m_world.add(name, std::move(setup));
}

template <typename S, typename State, typename... Args>
auto gse::engine::add_system(Args&&... args) -> State& {
	return m_scheduler.add_system<S, State>(m_world.registry(), std::forward<Args>(args)...);
}

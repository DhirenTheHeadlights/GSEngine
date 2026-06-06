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
import gse.gpu;
import gse.log;
import gse.save;
import gse.config;

import :scene;
import :world_system;

export namespace gse {
	using engine_networked_components =
		type_pack<physics::motion_component, physics::collision_component, render_component, player_controller>;

	struct engine_config {
		std::string title = "GSEngine Application";
		bool create_window = true;
		bool render = true;
		bool render_world = true;
		bool custom_chrome = false;
		bool scale_ui_with_resolution = true;
	};

	class engine : public identifiable {
	public:
		using setup_fn = std::function<void(engine&)>;

		engine(
			const engine_config& config
		);

		auto initialize(
			const setup_fn& app_setup = {}
		) -> void;

		auto update() -> void;

		auto render() -> void;

		auto shutdown() -> void;

		auto make_channel_writer() -> channel_writer;

		auto registry() -> gse::registry&;

		auto world() -> world_system::data&;

		auto window_should_close() -> bool;

		auto tick_window() -> void;

		template <typename S, typename... Args>
		auto add_system(
			Args&&... args
		) -> system_handle<S>;

	private:
		engine_config m_config;
		scheduler m_scheduler;
		save::registry m_save;
		primitives::data m_primitives;
		gse::registry m_registry;
		loading::state m_loading;
		std::function<void()> m_deferred_boot;
		std::atomic<bool> m_boot_tasks_done = false;
		std::uint32_t m_boot_init_baseline_settled = 0;
		bool m_boot_init_baseline_captured = false;
		std::uint32_t m_frames_since_rendered = 0;
		bool m_window_shown = false;
	};
}

template <typename S, typename... Args>
auto gse::engine::add_system(Args&&... args) -> system_handle<S> {
	return m_scheduler.add_system<S>(std::forward<Args>(args)...);
}

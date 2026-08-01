import std;

import gse;
import gse.config;
import gse.system_manifest;
import sandbox;

namespace sandbox::startup {
	struct config {
		gse::engine_config engine;
		bool use_gpu_solver = false;
		bool physics_parity = false;
		std::size_t physics_parity_envs = 1;
	};

	auto run_game(
		const gse::engine_config& engine
	) -> void;

	auto run_physics_parity(
		const config& cfg
	) -> void;

	// gse::network::run is a template over the game's replicated component set, so the manifest
	// needs its reflection handle rather than the template itself.
	template <typename... Components>
	consteval auto network_run_hook(gse::type_pack<Components...>) -> std::meta::info {
		return ^^gse::network::run<Components...>;
	}
}

auto sandbox::startup::run_game(const gse::engine_config& engine) -> void {
	gse::start(
		[](gse::engine& e) -> void {
			constexpr auto network_run = network_run_hook(networked_components{});
			static_assert(gse::meta::find_system_hook_anno(network_run) != std::meta::info{}, "network run hook annotation not visible on the templated instantiation");
			gse::system_manifest<^^gse::network::data, network_run, ^^gse::network::shutdown>{}.register_with(e);
			gse::system_manifest<^^crosshair::data, ^^crosshair::draw_settings>{}.register_with(e);
			gse::register_systems<^^sandbox::client_system>(e);
			gse::system_manifest<^^client_ui::data, ^^client_ui::run, ^^pause_menu::data, ^^pause_menu::run>{}.register_with(e);
			gse::system_manifest<^^dev_spawn::data, ^^dev_spawn::run>{}.register_with(e);
			gse::register_systems<^^gse::gui::popout_system>(e);
			world_loader_setup(e);
		},
		engine
	);
}

auto sandbox::startup::run_physics_parity(const config& cfg) -> void {
	gse::system_clock::set_fixed_step_override(1);
	gse::start(
		[n_envs = cfg.physics_parity_envs](gse::engine& e) -> void {
			gse::register_systems<^^sandbox::physics_parity>(e);
			auto* scene = physics_parity_world_setup(e, n_envs);
			if (scene) {
				gse::activate_scene(e.world(), scene->id());
			}
		},
		{
			.title = "Sandbox Physics Parity",
			.create_window = false,
			.render = false,
			.use_gpu_solver = cfg.use_gpu_solver,
			.persist_settings = false,
		}
	);
	gse::system_clock::set_fixed_step_override(std::nullopt);
}

auto main(int argc, char** argv) -> int {
	const auto cfg = gse::parse_args<sandbox::startup::config>(argc, argv);
	if (cfg.physics_parity) {
		sandbox::startup::run_physics_parity(cfg);
	}
	else {
		sandbox::startup::run_game(cfg.engine);
	}
	return 0;
}

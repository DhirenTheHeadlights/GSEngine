import std;

import gse;
import gse.config;
import gse.scenario;
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

	auto apply_scenario(
		config& cfg
	) -> bool;

	// gse::network::run is a template over the game's replicated component set, so the manifest
	// needs its reflection handle rather than the template itself.
	template <typename... Components>
	consteval auto network_run_hook(gse::type_pack<Components...>) -> std::meta::info {
		return ^^gse::network::run<Components...>;
	}
}

auto sandbox::startup::run_game(const gse::engine_config& engine) -> void {
	gse::engine_config resolved = engine;
	if (resolved.project_settings_path.empty()) {
		resolved.project_settings_path = gse::config::project_settings_path();
	}

	gse::start(
		[render = resolved.render](gse::engine& e) -> void {
			constexpr auto network_run = network_run_hook(networked_components{});
			static_assert(gse::meta::find_system_hook_anno(network_run) != std::meta::info{}, "network run hook annotation not visible on the templated instantiation");
			gse::system_manifest<^^gse::network::data, network_run, ^^gse::network::shutdown>{}.register_with(e);
			gse::register_systems<^^sandbox::client_system>(e);
			gse::system_manifest<^^dev_spawn::data, ^^dev_spawn::run>{}.register_with(e);
			if (render) {
				gse::system_manifest<^^crosshair::data, ^^crosshair::draw_settings>{}.register_with(e);
				gse::system_manifest<^^client_ui::data, ^^client_ui::run, ^^pause_menu::data, ^^pause_menu::run>{}.register_with(e);
				gse::register_systems<^^gse::gui::popout_system>(e);
			}
			world_loader_setup(e);
		},
		resolved
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

auto sandbox::startup::apply_scenario(config& cfg) -> bool {
	const auto table = gse::scenario::registry<^^sandbox::scenarios>();
	const auto* selected = gse::scenario::find(table, cfg.engine.bench.scenario);
	if (!selected) {
		std::cerr << std::format("unknown scenario '{}'; available:\n", cfg.engine.bench.scenario);
		for (const auto& entry : table) {
			std::cerr << std::format("  {}\n", std::string_view(entry.info.name));
		}
		return false;
	}

	const gse::bench_config defaults;
	auto& bench = cfg.engine.bench;

	bench.enabled = true;
	bench.scenario_body = selected->body;
	if (bench.scene == defaults.scene) {
		bench.scene = selected->info.scene;
	}
	if (bench.warmup_frames == defaults.warmup_frames) {
		bench.warmup_frames = selected->info.warmup_frames;
	}
	if (bench.frames == defaults.frames) {
		bench.frames = selected->info.frames;
	}
	if (selected->info.headless) {
		cfg.engine.create_window = false;
		cfg.engine.render = false;
	}
	cfg.engine.persist_settings = false;

	return true;
}

auto main(int argc, char** argv) -> int {
	auto cfg = gse::parse_args<sandbox::startup::config>(argc, argv);
	if (!cfg.engine.bench.scenario.empty()) {
		if (cfg.physics_parity) {
			std::cerr << "--engine-bench-scenario and --physics-parity select different run modes; pick one\n";
			return 1;
		}
		if (!sandbox::startup::apply_scenario(cfg)) {
			return 1;
		}
	}
	if (cfg.physics_parity) {
		sandbox::startup::run_physics_parity(cfg);
	}
	else {
		sandbox::startup::run_game(cfg.engine);
	}
	return 0;
}

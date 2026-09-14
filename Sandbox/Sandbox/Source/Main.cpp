import std;

import gse;
import gse.config;
import gse.scenario;
import gse.system_manifest;
import sandbox;

namespace sandbox::startup {
	auto run_game(
		const gse::engine_config& engine
	) -> void;

	auto run_physics_parity(
		const config& cfg
	) -> void;

	auto apply_scenario(
		config& cfg,
		std::span<const std::string> passed_flags
	) -> bool;
}

auto sandbox::startup::run_game(const gse::engine_config& engine) -> void {
	gse::engine_config resolved = engine;
	if (resolved.project_settings_path.empty()) {
		resolved.project_settings_path = gse::config::project_settings_path();
	}

	const gse::network::session_role mode = gse::network::resolve_role(resolved.net);

	if (mode == gse::network::session_role::dedicated) {
		resolved.create_window = false;
		resolved.render = false;
	}

	gse::start(
		[render = resolved.render, mode](gse::engine& e) -> void {
			if (mode == gse::network::session_role::dedicated) {
				server_setup(e);
				return;
			}

			gse::network_setup(e, networked_components{}, network_messages{});
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
			.use_gpu_solver = cfg.engine.use_gpu_solver,
			.persist_settings = false,
			.author_baked_assets = false,
		}
	);
	gse::system_clock::set_fixed_step_override(std::nullopt);
}

auto sandbox::startup::apply_scenario(config& cfg, const std::span<const std::string> passed_flags) -> bool {
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
	bench.real_time = selected->info.real_time;

	std::vector<std::string> owned;
	const auto own = [&owned](const std::string_view flag) -> void {
		owned.emplace_back(std::format("--engine-{}", flag));
		owned.emplace_back(std::format("--no-engine-{}", flag));
	};

	if (selected->info.headless) {
		cfg.engine.create_window = false;
		cfg.engine.render = false;
		own("create-window");
		own("render");
	}
	if (selected->info.gpu_solver) {
		cfg.engine.use_gpu_solver = true;
		own("use-gpu-solver");
	}
	cfg.engine.video_encode = selected->info.video_encode;
	own("video-encode");

	std::vector<std::string> pinned;
	pinned.push_back(std::format("Physics.use_gpu_solver={}", selected->info.gpu_solver));
	for (const auto& s : selected->info.settings) {
		if (s[0] != '\0') {
			pinned.emplace_back(s);
		}
	}
	cfg.engine.setting.insert(cfg.engine.setting.begin(), pinned.begin(), pinned.end());

	cfg.engine.load_settings = false;
	cfg.engine.persist_settings = false;
	cfg.engine.author_baked_assets = false;
	own("load-settings");
	own("persist-settings");
	own("author-baked-assets");

	std::vector<std::string_view> conflicts;
	for (const std::string& flag : passed_flags) {
		if (std::ranges::contains(owned, flag)) {
			conflicts.push_back(flag);
		}
	}

	if (!conflicts.empty()) {
		std::cerr << std::format(
			"error: scenario '{}' owns these flags, so a scenario run would ignore them:\n",
			std::string_view(selected->info.name)
		);
		for (const auto flag : conflicts) {
			std::cerr << std::format("  {}\n", flag);
		}
		std::cerr <<
			"a scenario run is hermetic: its gse::scenario::info annotation is the only source for them.\n"
			"drop the flag, edit the annotation, or use --engine-setting Section.key=value, which a scenario does not own.\n";
		return false;
	}

	return true;
}

auto main(int argc, char** argv) -> int {
	std::vector<std::string> passed_flags;
	auto cfg = gse::parse_args<sandbox::startup::config>(argc, argv, passed_flags);
	if (!cfg.scan_states.empty()) {
		return sandbox::startup::run_scan_states(cfg);
	}
	if (!cfg.compare_states_a.empty() || !cfg.compare_states_b.empty()) {
		if (cfg.compare_states_a.empty() || cfg.compare_states_b.empty()) {
			std::cerr << "--compare-states-a and --compare-states-b must both be given\n";
			return 1;
		}
		return sandbox::startup::run_compare_states(cfg);
	}
	if (!cfg.engine.bench.scenario.empty()) {
		if (cfg.physics_parity) {
			std::cerr << "--engine-bench-scenario and --physics-parity select different run modes; pick one\n";
			return 1;
		}
		if (!sandbox::startup::apply_scenario(cfg, passed_flags)) {
			return 1;
		}
		const auto args = std::span(argv, static_cast<std::size_t>(argc));
		if (std::ranges::contains(args, std::string_view("--engine-load-settings"), [](const char* a) { return std::string_view(a); })) {
			cfg.engine.load_settings = true;
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

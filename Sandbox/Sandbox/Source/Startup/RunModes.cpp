module sandbox:run_modes_impl;

import std;
import gse;
import gse.config;
import gse.scenario;
import gse.system_manifest;

import :game_systems;
import :net_setup;
import :parity_harness;
import :run_modes;
import :scenarios;
import :world_loader;

auto sandbox::startup::run_game(const gse::engine_config& engine) -> int {
	gse::engine_config resolved = engine;
	if (resolved.project_settings_path.empty()) {
		resolved.project_settings_path = gse::config::project_settings_path();
	}
	resolved.default_theme = gse::gui::theme::blueprint;

	const gse::network::session_role mode = gse::network::resolve_role(resolved.net);

	if (mode == gse::network::session_role::dedicated) {
		resolved.create_window = false;
		resolved.render = false;
	}

	return gse::start(
		[render = resolved.render, mode](gse::engine& e) -> void {
			if (mode == gse::network::session_role::dedicated) {
				server_setup(e);
				return;
			}

			gse::network_setup(e, networked_components{}, network_messages{});
			register_client_systems(e, render);
			world_loader_setup(e);
		},
		resolved
	);
}

auto sandbox::startup::run_physics_parity(const config& cfg) -> int {
	gse::system_clock::set_fixed_step_override(1);
	const int code = gse::start(
		[n_envs = cfg.physics_parity_envs](gse::engine& e) -> void {
			gse::system_manifest<^^physics_parity::data, ^^physics_parity::run>{}.register_with(e);
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
	return code;
}

auto sandbox::startup::apply_scenario(config& cfg, const std::span<const std::string> passed_flags) -> bool {
	const auto table = scenario_table::entries();
	const auto* selected = gse::scenario::find(table, cfg.engine.bench.scenario);
	if (!selected) {
		gse::log::println(gse::log::level::error, gse::log::category::general, "unknown scenario '{}'; available:", cfg.engine.bench.scenario);
		for (const auto& entry : table) {
			gse::log::println(gse::log::level::error, gse::log::category::general, "  {}", std::string_view(entry.info.name));
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

	cfg.engine.load_settings = cfg.engine.load_settings && std::ranges::contains(passed_flags, std::string_view("--engine-load-settings"));
	cfg.engine.persist_settings = false;
	cfg.engine.author_baked_assets = false;
	own("persist-settings");
	own("author-baked-assets");

	std::vector<std::string_view> conflicts;
	for (const std::string& flag : passed_flags) {
		if (std::ranges::contains(owned, flag)) {
			conflicts.push_back(flag);
		}
	}

	if (!conflicts.empty()) {
		gse::log::println(
			gse::log::level::error,
			gse::log::category::general,
			"error: scenario '{}' owns these flags, so a scenario run would ignore them:",
			std::string_view(selected->info.name)
		);
		for (const auto flag : conflicts) {
			gse::log::println(gse::log::level::error, gse::log::category::general, "  {}", flag);
		}
		gse::log::println(gse::log::level::error, gse::log::category::general, "a scenario run is hermetic: its gse::scenario::info annotation is the only source for them.");
		gse::log::println(gse::log::level::error, gse::log::category::general, "drop the flag, edit the annotation, or use --engine-setting Section.key=value, which a scenario does not own.");
		return false;
	}

	return true;
}

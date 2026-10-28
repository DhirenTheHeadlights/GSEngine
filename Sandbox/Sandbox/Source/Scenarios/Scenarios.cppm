export module sandbox:scenarios;

import std;

import gse;
import gse.scenario;

export namespace sandbox::scenarios {
	[[= gse::scenario::info{
		.name = "physics_stress",
		.scene = "Sandbox",
		.headless = true,
		.settings = {
			"Dev Spawn.stress.tumbler_radial_cubes=6",
			"Dev Spawn.stress.tumbler_axial_cubes=12",
		},
	}]]
	auto physics_stress(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "physics_stress_via_input",
		.scene = "Sandbox",
		.headless = true,
		.settings = {
			"Dev Spawn.stress.tumbler_radial_cubes=6",
			"Dev Spawn.stress.tumbler_axial_cubes=12",
		},
	}]]
	auto physics_stress_via_input(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "render_stress",
		.scene = "Sandbox",
		.settings = {
			"Window.present_mode=immediate",
			"Dev Spawn.stress.tumbler_radial_cubes=6",
			"Dev Spawn.stress.tumbler_axial_cubes=12",
		},
	}]]
	auto render_stress(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "sky_render_bench",
		.scene = "Sky",
		.warmup_frames = 0,
		.frames = 700,
		.settings = {
			"Window.present_mode=immediate",
			"Graphics.enabled=false",
			"Clouds.cloud_coverage=0.45",
			"Grid.fade_distance=14000 m",
			"Grid.show_labels=false",
			"Grid.minor_spacing=50 m",
			"Grid.major_spacing=500 m",
		},
	}]]
	auto sky_render_bench(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "light_hall_bench",
		.scene = "LightHall",
		.warmup_frames = 0,
		.frames = 700,
		.settings = {
			"Window.present_mode=immediate",
			"Graphics.enabled=false",
			"Grid.enabled=false",
			"Dev Spawn.lights.clusters=26",
			"Dev Spawn.lights.lights_per_cluster=14",
			"Dev Spawn.lights.cluster_radius=4 m",
			"Dev Spawn.lights.field_radius=52 m",
			"Dev Spawn.lights.field_height=9 m",
			"Dev Spawn.lights.intensity=16 W/m^2",
			"Dev Spawn.lights.falloff_radius=36 m",
			"Dev Spawn.lights.drift=0 m",
		},
	}]]
	auto light_hall_bench(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "carousel_bench",
		.scene = "Carousel",
		.headless = true,
		.warmup_frames = 0,
		.frames = 300,
	}]]
	auto carousel_bench(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "machines_bench",
		.scene = "Machines",
		.headless = true,
		.warmup_frames = 0,
		.frames = 400,
	}]]
	auto machines_bench(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "dominoes_bench",
		.scene = "Dominoes",
		.headless = true,
		.warmup_frames = 0,
		.frames = 300,
	}]]
	auto dominoes_bench(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "swings_bench",
		.scene = "Swings",
		.headless = true,
		.warmup_frames = 0,
		.frames = 400,
	}]]
	auto swings_bench(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "record_clip",
		.scene = "Sandbox",
		.video_encode = true,
		.warmup_frames = 0,
		.frames = 420,
		.settings = {
			"Window.present_mode=immediate",
			"Graphics.enabled=false",
		},
	}]]
	auto record_clip(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "solver_showcase",
		.scene = "Pyramid",
		.video_encode = true,
		.warmup_frames = 0,
		.frames = 780,
		.settings = {
			"Window.present_mode=immediate",
			"Dev Spawn.pyramid.base_count=60",
			"Physics.velocity_sleep_threshold=0 m/s",
			"Graphics.enabled=false",
		},
	}]]
	auto solver_showcase(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "atmosphere_showcase",
		.scene = "Sky",
		.video_encode = true,
		.warmup_frames = 0,
		.frames = 700,
		.settings = {
			"Window.present_mode=immediate",
			"Graphics.enabled=false",
			"Clouds.cloud_coverage=0.45",
			"Grid.fade_distance=14000 m",
			"Grid.show_labels=false",
			"Grid.minor_spacing=50 m",
			"Grid.major_spacing=500 m",
		},
	}]]
	auto atmosphere_showcase(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "cloud_showcase",
		.scene = "Sky",
		.video_encode = true,
		.warmup_frames = 0,
		.frames = 1150,
		.settings = {
			"Window.present_mode=immediate",
			"Graphics.enabled=false",
			"Clouds.cloud_coverage=0.50",
			"Clouds.weather_contrast=2.6",
			"Clouds.cloud_type=0.90",
			"Clouds.shadow_strength=1.0",
			"Grid.show_labels=false",
			"Grid.fade_distance=1200 m",
		},
	}]]
	auto cloud_showcase(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "tumbler_showcase",
		.scene = "Tumblers",
		.gpu_solver = true,
		.video_encode = true,
		.warmup_frames = 0,
		.frames = 1150,
		.settings = {
			"Window.present_mode=immediate",
			"Graphics.enabled=false",
			"Clouds.cloud_coverage=0.40",
			"Clouds.weather_contrast=3.0",
			"Clouds.shadow_strength=1.0",
			"Grid.show_labels=false",
			"Grid.fade_distance=400 m",
		},
	}]]
	auto tumbler_showcase(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "sunset_showcase",
		.scene = "Sandbox",
		.video_encode = true,
		.warmup_frames = 0,
		.frames = 1700,
		.settings = {
			"Window.present_mode=immediate",
			"Graphics.enabled=false",
			"Clouds.cloud_coverage=0.40",
			"Clouds.weather_contrast=3.0",
			"Clouds.cloud_type=0.85",
			"Clouds.shadow_strength=1.0",
			"Grid.fade_distance=2000 m",
			"Grid.show_labels=false",
		},
	}]]
	auto sunset_showcase(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "lighting_showcase",
		.scene = "Pyramid",
		.video_encode = true,
		.warmup_frames = 0,
		.frames = 660,
		.settings = {
			"Window.present_mode=immediate",
			"Dev Spawn.pyramid.base_count=34",
			"Graphics.enabled=false",
			"Atmosphere.sun_elevation=-8 deg",
		},
	}]]
	auto lighting_showcase(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "light_hall_showcase",
		.scene = "LightHall",
		.video_encode = true,
		.warmup_frames = 0,
		.frames = 900,
		.settings = {
			"Window.present_mode=immediate",
			"Graphics.enabled=false",
			"Grid.enabled=false",
			"Dev Spawn.lights.clusters=26",
			"Dev Spawn.lights.lights_per_cluster=14",
			"Dev Spawn.lights.cluster_radius=4 m",
			"Dev Spawn.lights.field_radius=52 m",
			"Dev Spawn.lights.field_height=9 m",
			"Dev Spawn.lights.intensity=16 W/m^2",
			"Dev Spawn.lights.falloff_radius=36 m",
			"Dev Spawn.lights.drift=5 m",
		},
	}]]
	auto light_hall_showcase(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "carousel_showcase",
		.scene = "Carousel",
		.gpu_solver = true,
		.video_encode = true,
		.warmup_frames = 0,
		.frames = 1020,
		.settings = {
			"Window.present_mode=immediate",
			"Graphics.enabled=false",
			"Clouds.cloud_coverage=0.40",
			"Grid.show_labels=false",
			"Grid.fade_distance=400 m",
		},
	}]]
	auto carousel_showcase(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "dominoes_showcase",
		.scene = "Dominoes",
		.video_encode = true,
		.warmup_frames = 0,
		.frames = 520,
		.settings = {
			"Window.present_mode=immediate",
			"Graphics.enabled=false",
			"Grid.show_labels=false",
			"Grid.fade_distance=400 m",
		},
	}]]
	auto dominoes_showcase(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "sky_horizon_probe",
		.scene = "Sky",
		.warmup_frames = 0,
		.frames = 1080,
		.settings = {
			"Window.present_mode=immediate",
			"Graphics.enabled=false",
			"Grid.enabled=false",
			"Clouds.cloud_coverage=0.45",
			"Graphics.reflection_quality=high",
			"Graphics.adaptation_time_bright=0.05 s",
			"Graphics.adaptation_time_dark=0.05 s",
		},
	}]]
	auto sky_horizon_probe(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "hall_horizon_probe",
		.scene = "LightHall",
		.warmup_frames = 0,
		.frames = 1080,
		.settings = {
			"Window.present_mode=immediate",
			"Graphics.enabled=false",
			"Grid.enabled=false",
			"Dev Spawn.lights.clusters=26",
			"Dev Spawn.lights.lights_per_cluster=14",
			"Dev Spawn.lights.cluster_radius=4 m",
			"Dev Spawn.lights.field_radius=52 m",
			"Dev Spawn.lights.field_height=9 m",
			"Dev Spawn.lights.intensity=16 W/m^2",
			"Dev Spawn.lights.falloff_radius=36 m",
			"Dev Spawn.lights.drift=0 m",
			"Graphics.adaptation_time_bright=0.05 s",
			"Graphics.adaptation_time_dark=0.05 s",
		},
	}]]
	auto hall_horizon_probe(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "scene_reset",
		.scene = "Pyramid",
		.headless = true,
		.gpu_solver = true,
		.warmup_frames = 0,
		.frames = 240,
		.settings = {
			"Dev Spawn.pyramid.base_count=12",
			"Dev Spawn.strike.row_begin=4",
			"Dev Spawn.strike.rows=3",
		},
	}]]
	auto scene_reset(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "scene_reset_cpu",
		.scene = "Pyramid",
		.headless = true,
		.warmup_frames = 0,
		.frames = 240,
		.settings = {
			"Dev Spawn.pyramid.base_count=12",
			"Dev Spawn.strike.row_begin=4",
			"Dev Spawn.strike.rows=3",
		},
	}]]
	auto scene_reset_cpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "relaunch_probe",
		.headless = true,
		.warmup_frames = 0,
		.frames = 10,
	}]]
	auto relaunch_probe(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "locomotion",
		.scene = "Sandbox",
		.headless = true,
		.settings = {
			"Dev Spawn.characters.count=8",
		},
	}]]
	auto locomotion(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "pyramid",
		.scene = "Pyramid",
		.headless = true,
		.backend_variants = true,
		.warmup_frames = 0,
	}]]
	auto pyramid(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "pyramid16k_cpu",
		.scene = "Pyramid",
		.headless = true,
		.warmup_frames = 0,
		.settings = {
			"Dev Spawn.pyramid.base_count=180",
			"Physics.solver_iterations=40",
		},
	}]]
	auto pyramid16k_cpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "net_walk",
		.headless = false,
		.backend_variants = true,
		.real_time = true,
		.warmup_frames = 0,
		.frames = 2400,
	}]]
	auto net_walk(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "rollback_reference",
		.scene = "Pyramid",
		.headless = true,
		.gpu_solver = false,
		.warmup_frames = 0,
		.frames = 120,
		.settings = {
			"Dev Spawn.pyramid.base_count=20",
			"Physics.rollback_history_steps=90",
		},
	}]]
	auto rollback_reference(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "rollback_replay",
		.scene = "Pyramid",
		.headless = true,
		.gpu_solver = false,
		.warmup_frames = 0,
		.frames = 120,
		.settings = {
			"Dev Spawn.pyramid.base_count=20",
			"Physics.rollback_history_steps=90",
		},
	}]]
	auto rollback_replay(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "rollback_reference_gpu",
		.scene = "Pyramid",
		.headless = true,
		.gpu_solver = true,
		.warmup_frames = 0,
		.frames = 120,
		.settings = {
			"Dev Spawn.pyramid.base_count=20",
			"Physics.rollback_history_steps=30",
		},
	}]]
	auto rollback_reference_gpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "rollback_replay_gpu",
		.scene = "Pyramid",
		.headless = true,
		.gpu_solver = true,
		.warmup_frames = 0,
		.frames = 120,
		.settings = {
			"Dev Spawn.pyramid.base_count=20",
			"Physics.rollback_history_steps=30",
		},
	}]]
	auto rollback_replay_gpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "rollback_impulse_reference",
		.scene = "Pyramid",
		.headless = true,
		.gpu_solver = false,
		.warmup_frames = 0,
		.frames = 120,
		.settings = {
			"Dev Spawn.pyramid.base_count=20",
			"Physics.rollback_history_steps=90",
		},
	}]]
	auto rollback_impulse_reference(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "rollback_impulse_replay",
		.scene = "Pyramid",
		.headless = true,
		.gpu_solver = false,
		.warmup_frames = 0,
		.frames = 120,
		.settings = {
			"Dev Spawn.pyramid.base_count=20",
			"Physics.rollback_history_steps=90",
		},
	}]]
	auto rollback_impulse_replay(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "rollback_impulse_reference_gpu",
		.scene = "Pyramid",
		.headless = true,
		.gpu_solver = true,
		.warmup_frames = 0,
		.frames = 120,
		.settings = {
			"Dev Spawn.pyramid.base_count=20",
			"Physics.rollback_history_steps=30",
		},
	}]]
	auto rollback_impulse_reference_gpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "rollback_impulse_replay_gpu",
		.scene = "Pyramid",
		.headless = true,
		.gpu_solver = true,
		.warmup_frames = 0,
		.frames = 120,
		.settings = {
			"Dev Spawn.pyramid.base_count=20",
			"Physics.rollback_history_steps=30",
		},
	}]]
	auto rollback_impulse_replay_gpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "rollback_reference_character",
		.scene = "Sandbox",
		.headless = true,
		.gpu_solver = false,
		.warmup_frames = 0,
		.frames = 600,
		.settings = {
			"Dev Spawn.characters.count=2",
			"Physics.rollback_history_steps=90",
		},
	}]]
	auto rollback_reference_character(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "rollback_replay_character",
		.scene = "Sandbox",
		.headless = true,
		.gpu_solver = false,
		.warmup_frames = 0,
		.frames = 600,
		.settings = {
			"Dev Spawn.characters.count=2",
			"Physics.rollback_history_steps=90",
		},
	}]]
	auto rollback_replay_character(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "rollback_reference_stress",
		.scene = "Sandbox",
		.headless = true,
		.gpu_solver = false,
		.warmup_frames = 0,
		.frames = 240,
		.settings = {
			"Dev Spawn.stress.tumbler_radial_cubes=6",
			"Dev Spawn.stress.tumbler_axial_cubes=12",
			"Physics.rollback_history_steps=90",
		},
	}]]
	auto rollback_reference_stress(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "rollback_replay_stress",
		.scene = "Sandbox",
		.headless = true,
		.gpu_solver = false,
		.warmup_frames = 0,
		.frames = 240,
		.settings = {
			"Dev Spawn.stress.tumbler_radial_cubes=6",
			"Dev Spawn.stress.tumbler_axial_cubes=12",
			"Physics.rollback_history_steps=90",
		},
	}]]
	auto rollback_replay_stress(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "parity_stress_cpu",
		.scene = "Sandbox",
		.headless = true,
	}]]
	auto parity_stress_cpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "parity_stress_gpu",
		.scene = "Sandbox",
		.headless = true,
		.gpu_solver = true,
		.settings = {
			"Dev Spawn.stress.tumbler_radial_cubes=6",
			"Dev Spawn.stress.tumbler_axial_cubes=12",
		},
	}]]
	auto parity_stress_gpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "parity_nojoints_gpu",
		.scene = "Sandbox",
		.headless = true,
		.gpu_solver = true,
		.warmup_frames = 0,
	}]]
	auto parity_nojoints_gpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "parity_empty_gpu",
		.scene = "Sandbox",
		.headless = true,
		.gpu_solver = true,
		.warmup_frames = 0,
	}]]
	auto parity_empty_gpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "parity_jointsonly",
		.scene = "Sandbox",
		.headless = true,
		.backend_variants = true,
		.warmup_frames = 0,
	}]]
	auto parity_jointsonly(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "parity_drop",
		.scene = "ParityDrop",
		.headless = true,
		.backend_variants = true,
		.warmup_frames = 0,
	}]]
	auto parity_drop(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "parity_pair",
		.scene = "ParityPair",
		.headless = true,
		.backend_variants = true,
		.warmup_frames = 0,
	}]]
	auto parity_pair(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "parity_stack",
		.scene = "ParityStack",
		.headless = true,
		.backend_variants = true,
		.warmup_frames = 0,
	}]]
	auto parity_stack(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "parity_cluster",
		.scene = "ParityCluster",
		.headless = true,
		.backend_variants = true,
		.warmup_frames = 0,
	}]]
	auto parity_cluster(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "parity_heap",
		.scene = "ParityHeap",
		.headless = true,
		.backend_variants = true,
		.warmup_frames = 0,
	}]]
	auto parity_heap(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "parity_mound",
		.scene = "ParityMound",
		.headless = true,
		.backend_variants = true,
		.warmup_frames = 0,
	}]]
	auto parity_mound(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "parity_pile",
		.scene = "ParityPile",
		.headless = true,
		.backend_variants = true,
		.warmup_frames = 0,
	}]]
	auto parity_pile(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "parity_hull_pile",
		.scene = "ParityHullPile",
		.headless = true,
		.backend_variants = true,
		.warmup_frames = 0,
	}]]
	auto parity_hull_pile(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "parity_overlap",
		.scene = "ParityOverlap",
		.headless = true,
		.backend_variants = true,
		.warmup_frames = 0,
	}]]
	auto parity_overlap(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "parity_shapes",
		.scene = "ParityShapes",
		.headless = true,
		.backend_variants = true,
		.warmup_frames = 0,
	}]]
	auto parity_shapes(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "parity_domino",
		.scene = "ParityDomino",
		.headless = true,
		.backend_variants = true,
		.warmup_frames = 0,
	}]]
	auto parity_domino(
		gse::scenario::context& ctx
	) -> gse::async::task<>;
}

export namespace sandbox::scenario_table {
	auto entries(
	) -> std::span<const gse::scenario::entry>;
}

auto sandbox::scenario_table::entries() -> std::span<const gse::scenario::entry> {
	return gse::scenario::registry<^^sandbox::scenarios>();
}

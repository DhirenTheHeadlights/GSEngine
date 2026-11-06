export module sandbox:scenarios;

import std;

import gse;
import gse.gpu_backend;
import gse.scenario;

import :dev_spawn_system;
import :spawn_params;

namespace sandbox::scenario_settings {
	using window = gse::window::data;
	using physics_debug = gse::renderer::physics_debug::data;
	using forward = gse::renderer::forward::data;
	using tonemap = gse::renderer::tonemap::data;
	using clouds = gse::renderer::cloud::data;
	using grid = gse::renderer::sdf_grid::data;
	using atmosphere = gse::renderer::atmosphere::data;
	using physics = gse::physics::data;
	using dev_spawn = sandbox::dev_spawn::data;
}

namespace ss = sandbox::scenario_settings;

export namespace sandbox::scenarios {
	[[= gse::scenario::info{
		.name = "physics_stress",
		.scene = "Sandbox",
		.headless = true,
	},
		= gse::scenario::pin<^^ss::dev_spawn::stress, ^^stress_scene_params::tumbler_radial_cubes>{ 6 },
		= gse::scenario::pin<^^ss::dev_spawn::stress, ^^stress_scene_params::tumbler_axial_cubes>{ 12 }
	]]
	auto physics_stress(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "physics_stress_via_input",
		.scene = "Sandbox",
		.headless = true,
	},
		= gse::scenario::pin<^^ss::dev_spawn::stress, ^^stress_scene_params::tumbler_radial_cubes>{ 6 },
		= gse::scenario::pin<^^ss::dev_spawn::stress, ^^stress_scene_params::tumbler_axial_cubes>{ 12 }
	]]
	auto physics_stress_via_input(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "physics_grow",
		.scene = "Sandbox",
		.headless = true,
		.gpu_solver = true,
	},
		= gse::scenario::pin<^^ss::dev_spawn::stress, ^^stress_scene_params::tumbler_radial_cubes>{ 8 },
		= gse::scenario::pin<^^ss::dev_spawn::stress, ^^stress_scene_params::tumbler_axial_cubes>{ 16 },
		= gse::scenario::pin<^^ss::dev_spawn::pyramid, ^^pyramid_scene_params::base_count>{ 70 }
	]]
	auto physics_grow(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "grass_sleep_bench",
		.scene = "Grass",
		.headless = true,
		.gpu_solver = true,
		.warmup_frames = 300,
		.frames = 600,
	}]]
	auto grass_sleep_bench(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "render_stress",
		.scene = "Sandbox",
	},
		= gse::scenario::pin<^^ss::window::present_mode>{ gse::gpu::present_mode::immediate },
		= gse::scenario::pin<^^ss::dev_spawn::stress, ^^stress_scene_params::tumbler_radial_cubes>{ 6 },
		= gse::scenario::pin<^^ss::dev_spawn::stress, ^^stress_scene_params::tumbler_axial_cubes>{ 12 }
	]]
	auto render_stress(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "sky_render_bench",
		.scene = "Sky",
		.warmup_frames = 0,
		.frames = 700,
	},
		= gse::scenario::pin<^^ss::window::present_mode>{ gse::gpu::present_mode::immediate },
		= gse::scenario::pin<^^ss::physics_debug::enabled>{ false },
		= gse::scenario::pin<^^ss::clouds::cloud_coverage>{ 0.45f },
		= gse::scenario::pin<^^ss::grid::fade_distance>{ gse::meters(14000.f) },
		= gse::scenario::pin<^^ss::grid::show_labels>{ false },
		= gse::scenario::pin<^^ss::grid::minor_spacing>{ gse::meters(50.f) },
		= gse::scenario::pin<^^ss::grid::major_spacing>{ gse::meters(500.f) }
	]]
	auto sky_render_bench(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "light_hall_bench",
		.scene = "LightHall",
		.warmup_frames = 0,
		.frames = 700,
	},
		= gse::scenario::pin<^^ss::window::present_mode>{ gse::gpu::present_mode::immediate },
		= gse::scenario::pin<^^ss::physics_debug::enabled>{ false },
		= gse::scenario::pin<^^ss::grid::enabled>{ false },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::clusters>{ 26 },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::lights_per_cluster>{ 14 },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::cluster_radius>{ gse::meters(4.f) },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::field_radius>{ gse::meters(52.f) },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::field_height>{ gse::meters(9.f) },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::intensity>{ gse::watts_per_square_meter(16.f) },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::falloff_radius>{ gse::meters(36.f) },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::drift>{ gse::meters(0.f) }
	]]
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
	},
		= gse::scenario::pin<^^ss::window::present_mode>{ gse::gpu::present_mode::immediate },
		= gse::scenario::pin<^^ss::physics_debug::enabled>{ false }
	]]
	auto record_clip(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "solver_showcase",
		.scene = "Pyramid",
		.video_encode = true,
		.warmup_frames = 0,
		.frames = 780,
	},
		= gse::scenario::pin<^^ss::window::present_mode>{ gse::gpu::present_mode::immediate },
		= gse::scenario::pin<^^ss::dev_spawn::pyramid, ^^pyramid_scene_params::base_count>{ 60 },
		= gse::scenario::pin<^^ss::physics::velocity_sleep_threshold>{ gse::meters_per_second(0.f) },
		= gse::scenario::pin<^^ss::physics_debug::enabled>{ false }
	]]
	auto solver_showcase(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "atmosphere_showcase",
		.scene = "Sky",
		.video_encode = true,
		.warmup_frames = 0,
		.frames = 700,
	},
		= gse::scenario::pin<^^ss::window::present_mode>{ gse::gpu::present_mode::immediate },
		= gse::scenario::pin<^^ss::physics_debug::enabled>{ false },
		= gse::scenario::pin<^^ss::clouds::cloud_coverage>{ 0.45f },
		= gse::scenario::pin<^^ss::grid::fade_distance>{ gse::meters(14000.f) },
		= gse::scenario::pin<^^ss::grid::show_labels>{ false },
		= gse::scenario::pin<^^ss::grid::minor_spacing>{ gse::meters(50.f) },
		= gse::scenario::pin<^^ss::grid::major_spacing>{ gse::meters(500.f) }
	]]
	auto atmosphere_showcase(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "cloud_showcase",
		.scene = "Sky",
		.video_encode = true,
		.warmup_frames = 0,
		.frames = 1150,
	},
		= gse::scenario::pin<^^ss::window::present_mode>{ gse::gpu::present_mode::immediate },
		= gse::scenario::pin<^^ss::physics_debug::enabled>{ false },
		= gse::scenario::pin<^^ss::clouds::cloud_coverage>{ 0.50f },
		= gse::scenario::pin<^^ss::clouds::weather_contrast>{ 2.6f },
		= gse::scenario::pin<^^ss::clouds::cloud_type>{ 0.90f },
		= gse::scenario::pin<^^ss::clouds::shadow_strength>{ 1.0f },
		= gse::scenario::pin<^^ss::grid::show_labels>{ false },
		= gse::scenario::pin<^^ss::grid::fade_distance>{ gse::meters(1200.f) }
	]]
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
	},
		= gse::scenario::pin<^^ss::window::present_mode>{ gse::gpu::present_mode::immediate },
		= gse::scenario::pin<^^ss::physics_debug::enabled>{ false },
		= gse::scenario::pin<^^ss::clouds::cloud_coverage>{ 0.40f },
		= gse::scenario::pin<^^ss::clouds::weather_contrast>{ 3.0f },
		= gse::scenario::pin<^^ss::clouds::shadow_strength>{ 1.0f },
		= gse::scenario::pin<^^ss::grid::show_labels>{ false },
		= gse::scenario::pin<^^ss::grid::fade_distance>{ gse::meters(400.f) }
	]]
	auto tumbler_showcase(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "sunset_showcase",
		.scene = "Sandbox",
		.video_encode = true,
		.warmup_frames = 0,
		.frames = 1700,
	},
		= gse::scenario::pin<^^ss::window::present_mode>{ gse::gpu::present_mode::immediate },
		= gse::scenario::pin<^^ss::physics_debug::enabled>{ false },
		= gse::scenario::pin<^^ss::clouds::cloud_coverage>{ 0.40f },
		= gse::scenario::pin<^^ss::clouds::weather_contrast>{ 3.0f },
		= gse::scenario::pin<^^ss::clouds::cloud_type>{ 0.85f },
		= gse::scenario::pin<^^ss::clouds::shadow_strength>{ 1.0f },
		= gse::scenario::pin<^^ss::grid::fade_distance>{ gse::meters(2000.f) },
		= gse::scenario::pin<^^ss::grid::show_labels>{ false }
	]]
	auto sunset_showcase(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "lighting_showcase",
		.scene = "Pyramid",
		.video_encode = true,
		.warmup_frames = 0,
		.frames = 660,
	},
		= gse::scenario::pin<^^ss::window::present_mode>{ gse::gpu::present_mode::immediate },
		= gse::scenario::pin<^^ss::dev_spawn::pyramid, ^^pyramid_scene_params::base_count>{ 34 },
		= gse::scenario::pin<^^ss::physics_debug::enabled>{ false },
		= gse::scenario::pin<^^ss::atmosphere::sun_elevation>{ gse::degrees(-8.f) }
	]]
	auto lighting_showcase(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "lighting_parity",
		.scene = "Pyramid",
		.warmup_frames = 0,
		.frames = 480,
	},
		= gse::scenario::pin<^^ss::window::present_mode>{ gse::gpu::present_mode::immediate },
		= gse::scenario::pin<^^ss::dev_spawn::pyramid, ^^pyramid_scene_params::base_count>{ 34 },
		= gse::scenario::pin<^^ss::physics_debug::enabled>{ false },
		= gse::scenario::pin<^^ss::atmosphere::sun_elevation>{ gse::degrees(25.f) }
	]]
	auto lighting_parity(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "textured_parity",
		.scene = "ParityTextured",
		.warmup_frames = 0,
		.frames = 480,
	},
		= gse::scenario::pin<^^ss::window::present_mode>{ gse::gpu::present_mode::immediate },
		= gse::scenario::pin<^^ss::physics_debug::enabled>{ false },
		= gse::scenario::pin<^^ss::atmosphere::sun_elevation>{ gse::degrees(25.f) }
	]]
	auto textured_parity(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "light_hall_showcase",
		.scene = "LightHall",
		.video_encode = true,
		.warmup_frames = 0,
		.frames = 900,
	},
		= gse::scenario::pin<^^ss::window::present_mode>{ gse::gpu::present_mode::immediate },
		= gse::scenario::pin<^^ss::physics_debug::enabled>{ false },
		= gse::scenario::pin<^^ss::grid::enabled>{ false },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::clusters>{ 26 },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::lights_per_cluster>{ 14 },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::cluster_radius>{ gse::meters(4.f) },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::field_radius>{ gse::meters(52.f) },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::field_height>{ gse::meters(9.f) },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::intensity>{ gse::watts_per_square_meter(16.f) },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::falloff_radius>{ gse::meters(36.f) },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::drift>{ gse::meters(5.f) }
	]]
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
	},
		= gse::scenario::pin<^^ss::window::present_mode>{ gse::gpu::present_mode::immediate },
		= gse::scenario::pin<^^ss::physics_debug::enabled>{ false },
		= gse::scenario::pin<^^ss::clouds::cloud_coverage>{ 0.40f },
		= gse::scenario::pin<^^ss::grid::show_labels>{ false },
		= gse::scenario::pin<^^ss::grid::fade_distance>{ gse::meters(400.f) }
	]]
	auto carousel_showcase(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "dominoes_showcase",
		.scene = "Dominoes",
		.video_encode = true,
		.warmup_frames = 0,
		.frames = 520,
	},
		= gse::scenario::pin<^^ss::window::present_mode>{ gse::gpu::present_mode::immediate },
		= gse::scenario::pin<^^ss::physics_debug::enabled>{ false },
		= gse::scenario::pin<^^ss::grid::show_labels>{ false },
		= gse::scenario::pin<^^ss::grid::fade_distance>{ gse::meters(400.f) }
	]]
	auto dominoes_showcase(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "sky_horizon_probe",
		.scene = "Sky",
		.warmup_frames = 0,
		.frames = 1080,
	},
		= gse::scenario::pin<^^ss::window::present_mode>{ gse::gpu::present_mode::immediate },
		= gse::scenario::pin<^^ss::physics_debug::enabled>{ false },
		= gse::scenario::pin<^^ss::grid::enabled>{ false },
		= gse::scenario::pin<^^ss::clouds::cloud_coverage>{ 0.45f },
		= gse::scenario::pin<^^ss::forward::reflection_quality>{ gse::renderer::forward::reflection_quality_level::high },
		= gse::scenario::pin<^^ss::tonemap::adaptation_time_bright>{ gse::seconds(0.05f) },
		= gse::scenario::pin<^^ss::tonemap::adaptation_time_dark>{ gse::seconds(0.05f) }
	]]
	auto sky_horizon_probe(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "hall_horizon_probe",
		.scene = "LightHall",
		.warmup_frames = 0,
		.frames = 1080,
	},
		= gse::scenario::pin<^^ss::window::present_mode>{ gse::gpu::present_mode::immediate },
		= gse::scenario::pin<^^ss::physics_debug::enabled>{ false },
		= gse::scenario::pin<^^ss::grid::enabled>{ false },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::clusters>{ 26 },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::lights_per_cluster>{ 14 },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::cluster_radius>{ gse::meters(4.f) },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::field_radius>{ gse::meters(52.f) },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::field_height>{ gse::meters(9.f) },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::intensity>{ gse::watts_per_square_meter(16.f) },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::falloff_radius>{ gse::meters(36.f) },
		= gse::scenario::pin<^^ss::dev_spawn::lights, ^^light_field_params::drift>{ gse::meters(0.f) },
		= gse::scenario::pin<^^ss::tonemap::adaptation_time_bright>{ gse::seconds(0.05f) },
		= gse::scenario::pin<^^ss::tonemap::adaptation_time_dark>{ gse::seconds(0.05f) }
	]]
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
	},
		= gse::scenario::pin<^^ss::dev_spawn::pyramid, ^^pyramid_scene_params::base_count>{ 12 },
		= gse::scenario::pin<^^ss::dev_spawn::strike, ^^pyramid_strike_params::row_begin>{ 4 },
		= gse::scenario::pin<^^ss::dev_spawn::strike, ^^pyramid_strike_params::rows>{ 3 }
	]]
	auto scene_reset(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "scene_reset_cpu",
		.scene = "Pyramid",
		.headless = true,
		.warmup_frames = 0,
		.frames = 240,
	},
		= gse::scenario::pin<^^ss::dev_spawn::pyramid, ^^pyramid_scene_params::base_count>{ 12 },
		= gse::scenario::pin<^^ss::dev_spawn::strike, ^^pyramid_strike_params::row_begin>{ 4 },
		= gse::scenario::pin<^^ss::dev_spawn::strike, ^^pyramid_strike_params::rows>{ 3 }
	]]
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
		.name = "web_host_probe",
		.real_time = true,
		.warmup_frames = 0,
		.frames = 600,
	}]]
	auto web_host_probe(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "web_idle_probe",
		.real_time = true,
		.warmup_frames = 0,
		.frames = 80,
	}]]
	auto web_idle_probe(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "web_pointer_probe",
		.real_time = true,
		.warmup_frames = 0,
		.frames = 300,
	}]]
	auto web_pointer_probe(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "web_persist_write",
		.headless = true,
		.warmup_frames = 0,
		.frames = 10,
	}]]
	auto web_persist_write(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "web_persist_read",
		.headless = true,
		.warmup_frames = 0,
		.frames = 10,
	}]]
	auto web_persist_read(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "locomotion",
		.scene = "Sandbox",
		.headless = true,
	},
		= gse::scenario::pin<^^ss::dev_spawn::characters, ^^character_spawn_params::count>{ 8 }
	]]
	auto locomotion(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "character_gpu_bench",
		.scene = "Sandbox",
		.gpu_solver = true,
		.warmup_frames = 300,
		.frames = 600,
	},
		= gse::scenario::pin<^^ss::window::present_mode>{ gse::gpu::present_mode::immediate }
	]]
	auto character_gpu_bench(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{
		.name = "character_gpu_bench_headless",
		.scene = "Sandbox",
		.headless = true,
		.gpu_solver = true,
		.warmup_frames = 300,
		.frames = 600,
	}]]
	auto character_gpu_bench_headless(
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
	},
		= gse::scenario::pin<^^ss::dev_spawn::pyramid, ^^pyramid_scene_params::base_count>{ 180 },
		= gse::scenario::pin<^^ss::physics::solver_iterations>{ 40 }
	]]
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
	},
		= gse::scenario::pin<^^ss::dev_spawn::pyramid, ^^pyramid_scene_params::base_count>{ 20 },
		= gse::scenario::pin<^^ss::physics::rollback_history_steps>{ 90 }
	]]
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
	},
		= gse::scenario::pin<^^ss::dev_spawn::pyramid, ^^pyramid_scene_params::base_count>{ 20 },
		= gse::scenario::pin<^^ss::physics::rollback_history_steps>{ 90 }
	]]
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
	},
		= gse::scenario::pin<^^ss::dev_spawn::pyramid, ^^pyramid_scene_params::base_count>{ 20 },
		= gse::scenario::pin<^^ss::physics::rollback_history_steps>{ 30 }
	]]
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
	},
		= gse::scenario::pin<^^ss::dev_spawn::pyramid, ^^pyramid_scene_params::base_count>{ 20 },
		= gse::scenario::pin<^^ss::physics::rollback_history_steps>{ 30 }
	]]
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
	},
		= gse::scenario::pin<^^ss::dev_spawn::pyramid, ^^pyramid_scene_params::base_count>{ 20 },
		= gse::scenario::pin<^^ss::physics::rollback_history_steps>{ 90 }
	]]
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
	},
		= gse::scenario::pin<^^ss::dev_spawn::pyramid, ^^pyramid_scene_params::base_count>{ 20 },
		= gse::scenario::pin<^^ss::physics::rollback_history_steps>{ 90 }
	]]
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
	},
		= gse::scenario::pin<^^ss::dev_spawn::pyramid, ^^pyramid_scene_params::base_count>{ 20 },
		= gse::scenario::pin<^^ss::physics::rollback_history_steps>{ 30 }
	]]
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
	},
		= gse::scenario::pin<^^ss::dev_spawn::pyramid, ^^pyramid_scene_params::base_count>{ 20 },
		= gse::scenario::pin<^^ss::physics::rollback_history_steps>{ 30 }
	]]
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
	},
		= gse::scenario::pin<^^ss::dev_spawn::characters, ^^character_spawn_params::count>{ 2 },
		= gse::scenario::pin<^^ss::physics::rollback_history_steps>{ 90 }
	]]
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
	},
		= gse::scenario::pin<^^ss::dev_spawn::characters, ^^character_spawn_params::count>{ 2 },
		= gse::scenario::pin<^^ss::physics::rollback_history_steps>{ 90 }
	]]
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
	},
		= gse::scenario::pin<^^ss::dev_spawn::stress, ^^stress_scene_params::tumbler_radial_cubes>{ 6 },
		= gse::scenario::pin<^^ss::dev_spawn::stress, ^^stress_scene_params::tumbler_axial_cubes>{ 12 },
		= gse::scenario::pin<^^ss::physics::rollback_history_steps>{ 90 }
	]]
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
	},
		= gse::scenario::pin<^^ss::dev_spawn::stress, ^^stress_scene_params::tumbler_radial_cubes>{ 6 },
		= gse::scenario::pin<^^ss::dev_spawn::stress, ^^stress_scene_params::tumbler_axial_cubes>{ 12 },
		= gse::scenario::pin<^^ss::physics::rollback_history_steps>{ 90 }
	]]
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
	},
		= gse::scenario::pin<^^ss::dev_spawn::stress, ^^stress_scene_params::tumbler_radial_cubes>{ 6 },
		= gse::scenario::pin<^^ss::dev_spawn::stress, ^^stress_scene_params::tumbler_axial_cubes>{ 12 }
	]]
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

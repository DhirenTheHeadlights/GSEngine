export module sandbox:scenarios;

import std;
import gse;
import gse.scenario;

export namespace sandbox::scenarios {
	[[= gse::scenario::info{ .name = "physics_stress", .scene = "Sandbox", .headless = true, .settings = { "Dev Spawn.stress.tumbler_radial_cubes=6", "Dev Spawn.stress.tumbler_axial_cubes=12" } }]]
	auto physics_stress(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "render_stress", .scene = "Sandbox", .settings = { "Dev Spawn.stress.tumbler_radial_cubes=6", "Dev Spawn.stress.tumbler_axial_cubes=12" } }]]
	auto render_stress(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "physics_stress_via_input", .scene = "Sandbox", .headless = true, .settings = { "Dev Spawn.stress.tumbler_radial_cubes=6", "Dev Spawn.stress.tumbler_axial_cubes=12" } }]]
	auto physics_stress_via_input(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "record_clip", .scene = "Sandbox", .warmup_frames = 0, .frames = 420, .settings = { "Graphics.enabled=false" } }]]
	auto record_clip(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "solver_showcase", .scene = "Pyramid", .warmup_frames = 0, .frames = 780, .settings = { "Dev Spawn.pyramid.base_count=60", "Physics.velocity_sleep_threshold=0 m/s", "Graphics.enabled=false" } }]]
	auto solver_showcase(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "atmosphere_showcase", .scene = "Sky", .warmup_frames = 0, .frames = 700, .settings = { "Graphics.enabled=false", "Clouds.cloud_coverage=0.45", "Grid.fade_distance=14000 m", "Grid.show_labels=false", "Grid.minor_spacing=50 m", "Grid.major_spacing=500 m" } }]]
	auto atmosphere_showcase(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "lighting_showcase", .scene = "Pyramid", .warmup_frames = 0, .frames = 660, .settings = { "Dev Spawn.pyramid.base_count=34", "Graphics.enabled=false", "Atmosphere.sun_elevation=-8 deg" } }]]
	auto lighting_showcase(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "locomotion", .scene = "Sandbox", .headless = true }]]
	auto locomotion(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "pyramid_cpu", .scene = "Pyramid", .headless = true, .warmup_frames = 0 }]]
	auto pyramid_cpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "pyramid_gpu", .scene = "Pyramid", .headless = true, .gpu_solver = true, .warmup_frames = 0 }]]
	auto pyramid_gpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "pyramid16k_cpu", .scene = "Pyramid", .headless = true, .warmup_frames = 0, .settings = { "Dev Spawn.pyramid.base_count=180", "Physics.solver_iterations=40" } }]]
	auto pyramid16k_cpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_stress_cpu", .scene = "Sandbox", .headless = true }]]
	auto parity_stress_cpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_stress_gpu", .scene = "Sandbox", .headless = true, .gpu_solver = true, .settings = { "Dev Spawn.stress.tumbler_radial_cubes=6", "Dev Spawn.stress.tumbler_axial_cubes=12" } }]]
	auto parity_stress_gpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_drop_cpu", .scene = "ParityDrop", .headless = true, .warmup_frames = 0 }]]
	auto parity_drop_cpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_drop_gpu", .scene = "ParityDrop", .headless = true, .gpu_solver = true, .warmup_frames = 0 }]]
	auto parity_drop_gpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_pair_cpu", .scene = "ParityPair", .headless = true, .warmup_frames = 0 }]]
	auto parity_pair_cpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_pair_gpu", .scene = "ParityPair", .headless = true, .gpu_solver = true, .warmup_frames = 0 }]]
	auto parity_pair_gpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_stack_cpu", .scene = "ParityStack", .headless = true, .warmup_frames = 0 }]]
	auto parity_stack_cpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_stack_gpu", .scene = "ParityStack", .headless = true, .gpu_solver = true, .warmup_frames = 0 }]]
	auto parity_stack_gpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_cluster_cpu", .scene = "ParityCluster", .headless = true, .warmup_frames = 0 }]]
	auto parity_cluster_cpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_cluster_gpu", .scene = "ParityCluster", .headless = true, .gpu_solver = true, .warmup_frames = 0 }]]
	auto parity_cluster_gpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_heap_cpu", .scene = "ParityHeap", .headless = true, .warmup_frames = 0 }]]
	auto parity_heap_cpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_heap_gpu", .scene = "ParityHeap", .headless = true, .gpu_solver = true, .warmup_frames = 0 }]]
	auto parity_heap_gpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_mound_cpu", .scene = "ParityMound", .headless = true, .warmup_frames = 0 }]]
	auto parity_mound_cpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_mound_gpu", .scene = "ParityMound", .headless = true, .gpu_solver = true, .warmup_frames = 0 }]]
	auto parity_mound_gpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_pile_cpu", .scene = "ParityPile", .headless = true, .warmup_frames = 0 }]]
	auto parity_pile_cpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_pile_gpu", .scene = "ParityPile", .headless = true, .gpu_solver = true, .warmup_frames = 0 }]]
	auto parity_pile_gpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_hull_pile_cpu", .scene = "ParityHullPile", .headless = true, .warmup_frames = 0 }]]
	auto parity_hull_pile_cpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_nojoints_gpu", .scene = "Sandbox", .headless = true, .gpu_solver = true, .warmup_frames = 0 }]]
	auto parity_nojoints_gpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_empty_gpu", .scene = "Sandbox", .headless = true, .gpu_solver = true, .warmup_frames = 0 }]]
	auto parity_empty_gpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_jointsonly_gpu", .scene = "Sandbox", .headless = true, .gpu_solver = true, .warmup_frames = 0 }]]
	auto parity_jointsonly_gpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_jointsonly_cpu", .scene = "Sandbox", .headless = true, .warmup_frames = 0 }]]
	auto parity_jointsonly_cpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_overlap_cpu", .scene = "ParityOverlap", .headless = true, .warmup_frames = 0 }]]
	auto parity_overlap_cpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_overlap_gpu", .scene = "ParityOverlap", .headless = true, .gpu_solver = true, .warmup_frames = 0 }]]
	auto parity_overlap_gpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_shapes_cpu", .scene = "ParityShapes", .headless = true, .warmup_frames = 0 }]]
	auto parity_shapes_cpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_shapes_gpu", .scene = "ParityShapes", .headless = true, .gpu_solver = true, .warmup_frames = 0 }]]
	auto parity_shapes_gpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_domino_cpu", .scene = "ParityDomino", .headless = true, .warmup_frames = 0 }]]
	auto parity_domino_cpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_domino_gpu", .scene = "ParityDomino", .headless = true, .gpu_solver = true, .warmup_frames = 0 }]]
	auto parity_domino_gpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;
}

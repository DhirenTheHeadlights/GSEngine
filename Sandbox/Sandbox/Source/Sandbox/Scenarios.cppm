export module sandbox:scenarios;

import std;
import gse;
import gse.scenario;

export namespace sandbox::scenarios {
	[[= gse::scenario::info{ .name = "physics_stress", .scene = "Sandbox", .headless = true }]]
	auto physics_stress(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "render_stress", .scene = "Sandbox" }]]
	auto render_stress(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "physics_stress_via_input", .scene = "Sandbox", .headless = true }]]
	auto physics_stress_via_input(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "locomotion", .scene = "Sandbox", .headless = true }]]
	auto locomotion(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_stress_cpu", .scene = "Sandbox", .headless = true }]]
	auto parity_stress_cpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_stress_gpu", .scene = "Sandbox", .headless = true, .gpu_solver = true }]]
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

	[[= gse::scenario::info{ .name = "parity_pile_cpu", .scene = "ParityPile", .headless = true, .warmup_frames = 0 }]]
	auto parity_pile_cpu(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "parity_pile_gpu", .scene = "ParityPile", .headless = true, .gpu_solver = true, .warmup_frames = 0 }]]
	auto parity_pile_gpu(
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
}

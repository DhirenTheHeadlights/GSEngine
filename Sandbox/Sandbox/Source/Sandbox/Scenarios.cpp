module sandbox:scenarios_impl;

import std;
import gse;
import gse.scenario;

import :dev_spawn_system;
import :scenarios;

namespace sandbox::scenarios {
	auto build_stress_workload(
		gse::scenario::context& ctx
	) -> gse::async::task<>;
}

auto sandbox::scenarios::build_stress_workload(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	ctx.channels().push<spawn_stress_request>({});
	co_await gse::scenario::wait_frames(ctx, 1);
	ctx.channels().push<spawn_joints_request>({});
}

auto sandbox::scenarios::physics_stress(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await build_stress_workload(ctx);
}

auto sandbox::scenarios::render_stress(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await build_stress_workload(ctx);
}

auto sandbox::scenarios::physics_stress_via_input(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	ctx.channels().push<gse::input::synthetic_input_request>({
		.value = gse::input::key_pressed{ .key_code = gse::key::f5 },
	});
	co_await gse::scenario::wait_frames(ctx, 1);
	ctx.channels().push<gse::input::synthetic_input_request>({
		.value = gse::input::key_released{ .key_code = gse::key::f5 },
	});
}

auto sandbox::scenarios::locomotion(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	ctx.channels().push<spawn_character_request>({});

	co_await gse::scenario::wait(ctx, gse::seconds(1.f));
	ctx.channels().push<gse::input::synthetic_input_request>({
		.value = gse::input::key_pressed{ .key_code = gse::key::left_shift },
	});
	ctx.channels().push<gse::input::synthetic_input_request>({
		.value = gse::input::key_pressed{ .key_code = gse::key::w },
	});
}

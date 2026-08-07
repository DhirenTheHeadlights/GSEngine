module sandbox:scenarios_impl;

import std;
import gse;
import gse.scenario;

import :dev_spawn_system;
import :scenarios;

auto sandbox::scenarios::physics_stress(gse::scenario::context& ctx) -> gse::async::task<> {
	co_await gse::scenario::wait_settled(ctx);
	ctx.channels().push<spawn_stress_request>({});
	co_await gse::scenario::wait_frames(ctx, 1);
	ctx.channels().push<spawn_joints_request>({});
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

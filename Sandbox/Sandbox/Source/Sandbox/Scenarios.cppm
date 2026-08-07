export module sandbox:scenarios;

import std;
import gse;
import gse.scenario;

export namespace sandbox::scenarios {
	[[= gse::scenario::info{ .name = "physics_stress", .scene = "Sandbox", .headless = true }]]
	auto physics_stress(
		gse::scenario::context& ctx
	) -> gse::async::task<>;

	[[= gse::scenario::info{ .name = "physics_stress_via_input", .scene = "Sandbox", .headless = true }]]
	auto physics_stress_via_input(
		gse::scenario::context& ctx
	) -> gse::async::task<>;
}

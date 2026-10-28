import std;

import gse;
import gse.test;
import gse.tests;
import sandbox;

auto main(int argc, char** argv) -> int {
	auto [cfg, passed_flags] = gse::parse_args<sandbox::startup::config>(argc, argv);

	if (gse::test::requested(cfg.engine.test)) {
		const std::array tables = { gse::test::registry<^^gse::tests>() };
		return gse::test::run({
			.tables = tables,
			.options = cfg.engine.test,
			.flag_prefix = "--engine-test",
		});
	}

	if (!cfg.scan_states.empty()) {
		return sandbox::startup::run_scan_states(cfg);
	}

	if (!cfg.compare_states_a.empty() || !cfg.compare_states_b.empty()) {
		if (cfg.compare_states_a.empty() || cfg.compare_states_b.empty()) {
			gse::log::println(gse::log::level::error, gse::log::category::general, "--compare-states-a and --compare-states-b must both be given");
			return 1;
		}
		return sandbox::startup::run_compare_states(cfg);
	}

	if (!cfg.engine.bench.scenario.empty()) {
		if (cfg.physics_parity) {
			gse::log::println(gse::log::level::error, gse::log::category::general, "--engine-bench-scenario and --physics-parity select different run modes; pick one");
			return 1;
		}
		if (!sandbox::startup::apply_scenario(cfg, passed_flags)) {
			return 1;
		}
	}

	if (cfg.physics_parity) {
		return sandbox::startup::run_physics_parity(cfg);
	}

	return sandbox::startup::run_game(cfg.engine);
}

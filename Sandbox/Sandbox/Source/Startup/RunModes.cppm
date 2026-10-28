export module sandbox:run_modes;

import std;
import gse;

import :startup;

export namespace sandbox::startup {
	auto run_game(
		const gse::engine_config& engine
	) -> int;

	auto run_physics_parity(
		const config& cfg
	) -> int;

	auto apply_scenario(
		config& cfg,
		std::span<const std::string> passed_flags
	) -> bool;
}

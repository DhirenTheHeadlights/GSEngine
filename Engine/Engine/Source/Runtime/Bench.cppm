module gse.runtime:bench;

import std;

import gse.core;
import gse.diag;
import gse.ecs;
import gse.concurrency;
import gse.log;
import gse.scenario;

import :engine;

namespace gse {
	enum class bench_phase : std::uint8_t {
		awaiting_scene,
		settling [[
			= entered_from({ awaiting_scene })
		]],
		warmup [[
			= entered_from({ awaiting_scene, settling })
		]],
		measuring [[
			= entered_from({ warmup })
		]],
		done [[
			= entered_from({ awaiting_scene, settling, measuring })
		]],
	};

	struct bench_state {
		lifecycle<bench_phase> phase{ "bench", log::category::general };
		int frames_in_phase = 0;
		scenario::wait_gate gate;
		std::optional<scenario::context> scenario_ctx;
		async::task<> scenario_task;
		std::ofstream state_dump;
		std::vector<std::string> failures;
	};

	auto world_populated(
		registry& reg
	) -> bool;

	auto world_state_hash(
		registry& reg
	) -> std::uint64_t;

	auto write_state_frame(
		registry& reg,
		std::ostream& out,
		std::uint32_t frame
	) -> void;

	auto bench_world_ready(
		const bench_config& config,
		engine& e
	) -> bool;

	auto drive_scenario(
		const bench_config& config,
		bench_state& state,
		engine& e
	) -> void;

	auto begin_bench(
		const bench_config& config
	) -> void;

	auto step_bench(
		const bench_config& config,
		bench_state& state,
		engine& e
	) -> bool;

	auto finish_bench(
		const bench_config& config,
		bench_state& state,
		engine& e
	) -> void;

	auto bench_exit_code(
		const bench_state& state
	) -> int;
}

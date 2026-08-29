export module gse.physics:vbd_gpu_solver_diagnostics;

import std;

import gse.core;
import gse.concurrency;
import gse.ecs;

import :system;
import :vbd_gpu_solver;

export namespace gse::physics::gpu_diagnostics {
	struct [[= system_state<"GPU Solver Diagnostics">{}, = settings::category<"GpuSolverDiagnostics">{}, = deferred_system{}]] data {
		[[
			= settings::describe<"Track the gpu solver's per-tick diagnostic counters (contact and colour peaks, "
									  "coloring conflicts, stale reads, joint extremes) and log them whenever a peak "
									  "moves or every 60 ticks. Reads the diagnostics header the solver already reads "
									  "back each dispatch, so the solve itself is untouched.">{}
		]]
		bool enabled = true;

		vbd::solver_diagnostics peak;
		std::uint64_t conflict_total = 0;
		std::uint64_t stale_read_total = 0;
		std::uint64_t stale_check_total = 0;
		std::uint32_t stale_first_tick = 0;
		std::uint32_t ticks = 0;
	};

	[[= system_run<3>{}]]
	auto run(
		data& d,
		shared_view<physics::data> phys
	) -> async::task<>;
}
export module gse.physics:vbd_cpu_contact_trace;

import std;

import gse.core;
import gse.concurrency;
import gse.ecs;

import :system;

export namespace gse::physics::cpu_contact_trace {
	struct [[= system_state<"CPU Contact Trace">{}, = settings::category<"CpuContactTrace">{}, = deferred_system{}]] data {
		[[
			= settings::describe<"Log the cpu solver's retired contact state (feature key, lambda, penalty, gap, sticking, "
									  "anchors) for every contact touching the body with this owner id, plus the body's own "
									  "state line each frame. 0 traces nothing. A real-path instrument: it reads the constraint "
									  "graph the solver already holds after its solve, so the solve itself is untouched. "
									  "Requires Physics.use_gpu_solver to be off.">{}
		]]
		std::uint64_t body = 0;

		std::uint64_t tick = 0;
		std::uint32_t reports = 0;
	};

	[[= system_run<3>{}]]
	auto run(
		data& d,
		shared_view<physics::data> phys
	) -> async::task<>;
}
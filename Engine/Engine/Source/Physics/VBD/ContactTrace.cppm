export module gse.physics:vbd_contact_trace;

import std;

import gse.core;
import gse.concurrency;
import gse.ecs;

import :system;

export namespace gse::physics::contact_trace {
	struct [[= gse::system_state<"Contact Trace">{}, = gse::settings::category<"ContactTrace">{}, = gse::deferred_system{}]] data {
		[[
			= gse::settings::describe<"Log the gpu solver's retired per-tick contact state (lambda, penalty, gap, sticking, "
									  "anchors) for every contact touching the body with this owner id, plus the body's own "
									  "state line each tick. 0 traces nothing. A real-path instrument: it consumes the "
									  "readback the solver already performs every dispatch, so the solve itself is untouched. "
									  "Requires Physics.use_gpu_solver.">{}
		]]
		std::uint64_t body = 0;

		std::uint64_t last_generation = 0;
		bool missing_reported = false;
		bool overflow_reported = false;
		bool hashes_warned = false;
	};

	[[= gse::system_run<3>{}]]
	auto run(
		data& d,
		shared_view<physics::data> phys
	) -> async::task<>;
}

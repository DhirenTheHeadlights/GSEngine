export module gse.physics:vbd_cpu_contact_trace;

import std;

import gse.core;
import gse.concurrency;
import gse.ecs;

import :system;

export namespace gse::physics::cpu_contact_trace {
	struct [[= system_state<"CPU Contact Trace">{}, = deferred_system{}]] data {
		std::uint64_t tick = 0;
		std::uint32_t reports = 0;
	};

	[[= system_run<3>{}]]
	auto run(
		data& d,
		shared_view<physics::data> phys
	) -> async::task<>;
}
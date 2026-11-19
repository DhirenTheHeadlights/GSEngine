export module gse.physics:vbd_contact_trace;

import std;

import gse.core;
import gse.concurrency;
import gse.ecs;

import :system;

export namespace gse::physics::contact_trace {
	struct [[= system_state<"Contact Trace">{}, = deferred_system{}]] data {
		std::uint64_t last_generation = 0;
		bool missing_reported = false;
		bool overflow_reported = false;
		bool hashes_warned = false;
	};

	[[= system_run<3>{}]]
	auto run(
		data& d,
		shared_view<physics::data> phys
	) -> async::task<>;
}
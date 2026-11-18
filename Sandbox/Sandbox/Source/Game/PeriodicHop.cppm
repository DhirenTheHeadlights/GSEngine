export module sandbox:periodic_hop;

import std;
import gse;

export namespace sandbox::periodic_hop {
	struct component {
		gse::time period = gse::seconds(1.f);
		gse::vec3<gse::velocity> kick = gse::vec3<gse::velocity>(gse::meters_per_second(0.f), gse::meters_per_second(2.f), gse::meters_per_second(0.f));
		bool reversing = false;
		gse::time waited;
	};

	struct [[= gse::system_state<"PeriodicHop">{}]]
	data {};

	[[= gse::system_run<>{}]]
	auto run(
		data& d,
		gse::write<component> hops,
		gse::read<gse::physics::motion_component> motions,
		gse::channel_write<gse::physics::impulse_request> impulse_out
	) -> gse::async::task<>;
}

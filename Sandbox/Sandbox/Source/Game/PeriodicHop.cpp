module sandbox:periodic_hop_impl;

import std;
import gse;

import :periodic_hop;

auto sandbox::periodic_hop::run(data&, gse::write<component> hops, gse::read<gse::physics::motion_component> motions, const gse::channel_write<gse::physics::impulse_request> impulse_out) -> gse::async::task<> {
	const gse::time stepped = gse::system_clock::fixed_dt<gse::time>() * static_cast<float>(gse::system_clock::fixed_steps_this_frame());

	const auto owners = hops.owner_ids();
	for (std::size_t index = 0; index < hops.size(); ++index) {
		auto& hop = hops[index];
		hop.waited += stepped;
		if (hop.waited < hop.period) {
			continue;
		}
		hop.waited -= hop.period;

		const auto* motion = motions.find(owners[index]);
		if (motion == nullptr) {
			continue;
		}
		impulse_out.push<gse::physics::impulse_request>({
			.target = owners[index],
			.impulse = hop.kick * gse::physics::mass_of(*motion),
		});
		if (hop.reversing) {
			hop.kick = -hop.kick;
		}
	}

	return {};
}

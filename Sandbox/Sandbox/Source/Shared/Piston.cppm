export module sandbox:piston;

import std;
import gse;

export namespace sandbox::piston {
	struct component {
		gse::vec3<gse::position> center;
		gse::vec3<gse::length> amplitude;
		gse::angular_velocity omega = gse::radians_per_second(0.f);
		gse::angle phase = gse::radians(0.f);
	};
}

export namespace sandbox::piston {
	struct [[= gse::system_state<"Piston">{}]] data {};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		gse::write<component> pistons,
		gse::write<gse::physics::kinematic_target_component> targets
	) -> gse::async::task<>;
}

auto sandbox::piston::run(gse::context& ctx, gse::write<component> pistons, gse::write<gse::physics::kinematic_target_component> targets) -> gse::async::task<> {
	const int steps = gse::system_clock::fixed_steps_this_frame();
	const auto step_dt = gse::system_clock::fixed_dt<gse::time>();
	const float frame_step_count = static_cast<float>(steps);

	const auto piston_ids = pistons.owner_ids();
	for (std::size_t i = 0; i < pistons.size(); ++i) {
		auto& p = pistons[i];
		auto* target = targets.find(piston_ids[i]);
		if (!target) {
			continue;
		}

		target->position = p.center + p.amplitude * gse::sin(p.phase);

		p.phase += p.omega * step_dt * frame_step_count;
	}

	return {};
}

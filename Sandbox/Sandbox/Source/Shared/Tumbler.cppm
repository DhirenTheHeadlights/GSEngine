export module sandbox:tumbler;

import std;
import gse;

export namespace sandbox::tumbler {
	struct component {
		gse::vec3<gse::position> center;
		gse::vec3f axis = gse::axis_z;
		gse::angular_velocity angular_speed = gse::radians_per_second(0.6f);
		gse::vec3<gse::length> local_offset;
		gse::angle phase = gse::radians(0.f);
	};
}

export namespace sandbox::tumbler {
	struct [[= gse::system_state<"Tumbler">{}]] data {};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		gse::write<component> tumblers,
		gse::write<gse::physics::kinematic_target_component> targets
	) -> gse::async::task<>;
}

auto sandbox::tumbler::run(gse::context& ctx, gse::write<component> tumblers, gse::write<gse::physics::kinematic_target_component> targets) -> gse::async::task<> {
	const int steps = gse::system_clock::fixed_steps_this_frame();
	const auto step_dt = gse::system_clock::fixed_dt<gse::time>();
	const float frame_step_count = static_cast<float>(steps);

	const auto tumbler_ids = tumblers.owner_ids();
	for (std::size_t i = 0; i < tumblers.size(); ++i) {
		auto& t = tumblers[i];
		auto* target = targets.find(tumbler_ids[i]);
		if (!target) {
			continue;
		}

		const gse::quat world_rot(t.axis, t.phase);

		target->position = t.center + gse::rotate_vector(world_rot, t.local_offset);
		target->orientation = world_rot;

		t.phase += t.angular_speed * step_dt * frame_step_count;
	}

	return {};
}

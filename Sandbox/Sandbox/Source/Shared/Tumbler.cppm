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
	struct [[= gse::system_state<"Tumbler">{}]] data {
		bool traced = false;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		gse::write<component> tumblers,
		gse::write<gse::physics::kinematic_target_component> targets
	) -> gse::async::task<>;
}

auto sandbox::tumbler::run(gse::context& ctx, data& d, gse::write<component> tumblers, gse::write<gse::physics::kinematic_target_component> targets) -> gse::async::task<> {
	const int steps = gse::system_clock::fixed_steps_this_frame();
	const auto step_dt = gse::system_clock::fixed_dt<gse::time>();
	const float frame_step_count = static_cast<float>(steps);

	const bool trace = !d.traced && !tumblers.empty();
	d.traced = d.traced || !tumblers.empty();

	const auto tumbler_ids = tumblers.owner_ids();
	for (std::size_t i = 0; i < tumblers.size(); ++i) {
		auto& t = tumblers[i];
		auto* target = targets.find(tumbler_ids[i]);
		if (!target) {
			if (trace) {
				gse::log::println(
					gse::log::category::physics,
					"[tumbler-trace] {} has no kinematic_target_component; center={:.2f} local_offset={:.2f}",
					tumbler_ids[i],
					t.center,
					t.local_offset
				);
			}
			continue;
		}

		t.phase += t.angular_speed * step_dt * frame_step_count;

		const gse::quat world_rot(t.axis, t.phase);

		target->position = t.center + gse::rotate_vector(world_rot, t.local_offset);
		target->orientation = world_rot;

		if (trace) {
			gse::log::println(
				gse::log::category::physics,
				"[tumbler-trace] {} center={:.2f} local_offset={:.2f} axis={:.2f} phase={:.3f:rad} steps={} -> target={:.2f}",
				tumbler_ids[i],
				t.center,
				t.local_offset,
				t.axis,
				t.phase,
				steps,
				target->position
			);
		}
	}

	return {};
}

export module gs:tumbler;

import std;
import gse;

export namespace gs::tumbler {
	struct component {
		gse::vec3<gse::position> center;
		gse::vec3f axis = gse::axis_z;
		gse::angular_velocity angular_speed = gse::radians_per_second(0.6f);
		gse::vec3<gse::length> local_offset;
		gse::angle phase = gse::radians(0.f);
	};

	struct system {
		static auto run(
			gse::run_context& ctx
		) -> gse::async::task<>;
	};
}

auto gs::tumbler::system::run(gse::run_context& ctx) -> gse::async::task<> {
	while (true) {
		{
			auto [tumblers, transforms, motions] = co_await ctx.acquire_with(
				gse::write_v<component>,
				gse::write_v<gse::physics::transform_component>,
				gse::write_v<gse::physics::motion_component>
			);

			const int steps = gse::system_clock::fixed_steps_this_frame();
			const auto step_dt = gse::system_clock::fixed_dt<gse::time>();
			const float frame_step_count = static_cast<float>(steps);

			const auto tumbler_ids = tumblers.owner_ids();
			for (std::size_t i = 0; i < tumblers.size(); ++i) {
				auto& t = tumblers[i];
				const auto eid = tumbler_ids[i];
				auto* motion = motions.find(eid);
				auto* transform = transforms.find(eid);
				if (!motion || !transform) {
					continue;
				}

				const gse::quat world_rot(t.axis, t.phase);
				const auto world_offset = gse::rotate_vector(world_rot, t.local_offset);
				const gse::vec3<gse::angular_velocity> ang_vel(
					t.axis.x() * t.angular_speed,
					t.axis.y() * t.angular_speed,
					t.axis.z() * t.angular_speed
				);
				const auto lin_vel = cross(ang_vel, world_offset) / gse::rad;

				transform->position = t.center + world_offset;
				transform->orientation = world_rot;
				motion->current_velocity = lin_vel;
				motion->angular_velocity = ang_vel;

				t.phase += t.angular_speed * step_dt * frame_step_count;
			}
		}

		co_await ctx.next_tick();
	}
}

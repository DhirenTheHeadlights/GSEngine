export module gs:tumbler;

import std;
import gse;

export namespace gs::tumbler {
	struct component_data {
		gse::vec3<gse::position> center;
		gse::vec3f axis = gse::axis_z;
		gse::angular_velocity angular_speed = gse::radians_per_second(0.6f);
		gse::vec3<gse::length> local_offset;
		gse::angle phase = gse::radians(0.f);
		gse::time accumulator = gse::seconds(0.f);
	};

	using component = gse::component<component_data>;

	struct state {};

	struct system {
		static auto update(
			gse::update_context& ctx
		) -> void;
	};
}

auto gs::tumbler::system::update(gse::update_context& ctx) -> void {
	auto tumblers = ctx.reg.acquire_write<component>();

	constexpr auto physics_step = gse::seconds(1.f / 60.f);

	for (auto& t : tumblers) {
		const auto owner_id = t.owner_id();
		auto* motion = ctx.reg.try_component<gse::physics::motion_component>(owner_id);
		if (!motion) {
			continue;
		}

		t.accumulator += gse::system_clock::dt();
		while (t.accumulator >= physics_step) {
			t.accumulator -= physics_step;
			t.phase += t.angular_speed * physics_step;
		}

		const gse::quat world_rot(t.axis, t.phase);
		const auto world_offset = gse::rotate_vector(world_rot, t.local_offset);
		const gse::vec3<gse::angular_velocity> ang_vel(
			t.axis.x() * t.angular_speed,
			t.axis.y() * t.angular_speed,
			t.axis.z() * t.angular_speed
		);
		const auto lin_vel = cross(ang_vel, world_offset) / gse::rad;

		motion->current_position = t.center + world_offset;
		motion->orientation = world_rot;
		motion->current_velocity = lin_vel;
		motion->angular_velocity = ang_vel;
		motion->sleeping = false;
	}
}

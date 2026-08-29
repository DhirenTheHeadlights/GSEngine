export module gse.examples:free_camera;

import std;

import gse.runtime;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.graphics;
import gse.physics;
import gse.os;
import gse.assets;
import gse.gpu;

export namespace gse::free_camera {
	struct component {
		vec3<position> initial_position = vec3<position>(0.f, 0.f, 5.f);
		int priority = 10;
		int detached_priority = 100;
		velocity speed = meters_per_second(100.f);
		angle yaw = degrees(-90.f);
		angle pitch = degrees(0.f);
		float mouse_sensitivity = 0.1f;
		length collision_radius = meters(0.25f);
		length collision_skin = meters(0.05f);
		bool collide_with_geometry = true;
	};

	struct bindings {
		actions::handle forward;
		actions::handle left;
		actions::handle back;
		actions::handle right;
		actions::handle up;
		actions::handle down;
		actions::handle toggle;
		id move_axis_id;
		bool detached = false;
	};

}

export namespace gse::free_camera::system {
	struct [[= system_state<"FreeCamera">{}]] data {
		std::unordered_map<id, bindings> bindings_by_owner;
	};

	[[= system_run<>{}]]
	auto attach(
		context& ctx,
		data& d,
		channel_write<actions::add_action_request, actions::bind_axis2_request> actions_out,
		write<component> cameras,
		structural<camera::follow_component> follows
	) -> async::task<>;

	[[= system_run<1>{}]]
	auto update(
		context& ctx,
		data& d,
		shared_view<actions::data> as,
		shared_view<input::data> input_s,
		shared_view<camera::data> cam_s,
		write<component> cameras,
		write<camera::follow_component> follows,
		read<physics::transform_component> transforms,
		read<physics::collision_component> collisions,
		read<physics::motion_component> motions
	) -> async::task<>;
}
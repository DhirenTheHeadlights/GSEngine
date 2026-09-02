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
import gse.meta;
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
		angle mouse_sensitivity = degrees(0.1f);
		length collision_radius = meters(0.25f);
		length collision_skin = meters(0.05f);
		bool collide_with_geometry = true;
	};

	struct bindings {
		[[= actions::bind<"Free Camera Forward", key::w>{}]]
		actions::handle forward;

		[[= actions::bind<"Free Camera Left", key::a>{}]]
		actions::handle left;

		[[= actions::bind<"Free Camera Backward", key::s>{}]]
		actions::handle back;

		[[= actions::bind<"Free Camera Right", key::d>{}]]
		actions::handle right;

		[[= actions::bind<"Free Camera Up", key::space>{}]]
		actions::handle up;

		[[= actions::bind<"Free Camera Down", key::left_control>{}]]
		actions::handle down;

		[[= actions::bind<"Toggle Free Camera", key::f1>{}]]
		actions::handle toggle;

		[[= actions::axis2<"Free Camera Move", "left", "right", "back", "forward">{}]]
		id move_axis_id;

		[[= actions::axis2_mouse<"Free Camera Look">{}]]
		id look_axis_id;
	};

	struct owner_state {
		bool detached = false;
	};

}

export namespace gse::free_camera::system {
	struct [[= system_state<"FreeCamera">{}]] data {
		[[= actions::set{}]] bindings binds;
		std::unordered_map<id, owner_state> owners;
	};

	[[= system_run<>{}]]
	auto attach(
		context& ctx,
		data& d,
		write<component> cameras,
		structural<camera::follow_component> follows
	) -> async::task<>;

	[[= system_run<1>{}]]
	auto update(
		context& ctx,
		data& d,
		shared_view<actions::data> as,
		shared_view<camera::data> cam_s,
		write<component> cameras,
		write<camera::follow_component> follows,
		read<physics::transform_component> transforms,
		read<physics::collision_component> collisions,
		read<physics::motion_component> motions
	) -> async::task<>;
}
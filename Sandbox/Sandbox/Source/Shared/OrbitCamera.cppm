export module sandbox:orbit_camera;

import std;
import gse;

export namespace sandbox::orbit_camera {
	struct component {
		gse::id target;
		gse::length pivot_height = gse::meters(0.5f);
		gse::angle yaw = gse::degrees(0.f);
		gse::angle pitch = gse::degrees(-15.f);
		gse::length distance = gse::meters(8.f);
		std::array<gse::length, 4> view_steps = {
			gse::meters(0.f),
			gse::meters(2.f),
			gse::meters(4.f),
			gse::meters(7.f),
		};
		int view_step = 2;
		bool stepped_views = false;
		gse::length min_distance = gse::meters(1.5f);
		gse::length max_distance = gse::meters(50.f);
		int priority = 60;
		bool active = false;
		bool free_look = false;
		float mouse_sensitivity = 0.25f;
		float arrow_speed = 90.f;
		float scroll_zoom_step = 0.5f;
		gse::length collision_radius = gse::meters(0.25f);
		gse::length collision_skin = gse::meters(0.05f);
		bool collide_with_geometry = true;
	};

	struct bindings {
		gse::actions::handle toggle;
		gse::actions::handle yaw_left;
		gse::actions::handle yaw_right;
		gse::actions::handle pitch_up;
		gse::actions::handle pitch_down;
	};

}

export namespace sandbox::orbit_camera {
	struct [[= gse::system_state<"OrbitCamera">{}]] data {
		std::unordered_map<gse::id, bindings> bindings_by_owner;
	};

	[[= gse::system_run<>{}]]
	auto attach(
		gse::context& ctx,
		data& d,
		gse::write<component> orbits,
		gse::structural<gse::camera::follow_component> follows
	) -> gse::async::task<>;

	[[= gse::system_run<1>{}]]
	auto update(
		gse::context& ctx,
		data& d,
		gse::shared_view<gse::actions::data> as,
		gse::shared_view<gse::input::data> input_s,
		gse::shared_view<gse::camera::data> cam_s,
		gse::shared_view<gse::physics::data> phys_s,
		gse::write<component> orbits,
		gse::write<gse::camera::follow_component> follows,
		gse::read<gse::physics::transform_component> transforms,
		gse::read<gse::physics::collision_component> collisions,
		gse::read<gse::physics::motion_component> motions
	) -> gse::async::task<>;
}

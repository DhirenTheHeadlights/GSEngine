export module sandbox:workloads;

import gse;

import :archetypes;
import :joints;
import :machines;
import :spawn_params;
import :stacks;

export namespace sandbox {
	auto spawn_physics_stress(
		gse::scene& s,
		const stress_scene_params& params = {}
	) -> void;

	auto spawn_joint_test(
		gse::scene& s
	) -> void;
}

auto sandbox::spawn_physics_stress(gse::scene& s, const stress_scene_params& params) -> void {
	spawn_inverted_mass_pyramid(s, gse::vec3<gse::position>(gse::meters(-15.f, 0.f, 0.f)));
	spawn_domino_chain(s, 0, gse::vec3<gse::position>(gse::meters(-8.f, 0.f, -10.f)));
	spawn_funnel(s, gse::vec3<gse::position>(gse::meters(15.f, 0.f, 0.f)));
	spawn_slope_friction_test(s, gse::vec3<gse::position>(gse::meters(0.f, 0.f, 15.f)));
	spawn_high_speed_impact_target(s, gse::vec3<gse::position>(gse::meters(0.f, 0.f, -20.f)));
	spawn_box_grid(s, gse::vec3<gse::position>(gse::meters(20.f, 0.f, -10.f)), params);
	spawn_spring_tests(s, gse::vec3<gse::position>(gse::meters(-25.f, 0.f, -20.f)));

	const float drum_clearance = static_cast<float>(params.tumbler_radial_cubes) * 0.5f + 2.f;
	spawn_tumbler(s, 0, gse::vec3<gse::position>(gse::meters(-12.f - drum_clearance, 4.f + drum_clearance, 24.f + drum_clearance)), gse::axis_z, gse::radians_per_second(0.6f), params);
	spawn_tumbler(s, 1, gse::vec3<gse::position>(gse::meters(12.f + drum_clearance, 4.f + drum_clearance, 24.f + drum_clearance)), gse::axis_x, gse::radians_per_second(0.5f), params);

	spawn_sphere_stack(s, gse::vec3<gse::position>(gse::meters(30.f, 0.f, 5.f)));
	spawn_corner_drop(s, gse::vec3<gse::position>(gse::meters(30.f, 0.f, 15.f)));
	spawn_elevator(s, gse::vec3<gse::position>(gse::meters(-30.f, 0.f, 10.f)));
	spawn_vise(s, gse::vec3<gse::position>(gse::meters(0.f, 0.f, 30.f)));

	s.spawn("Bouncy Sphere", sphere(gse::vec3<gse::position>(gse::meters(-15.f, 8.f, 0.f)), gse::meters(1.f)));
}

auto sandbox::spawn_joint_test(gse::scene& s) -> void {
	spawn_fixed_joint(s, gse::vec3<gse::position>(gse::meters(-20.f, 0.f, 0.f)));
	spawn_distance_pendulum(s, gse::vec3<gse::position>(gse::meters(-10.f, 0.f, 0.f)));
	spawn_hinge_door(s, gse::vec3<gse::position>(gse::meters(0.f, 0.f, 0.f)));
	spawn_slider_elevator(s, gse::vec3<gse::position>(gse::meters(10.f, 0.f, 0.f)));
	spawn_pendulum_chain(s, 0, gse::vec3<gse::position>(gse::meters(20.f, 0.f, 0.f)));

	s.spawn("Joint Test Sphere", sphere(gse::vec3<gse::position>(gse::meters(0.f, 6.f, -8.f)), gse::meters(1.f)));
}

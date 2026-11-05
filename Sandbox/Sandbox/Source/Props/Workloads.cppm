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

	auto spawn_grass_field(
		gse::scene& s,
		const gse::vec3<gse::position>& origin,
		const grass_field_params& params
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

auto sandbox::spawn_grass_field(gse::scene& s, const gse::vec3<gse::position>& origin, const grass_field_params& params) -> void {
	const auto side = static_cast<float>(params.blades_per_side);
	const auto extent = params.spacing * side;
	const auto plate_size = gse::vec3<gse::length>(extent, gse::meters(0.1f), extent);
	const auto plate_pos = origin + gse::vec3<gse::displacement>(gse::meters(0.f), gse::meters(-0.05f), gse::meters(0.f));
	const auto plate_id = s.spawn("Grass Plate", static_box(plate_pos, plate_size, gse::quat(1.f, 0.f, 0.f, 0.f), gse::vec3f(0.22f, 0.16f, 0.1f)));

	const auto half_segment = params.segment_length * 0.5f;
	const auto segment_size = gse::vec3<gse::length>(gse::meters(0.03f), params.segment_length, gse::meters(0.006f));
	const auto top_of_plate = gse::vec3<gse::displacement>(gse::meters(0.f), gse::meters(0.05f), gse::meters(0.f));
	const auto up = gse::vec3<gse::displacement>(gse::meters(0.f), half_segment, gse::meters(0.f));
	const auto down = gse::vec3<gse::displacement>(gse::meters(0.f), -half_segment, gse::meters(0.f));
	const auto stiffness = gse::vec3<gse::angular_stiffness>(params.bend_stiffness, params.bend_stiffness, params.bend_stiffness);

	for (int bx = 0; bx < params.blades_per_side; ++bx) {
		for (int bz = 0; bz < params.blades_per_side; ++bz) {
			const auto root = gse::vec3<gse::displacement>(
				params.spacing * (static_cast<float>(bx) + 0.5f) - extent * 0.5f,
				gse::meters(0.f),
				params.spacing * (static_cast<float>(bz) + 0.5f) - extent * 0.5f
			);
			gse::id previous = plate_id;
			for (int seg = 0; seg < params.segments; ++seg) {
				const auto height = params.segment_length * (static_cast<float>(seg) + 0.5f);
				const auto centre = origin + root + gse::vec3<gse::displacement>(gse::meters(0.f), height, gse::meters(0.f));
				const auto segment_id = s.spawn(
					std::format("Grass {}_{} Segment {}", bx, bz, seg),
					box(centre, segment_size, gse::kilograms(0.002f))
				);
				s.build(std::format("Grass {}_{} Joint {}", bx, bz, seg))
					.with<gse::physics::joint_spec>({
						.entity_a = previous,
						.entity_b = segment_id,
						.config = gse::physics::ball_joint{
							.anchor_a = seg == 0 ? root + top_of_plate : up,
							.anchor_b = down,
							.rest_stiffness = stiffness,
						},
					});
				previous = segment_id;
			}
		}
	}
}

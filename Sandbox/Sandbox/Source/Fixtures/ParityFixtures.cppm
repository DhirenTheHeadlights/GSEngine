export module sandbox:parity_fixtures;

import std;
import gse;

import :archetypes;
import :stacks;

export namespace sandbox {
	struct parity_grid_spec {
		std::string_view prefix;
		gse::length floor_extent;
		int side;
		float spacing;
		bool hulls = false;
	};

	[[nodiscard]] auto parity_grid_fixture(
		const parity_grid_spec& spec
	) -> gse::scene::setup_fn;

	auto parity_drop_scene_setup(
		gse::scene& s
	) -> void;

	auto parity_pair_scene_setup(
		gse::scene& s
	) -> void;

	auto parity_stack_scene_setup(
		gse::scene& s
	) -> void;

	auto parity_shapes_scene_setup(
		gse::scene& s
	) -> void;

	auto parity_domino_scene_setup(
		gse::scene& s
	) -> void;
}

namespace sandbox {
	auto parity_floor(
		gse::scene& s,
		const gse::vec3<gse::length>& size
	) -> void;

	auto parity_unit_cube() -> gse::vec3<gse::length>;
}

auto sandbox::parity_floor(gse::scene& s, const gse::vec3<gse::length>& size) -> void {
	s.spawn(
		"ParityFloor",
		static_box(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(-0.5f), gse::meters(0.f)),
			size
		)
	);
}

auto sandbox::parity_unit_cube() -> gse::vec3<gse::length> {
	return gse::vec3<gse::length>(gse::meters(0.5f), gse::meters(0.5f), gse::meters(0.5f));
}

auto sandbox::parity_grid_fixture(const parity_grid_spec& spec) -> gse::scene::setup_fn {
	return [spec](gse::scene& s) -> void {
		parity_floor(s, gse::vec3<gse::length>(spec.floor_extent, gse::meters(1.f), spec.floor_extent));

		const auto size = parity_unit_cube();
		for (int x = 0; x < spec.side; ++x) {
			for (int y = 0; y < spec.side; ++y) {
				for (int z = 0; z < spec.side; ++z) {
					const float px = (static_cast<float>(x) - static_cast<float>(spec.side - 1) * 0.5f) * spec.spacing;
					const float pz = (static_cast<float>(z) - static_cast<float>(spec.side - 1) * 0.5f) * spec.spacing;
					const float py = 0.5f + static_cast<float>(y) * spec.spacing;
					const auto position = gse::vec3<gse::position>(gse::meters(px), gse::meters(py), gse::meters(pz));
					const auto name = std::format("{}_{}_{}_{}", spec.prefix, x, y, z);

					if (spec.hulls) {
						s.spawn(name, box_as_hull(position, size));
					}
					else {
						s.spawn(name, box(position, size));
					}
				}
			}
		}
	};
}

auto sandbox::parity_drop_scene_setup(gse::scene& s) -> void {
	parity_floor(s, gse::meters(20.f, 1.f, 20.f));

	s.spawn(
		"ParityFaller",
		box(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(2.f), gse::meters(0.f)),
			parity_unit_cube()
		)
	);
}

auto sandbox::parity_pair_scene_setup(gse::scene& s) -> void {
	parity_floor(s, gse::meters(20.f, 1.f, 20.f));

	s.spawn(
		"ParityResting",
		box(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(0.5f), gse::meters(0.f)),
			parity_unit_cube()
		)
	);

	s.spawn(
		"ParityFaller",
		box(
			gse::vec3<gse::position>(gse::meters(0.f), gse::meters(3.f), gse::meters(0.f)),
			parity_unit_cube()
		)
	);
}

auto sandbox::parity_stack_scene_setup(gse::scene& s) -> void {
	parity_floor(s, gse::meters(20.f, 1.f, 20.f));

	for (int i = 0; i < 8; ++i) {
		s.spawn(
			std::format("ParityStack_{}", i),
			box(
				gse::vec3<gse::position>(gse::meters(0.f), gse::meters(0.5f + static_cast<float>(i) * 1.02f), gse::meters(0.f)),
				parity_unit_cube()
			)
		);
	}
}

auto sandbox::parity_shapes_scene_setup(gse::scene& s) -> void {
	parity_floor(s, gse::meters(60.f, 1.f, 20.f));

	const auto axis_along_z = gse::quat(gse::axis_x, gse::degrees(90.f));
	const auto axis_along_x = gse::quat(gse::axis_z, gse::degrees(90.f));

	const auto unit_box = gse::vec3<gse::length>(gse::meters(1.f), gse::meters(1.f), gse::meters(1.f));
	const auto sphere_radius = gse::meters(0.5f);
	const auto capsule_radius = gse::meters(0.35f);
	const auto capsule_half_height = gse::meters(0.5f);

	s.spawn(
		"ParityShapesBoxBox_support",
		static_box(gse::vec3<gse::position>(gse::meters(-15.f), gse::meters(0.5f), gse::meters(0.f)), unit_box)
	);
	s.spawn(
		"ParityShapesBoxBox_body",
		box(gse::vec3<gse::position>(gse::meters(-15.f), gse::meters(1.55f), gse::meters(0.f)), unit_box)
	);

	s.spawn(
		"ParityShapesHullBox_support",
		static_box(gse::vec3<gse::position>(gse::meters(-21.f), gse::meters(0.5f), gse::meters(0.f)), unit_box)
	);
	s.spawn(
		"ParityShapesHullBox_body",
		box_as_hull(gse::vec3<gse::position>(gse::meters(-21.f), gse::meters(1.55f), gse::meters(0.f)), unit_box)
	);

	s.spawn(
		"ParityShapesBoxSphere_support",
		static_box(gse::vec3<gse::position>(gse::meters(-9.f), gse::meters(0.5f), gse::meters(0.f)), unit_box)
	);
	s.spawn(
		"ParityShapesBoxSphere_body",
		sphere(gse::vec3<gse::position>(gse::meters(-9.f), gse::meters(1.55f), gse::meters(0.f)), sphere_radius)
	);

	s.spawn(
		"ParityShapesBoxCapsule_support",
		static_box(
			gse::vec3<gse::position>(gse::meters(-3.f), gse::meters(0.5f), gse::meters(0.f)),
			gse::vec3<gse::length>(gse::meters(1.6f), gse::meters(1.f), gse::meters(1.6f))
		)
	);
	s.spawn(
		"ParityShapesBoxCapsule_body",
		capsule(
			gse::vec3<gse::position>(gse::meters(-3.f), gse::meters(1.4f), gse::meters(0.f)),
			capsule_radius,
			capsule_half_height,
			gse::kilograms(1000.f),
			axis_along_z
		)
	);

	s.spawn(
		"ParityShapesSphereSphere_support_a",
		static_sphere(gse::vec3<gse::position>(gse::meters(2.45f), gse::meters(0.5f), gse::meters(0.f)), sphere_radius)
	);
	s.spawn(
		"ParityShapesSphereSphere_support_b",
		static_sphere(gse::vec3<gse::position>(gse::meters(3.55f), gse::meters(0.5f), gse::meters(0.f)), sphere_radius)
	);
	s.spawn(
		"ParityShapesSphereSphere_body",
		sphere(gse::vec3<gse::position>(gse::meters(3.f), gse::meters(1.4f), gse::meters(0.f)), sphere_radius)
	);

	s.spawn(
		"ParityShapesSphereCapsule_support_a",
		static_capsule(
			gse::vec3<gse::position>(gse::meters(8.55f), gse::meters(0.5f), gse::meters(0.f)),
			capsule_radius,
			capsule_half_height,
			axis_along_z
		)
	);
	s.spawn(
		"ParityShapesSphereCapsule_support_b",
		static_capsule(
			gse::vec3<gse::position>(gse::meters(9.45f), gse::meters(0.5f), gse::meters(0.f)),
			capsule_radius,
			capsule_half_height,
			axis_along_z
		)
	);
	s.spawn(
		"ParityShapesSphereCapsule_body",
		sphere(gse::vec3<gse::position>(gse::meters(9.f), gse::meters(1.28f), gse::meters(0.f)), sphere_radius)
	);

	s.spawn(
		"ParityShapesCapsuleCapsule_support_a",
		static_capsule(
			gse::vec3<gse::position>(gse::meters(14.55f), gse::meters(0.5f), gse::meters(0.f)),
			capsule_radius,
			capsule_half_height,
			axis_along_z
		)
	);
	s.spawn(
		"ParityShapesCapsuleCapsule_support_b",
		static_capsule(
			gse::vec3<gse::position>(gse::meters(15.45f), gse::meters(0.5f), gse::meters(0.f)),
			capsule_radius,
			capsule_half_height,
			axis_along_z
		)
	);
	s.spawn(
		"ParityShapesCapsuleCapsule_body",
		capsule(
			gse::vec3<gse::position>(gse::meters(15.f), gse::meters(1.26f), gse::meters(0.f)),
			capsule_radius,
			capsule_half_height,
			gse::kilograms(1000.f),
			axis_along_x
		)
	);
}

auto sandbox::parity_domino_scene_setup(gse::scene& s) -> void {
	parity_floor(s, gse::meters(20.f, 1.f, 20.f));

	spawn_domino_chain(s, 0, gse::vec3<gse::position>(gse::meters(-5.f, 0.f, 0.f)));
}

export module gs:physics_joint_test_scene;

import std;
import gse;

import :player;
import :entity_builders;

export namespace gs {
	class physics_joint_test_scene final : public gse::hook<gse::scene> {
	public:
		using hook::hook;

		auto initialize(
		) -> void override;
	private:
		auto build_fixed_joint_test(
		) const -> void;

		auto build_distance_pendulum(
		) const -> void;

		auto build_hinge_door(
		) const -> void;

		auto build_slider_elevator(
		) const -> void;

		auto build_pendulum_chain(
		) const -> void;
	};
}

auto gs::physics_joint_test_scene::initialize() -> void {
	const auto floor_pos = gse::vec3<gse::position>(0.f, -0.5f, 0.f);
	build_static_box(this, "Floor", floor_pos, gse::vec3<gse::length>(80.f, 1.f, 80.f));

	build_fixed_joint_test();
	build_distance_pendulum();
	build_hinge_door();
	build_slider_elevator();
	build_pendulum_chain();

	build_sphere(this, "Sphere Ball",
		gse::vec3<gse::position>(0.f, 6.f, -8.f),
		gse::meters(1.f));

	build("Player")
		.with<gs::player::component>({
			.initial_position = gse::vec3<gse::position>(0.f, 10.f, 0.f),
		});

	build("Scene Camera")
		.with<gse::free_camera::component>({
			.initial_position = gse::vec3<gse::position>(0.f, 15.f, 30.f),
		});

	build_sphere_light(this, "Scene Light",
		gse::vec3<gse::position>(0.f, 30.f, 0.f),
		gse::meters(0.5f),
		12,
		8);
}

auto gs::physics_joint_test_scene::build_fixed_joint_test() const -> void {
	constexpr float x = -20.f;
	constexpr float z = 0.f;

	const auto anchor_id = build_static_box(this, "Fixed Anchor",
		gse::vec3<gse::position>(x, 5.f, z),
		gse::vec3<gse::length>(gse::meters(1.f))).identify();

	const auto hanging_id = build_box(this, "Fixed Hanging Box",
		gse::vec3<gse::position>(x, 3.5f, z),
		gse::vec3<gse::length>(gse::meters(1.f)),
		gse::kilograms(20.f)).identify();

	gse::physics::join(anchor_id, hanging_id, gse::physics::fixed_joint{
		.anchor_a = gse::vec3<gse::displacement>(0.f, -0.5f, 0.f),
		.anchor_b = gse::vec3<gse::displacement>(0.f, 0.5f, 0.f),
	});
}

auto gs::physics_joint_test_scene::build_distance_pendulum() const -> void {
	constexpr float x = -10.f;
	constexpr float z = 0.f;

	const auto pivot_id = build_static_box(this, "Distance Pivot",
		gse::vec3<gse::position>(x, 8.f, z),
		gse::vec3<gse::length>(0.5f, 0.5f, 0.5f)).identify();

	const auto bob_id = build_box(this, "Distance Bob",
		gse::vec3<gse::position>(x + 3.f, 5.f, z),
		gse::vec3<gse::length>(gse::meters(1.f)),
		gse::kilograms(30.f)).identify();

	gse::physics::join(pivot_id, bob_id, gse::physics::distance_joint{
		.target = gse::meters(4.f),
	});
}

auto gs::physics_joint_test_scene::build_hinge_door() const -> void {
	constexpr float x = 0.f;
	constexpr float z = 0.f;

	const auto frame_id = build_static_box(this, "Hinge Frame",
		gse::vec3<gse::position>(x, 2.f, z),
		gse::vec3<gse::length>(0.3f, 4.f, 0.3f)).identify();

	const auto door_id = build_box(this, "Hinge Door",
		gse::vec3<gse::position>(x + 1.5f, 2.f, z),
		gse::vec3<gse::length>(3.f, 3.5f, 0.2f),
		gse::kilograms(40.f))
		.configure([](gse::physics::motion_component& mc) {
			mc.affected_by_gravity = false;
		})
		.identify();

	gse::physics::join(frame_id, door_id, gse::physics::hinge_joint{
		.anchor_a = gse::vec3<gse::displacement>(0.15f, 0.f, 0.f),
		.anchor_b = gse::vec3<gse::displacement>(-1.5f, 0.f, 0.f),
		.axis = { 0.f, 1.f, 0.f },
		.limits = std::pair{ gse::radians(-1.57f), gse::radians(1.57f) },
	});
}

auto gs::physics_joint_test_scene::build_slider_elevator() const -> void {
	constexpr float x = 10.f;
	constexpr float z = 0.f;

	const auto rail_id = build_static_box(this, "Slider Rail",
		gse::vec3<gse::position>(x, 4.f, z),
		gse::vec3<gse::length>(0.3f, 8.f, 0.3f)).identify();

	const auto platform_id = build_box(this, "Slider Platform",
		gse::vec3<gse::position>(x, 6.f, z),
		gse::vec3<gse::length>(2.f, 0.3f, 2.f),
		gse::kilograms(30.f)).identify();

	gse::physics::join(rail_id, platform_id, gse::physics::slider_joint{
		.axis = { 0.f, 1.f, 0.f },
	});
}

auto gs::physics_joint_test_scene::build_pendulum_chain() const -> void {
	constexpr float x = 20.f;
	constexpr float z = 0.f;
	constexpr int chain_length = 5;
	constexpr float link_spacing = 1.5f;

	const auto ceiling_id = build_static_box(this, "Chain Ceiling",
		gse::vec3<gse::position>(x, 12.f, z),
		gse::vec3<gse::length>(1.f, 0.5f, 1.f)).identify();

	std::vector<gse::id> link_ids;
	link_ids.push_back(ceiling_id);

	for (int i = 0; i < chain_length; ++i) {
		const float y = 12.f - (static_cast<float>(i) + 1.f) * link_spacing;
		const float x_offset = (i == chain_length - 1) ? 2.f : 0.f;
		const auto link_id = build_box(this, std::format("Chain Link {}", i),
			gse::vec3<gse::position>(x + x_offset, y, z),
			gse::vec3<gse::length>(0.6f, 0.6f, 0.6f),
			gse::kilograms(15.f)).identify();
		link_ids.push_back(link_id);
	}

	for (std::size_t i = 0; i + 1 < link_ids.size(); ++i) {
		gse::physics::join(link_ids[i], link_ids[i + 1], gse::physics::distance_joint{
			.target = gse::meters(link_spacing),
		});
	}
}

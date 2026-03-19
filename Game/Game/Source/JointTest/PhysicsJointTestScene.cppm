export module gs:physics_joint_test_scene;

import std;
import gse;

import :player;
import :sphere_light;

export namespace gs {
	class physics_joint_test_scene final : public gse::hook<gse::scene> {
	public:
		using hook::hook;

		auto initialize() -> void override {
			const auto floor_pos = gse::vec3<gse::length>(0.f, -0.5f, 0.f);
			build("Floor")
				.with<gse::box>({
					.initial_position = floor_pos,
					.size = gse::vec3<gse::length>(80.f, 1.f, 80.f)
				})
				.with_init([](hook<gse::entity>& h) {
					h.configure_when_present([](gse::physics::motion_component& mc) {
						mc.affected_by_gravity = false;
						mc.position_locked = true;
					});
				})
				.with_update([floor_pos](hook<gse::entity>& h) {
					h.component_write<gse::physics::motion_component>().current_position = floor_pos;
				});

			build_fixed_joint_test();
			build_distance_pendulum();
			build_hinge_door();
			build_slider_elevator();
			build_pendulum_chain();

			build("Player")
				.with<player>({
					.initial_position = gse::vec3<gse::length>(0.f, 10.f, 0.f)
				});

			build("Scene Camera")
				.with<gse::free_camera>({
					.initial_position = gse::vec3<gse::length>(0.f, 15.f, 30.f)
				});

			build("Scene Light")
				.with<sphere_light>({
					.initial_position = gse::vec3<gse::length>(0.f, 30.f, 0.f),
					.radius = gse::meters(0.5f),
					.sectors = 12,
					.stacks = 8
				});
		}

	private:
		auto build_fixed_joint_test() const -> void {
			constexpr float x = -20.f;
			constexpr float z = 0.f;

			const auto anchor_id = build("Fixed Anchor")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(x, 5.f, z),
					.size = gse::vec3<gse::length>(gse::meters(1.f)),
					.mass = gse::kilograms(100.f)
				})
				.with_init([](hook<gse::entity>& h) {
					h.configure_when_present([](gse::physics::motion_component& mc) {
						mc.affected_by_gravity = false;
						mc.position_locked = true;
					});
				})
				.identify();

			const auto hanging_id = build("Fixed Hanging Box")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(x, 3.5f, z),
					.size = gse::vec3<gse::length>(gse::meters(1.f)),
					.mass = gse::kilograms(20.f)
				})
				.identify();

			gse::physics::join(anchor_id, hanging_id, gse::physics::fixed_joint{
				.anchor_a = gse::vec3<gse::length>(0.f, -0.5f, 0.f),
				.anchor_b = gse::vec3<gse::length>(0.f, 0.5f, 0.f),
			});
		}

		auto build_distance_pendulum() const -> void {
			constexpr float x = -10.f;
			constexpr float z = 0.f;

			const auto pivot_id = build("Distance Pivot")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(x, 8.f, z),
					.size = gse::vec3<gse::length>(0.5f, 0.5f, 0.5f),
					.mass = gse::kilograms(100.f)
				})
				.with_init([](hook<gse::entity>& h) {
					h.configure_when_present([](gse::physics::motion_component& mc) {
						mc.affected_by_gravity = false;
						mc.position_locked = true;
					});
				})
				.identify();

			const auto bob_id = build("Distance Bob")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(x + 3.f, 5.f, z),
					.size = gse::vec3<gse::length>(gse::meters(1.f)),
					.mass = gse::kilograms(30.f)
				})
				.identify();

			gse::physics::join(pivot_id, bob_id, gse::physics::distance_joint{
				.target = gse::meters(4.f),
			});
		}

		auto build_hinge_door() const -> void {
			constexpr float x = 0.f;
			constexpr float z = 0.f;

			const auto frame_id = build("Hinge Frame")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(x, 2.f, z),
					.size = gse::vec3<gse::length>(0.3f, 4.f, 0.3f),
					.mass = gse::kilograms(500.f)
				})
				.with_init([](hook<gse::entity>& h) {
					h.configure_when_present([](gse::physics::motion_component& mc) {
						mc.affected_by_gravity = false;
						mc.position_locked = true;
					});
				})
				.identify();

			const auto door_id = build("Hinge Door")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(x + 1.5f, 2.f, z),
					.size = gse::vec3<gse::length>(3.f, 3.5f, 0.2f),
					.mass = gse::kilograms(40.f)
				})
				.with_init([](hook<gse::entity>& h) {
					h.configure_when_present([](gse::physics::motion_component& mc) {
						mc.affected_by_gravity = false;
					});
				})
				.identify();

			gse::physics::join(frame_id, door_id, gse::physics::hinge_joint{
				.anchor_a = gse::vec3<gse::length>(0.15f, 0.f, 0.f),
				.anchor_b = gse::vec3<gse::length>(-1.5f, 0.f, 0.f),
				.axis = { 0.f, 1.f, 0.f },
				.limits = std::pair{ gse::radians(-1.57f), gse::radians(1.57f) },
			});
		}

		auto build_slider_elevator() const -> void {
			constexpr float x = 10.f;
			constexpr float z = 0.f;

			const auto rail_id = build("Slider Rail")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(x, 4.f, z),
					.size = gse::vec3<gse::length>(0.3f, 8.f, 0.3f),
					.mass = gse::kilograms(500.f)
				})
				.with_init([](hook<gse::entity>& h) {
					h.configure_when_present([](gse::physics::motion_component& mc) {
						mc.affected_by_gravity = false;
						mc.position_locked = true;
					});
				})
				.identify();

			const auto platform_id = build("Slider Platform")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(x, 6.f, z),
					.size = gse::vec3<gse::length>(2.f, 0.3f, 2.f),
					.mass = gse::kilograms(30.f)
				})
				.identify();

			gse::physics::join(rail_id, platform_id, gse::physics::slider_joint{
				.axis = { 0.f, 1.f, 0.f },
			});
		}

		auto build_pendulum_chain() const -> void {
			constexpr float x = 20.f;
			constexpr float z = 0.f;
			constexpr int chain_length = 5;
			constexpr float link_spacing = 1.5f;

			const auto ceiling_id = build("Chain Ceiling")
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(x, 12.f, z),
					.size = gse::vec3<gse::length>(1.f, 0.5f, 1.f),
					.mass = gse::kilograms(500.f)
				})
				.with_init([](hook<gse::entity>& h) {
					h.configure_when_present([](gse::physics::motion_component& mc) {
						mc.affected_by_gravity = false;
						mc.position_locked = true;
					});
				})
				.identify();

			std::vector<gse::id> link_ids;
			link_ids.push_back(ceiling_id);

			for (int i = 0; i < chain_length; ++i) {
				const float y = 12.f - (static_cast<float>(i) + 1.f) * link_spacing;
				const float x_offset = (i == chain_length - 1) ? 2.f : 0.f;
				const auto link_id = build(std::format("Chain Link {}", i))
					.with<gse::box>({
						.initial_position = gse::vec3<gse::length>(x + x_offset, y, z),
						.size = gse::vec3<gse::length>(0.6f, 0.6f, 0.6f),
						.mass = gse::kilograms(15.f)
					})
					.identify();
				link_ids.push_back(link_id);
			}

			for (std::size_t i = 0; i + 1 < link_ids.size(); ++i) {
				gse::physics::join(link_ids[i], link_ids[i + 1], gse::physics::distance_joint{
					.target = gse::meters(link_spacing),
				});
			}
		}
	};
}

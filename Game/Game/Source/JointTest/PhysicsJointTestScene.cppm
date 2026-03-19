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

			const auto anchor_id = m_owner->add_entity("Fixed Anchor");
			builder(anchor_id, &m_owner->registry())
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
				});

			const auto hanging_id = m_owner->add_entity("Fixed Hanging Box");
			builder(hanging_id, &m_owner->registry())
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(x, 3.5f, z),
					.size = gse::vec3<gse::length>(gse::meters(1.f)),
					.mass = gse::kilograms(20.f)
				});

			gse::defer<gse::physics::state>([anchor_id, hanging_id](gse::physics::state& s) {
				gse::physics::create_joint(s, gse::physics::joint_definition{
					.entity_a = anchor_id,
					.entity_b = hanging_id,
					.type = gse::vbd::joint_type::fixed,
					.local_anchor_a = gse::vec3<gse::length>(0.f, -0.5f, 0.f),
					.local_anchor_b = gse::vec3<gse::length>(0.f, 0.5f, 0.f),
				});
			});
		}

		auto build_distance_pendulum() const -> void {
			constexpr float x = -10.f;
			constexpr float z = 0.f;

			const auto pivot_id = m_owner->add_entity("Distance Pivot");
			builder(pivot_id, &m_owner->registry())
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
				});

			const auto bob_id = m_owner->add_entity("Distance Bob");
			builder(bob_id, &m_owner->registry())
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(x + 3.f, 5.f, z),
					.size = gse::vec3<gse::length>(gse::meters(1.f)),
					.mass = gse::kilograms(30.f)
				});

			gse::defer<gse::physics::state>([pivot_id, bob_id](gse::physics::state& s) {
				gse::physics::create_joint(s, gse::physics::joint_definition{
					.entity_a = pivot_id,
					.entity_b = bob_id,
					.type = gse::vbd::joint_type::distance,
					.target_distance = gse::meters(4.f),
				});
			});
		}

		auto build_hinge_door() const -> void {
			constexpr float x = 0.f;
			constexpr float z = 0.f;

			const auto frame_id = m_owner->add_entity("Hinge Frame");
			builder(frame_id, &m_owner->registry())
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
				});

			const auto door_id = m_owner->add_entity("Hinge Door");
			builder(door_id, &m_owner->registry())
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(x + 1.5f, 2.f, z),
					.size = gse::vec3<gse::length>(3.f, 3.5f, 0.2f),
					.mass = gse::kilograms(40.f)
				})
				.with_init([](hook<gse::entity>& h) {
					h.configure_when_present([](gse::physics::motion_component& mc) {
						mc.affected_by_gravity = false;
					});
				});

			gse::defer<gse::physics::state>([frame_id, door_id](gse::physics::state& s) {
				gse::physics::create_joint(s, gse::physics::joint_definition{
					.entity_a = frame_id,
					.entity_b = door_id,
					.type = gse::vbd::joint_type::hinge,
					.local_anchor_a = gse::vec3<gse::length>(0.15f, 0.f, 0.f),
					.local_anchor_b = gse::vec3<gse::length>(-1.5f, 0.f, 0.f),
					.local_axis_a = { 0.f, 1.f, 0.f },
					.local_axis_b = { 0.f, 1.f, 0.f },
					.limit_lower = gse::radians(-1.57f),
					.limit_upper = gse::radians(1.57f),
					.limits_enabled = true,
				});
			});
		}

		auto build_slider_elevator() const -> void {
			constexpr float x = 10.f;
			constexpr float z = 0.f;

			const auto rail_id = m_owner->add_entity("Slider Rail");
			builder(rail_id, &m_owner->registry())
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
				});

			const auto platform_id = m_owner->add_entity("Slider Platform");
			builder(platform_id, &m_owner->registry())
				.with<gse::box>({
					.initial_position = gse::vec3<gse::length>(x, 6.f, z),
					.size = gse::vec3<gse::length>(2.f, 0.3f, 2.f),
					.mass = gse::kilograms(30.f)
				});

			gse::defer<gse::physics::state>([rail_id, platform_id](gse::physics::state& s) {
				gse::physics::create_joint(s, gse::physics::joint_definition{
					.entity_a = rail_id,
					.entity_b = platform_id,
					.type = gse::vbd::joint_type::slider,
					.local_axis_a = { 0.f, 1.f, 0.f },
					.local_axis_b = { 0.f, 1.f, 0.f },
				});
			});
		}

		auto build_pendulum_chain() const -> void {
			constexpr float x = 20.f;
			constexpr float z = 0.f;
			constexpr int chain_length = 5;
			constexpr float link_spacing = 1.5f;

			const auto ceiling_id = m_owner->add_entity("Chain Ceiling");
			builder(ceiling_id, &m_owner->registry())
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
				});

			std::vector<gse::id> link_ids;
			link_ids.push_back(ceiling_id);

			for (int i = 0; i < chain_length; ++i) {
				const float y = 12.f - (static_cast<float>(i) + 1.f) * link_spacing;
				const float x_offset = (i == chain_length - 1) ? 2.f : 0.f;
				const auto link_id = m_owner->add_entity(std::format("Chain Link {}", i));
				builder(link_id, &m_owner->registry())
					.with<gse::box>({
						.initial_position = gse::vec3<gse::length>(x + x_offset, y, z),
						.size = gse::vec3<gse::length>(0.6f, 0.6f, 0.6f),
						.mass = gse::kilograms(15.f)
					});
				link_ids.push_back(link_id);
			}

			gse::defer<gse::physics::state>([link_ids, link_spacing](gse::physics::state& s) {
				for (std::size_t i = 0; i + 1 < link_ids.size(); ++i) {
					gse::physics::create_joint(s, gse::physics::joint_definition{
						.entity_a = link_ids[i],
						.entity_b = link_ids[i + 1],
						.type = gse::vbd::joint_type::distance,
						.target_distance = gse::meters(link_spacing),
					});
				}
			});
		}
	};
}

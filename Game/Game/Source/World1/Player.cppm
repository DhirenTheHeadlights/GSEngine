export module gs:player;

import gse;
import std;

namespace gs {
	class jetpack_hook final : public gse::hook<gse::entity> {
	public:
		using hook::hook;

		auto initialize() -> void override {
			m_toggle_jetpack = gse::actions::add<"Toggle_Jetpack">(gse::key::j);
			m_thrust = gse::actions::add<"Jetpack_Thrust">(gse::key::space);
			m_boost = gse::actions::add<"Jetpack_Boost">(gse::key::left_shift);

			m_w = gse::actions::add<"Jetpack_Move_Forward">(gse::key::w);
			m_a = gse::actions::add<"Jetpack_Move_Left">(gse::key::a);
			m_s = gse::actions::add<"Jetpack_Move_Backward">(gse::key::s);
			m_d = gse::actions::add<"Jetpack_Move_Right">(gse::key::d);

			m_move_axis = gse::actions::add_axis2_actions({
				.left = m_a,
				.right = m_d,
				.back = m_s,
				.fwd = m_w,
				.scale = 1.f,
				.id = gse::find_or_generate_id("Jetpack_Move")
			});
		}

		auto update() -> void override {
			if (gse::actions::pressed(m_toggle_jetpack)) {
				m_jetpack = !m_jetpack;
			}

			if (m_jetpack && gse::actions::held(m_thrust)) {
				gse::force boost_force;
				if (gse::actions::held(m_boost) && m_boost_fuel > 0) {
					boost_force = gse::newtons(2000.f);
					m_boost_fuel -= 1;
				}
				else {
					m_boost_fuel += 1;
					m_boost_fuel = std::min(m_boost_fuel, 1000);
				}

				auto motion_component = component_write<gse::physics::motion_component>();

				apply_force(motion_component, gse::vec3<gse::force>(0.f, m_jetpack_force + boost_force, 0.f));

				const auto v = gse::actions::axis2_v(m_move_axis);
				const auto dir = gse::renderer::camera().direction_relative_to_origin(gse::unitless::vec3(v.x(), 0.f, v.y()));
				if (v.x() != 0.f || v.y() != 0.f) {
					const auto f = m_jetpack_side_force + boost_force;
					apply_force(motion_component, gse::vec3<gse::force>(f * dir.x(), 0.f, f * dir.z()));
				}
			}
		}
	private:
		gse::force m_jetpack_force = gse::newtons(1000.f);
		gse::force m_jetpack_side_force = gse::newtons(500.f);

		int m_boost_fuel = 1000;
		bool m_jetpack = false;

		gse::actions::index m_toggle_jetpack, m_thrust, m_boost, m_w, m_a, m_s, m_d;
		gse::id m_move_axis;
	};

	class player_hook final : public gse::hook<gse::entity> {
	public:
		using hook::hook;

		auto initialize() -> void override {
			add_component<gse::physics::motion_component>({
				.current_position = gse::vec3<gse::length>(0.f, 0.f, 0.f),
				.max_speed = m_max_speed,
				.mass = gse::pounds(180.f),
				.self_controlled = true,
				});

			gse::length height = gse::feet(6.0f);
			gse::length width = gse::feet(3.0f);

			add_component<gse::physics::collision_component>({
				.bounding_box = { gse::vec3<gse::length>(-10.f, -10.f, -10.f), { width, height, width } }
			});

			add_component<gse::render_component>({});

			m_w = gse::actions::add<"Player_Move_Forward">(gse::key::w);
			m_a = gse::actions::add<"Player_Move_Left">(gse::key::a);
			m_s = gse::actions::add<"Player_Move_Backward">(gse::key::s);
			m_d = gse::actions::add<"Player_Move_Right">(gse::key::d);

			m_move_axis = gse::actions::add_axis2_actions({
				.left = m_a,
				.right = m_d,
				.back = m_s,
				.fwd = m_w,
				.scale = 1.f,
				.id = gse::find_or_generate_id("Player_Move")
			});
		}

		auto update() -> void override {
			auto& motion_component = component_write<gse::physics::motion_component>();

			if (const auto v = gse::actions::axis2_v(m_move_axis); (v.x() != 0.f || v.y() != 0.f) && !motion_component.airborne) {
				const auto dir = gse::renderer::camera().direction_relative_to_origin(gse::unitless::vec3(v.x(), 0.f, v.y()));
				apply_force(motion_component, m_move_force * gse::unitless::vec3(dir.x(), 0.f, dir.z()));
			}

			motion_component.max_speed = gse::keyboard::held(gse::key::left_shift) ? m_shift_max_speed : m_max_speed;

			if (gse::keyboard::pressed(gse::key::space) && !motion_component.airborne) {
				apply_impulse(motion_component, gse::vec3<gse::force>(0.f, m_jump_force, 0.f), gse::seconds(0.5f));
			}

			gse::renderer::camera().set_position(motion_component.current_position + gse::vec3<gse::length>(gse::feet(0.f), gse::feet(6.f), gse::feet(0.f)));
		}
	private:
		gse::velocity m_max_speed = gse::miles_per_hour(20.f);
		gse::velocity m_shift_max_speed = gse::miles_per_hour(40.f);

		gse::force m_move_force = gse::newtons(100000.f);
		gse::force m_jump_force = gse::newtons(1000.f);

		gse::actions::index m_w, m_a, m_s, m_d;
		gse::id m_move_axis;
	};

	class player final : public gse::hook<gse::entity> {
	public:
		using hook::hook;
		auto initialize() -> void override {
			add_hook<jetpack_hook>();
			add_hook<player_hook>();
		}
	};
}

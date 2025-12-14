export module gs:player;

import gse;
import std;

namespace gs {
	class jetpack_hook final : public gse::hook<gse::entity> {
	public:
		using hook::hook;

		auto initialize() -> void override {
			gse::actions::bind_button_channel<"Toggle_Jetpack">(
				owner_id(),
				gse::key::j,
				m_toggle_channel
			);

			gse::actions::bind_button_channel<"Jetpack_Thrust">(
				owner_id(),
				gse::key::space,
				m_thrust_channel
			);

			gse::actions::bind_button_channel<"Jetpack_Boost">(
				owner_id(),
				gse::key::left_shift,
				m_boost_channel
			);

			const auto w = gse::actions::add<"Jetpack_Move_Forward">(gse::key::w);
			const auto a = gse::actions::add<"Jetpack_Move_Left">(gse::key::a);
			const auto s = gse::actions::add<"Jetpack_Move_Backward">(gse::key::s);
			const auto d = gse::actions::add<"Jetpack_Move_Right">(gse::key::d);

			gse::actions::bind_axis2_channel(
				owner_id(),
				gse::actions::pending_axis2_info{
					.left = a,
					.right = d,
					.back = s,
					.fwd = w,
					.scale = 1.f
				},
				m_move_axis_channel,
				gse::find_or_generate_id("Jetpack_Move")
			);
		}

		auto update() -> void override {
			if (m_toggle_channel.pressed) {
				m_jetpack = !m_jetpack;
			}

			if (!m_jetpack || !m_thrust_channel.held) {
				return;
			}

			gse::force boost_force;

			if (m_boost_channel.held && m_boost_fuel > 0) {
				boost_force = gse::newtons(2000.f);
				m_boost_fuel -= 1;
			}
			else {
				m_boost_fuel += 1;
				m_boost_fuel = std::min(m_boost_fuel, 1000);
			}

			auto motion_component = component_write<gse::physics::motion_component>();

			apply_force(
				motion_component,
				gse::vec3<gse::force>(0.f, m_jetpack_force + boost_force, 0.f)
			);

			const auto v = m_move_axis_channel.value;

			if (v.x() == 0.f && v.y() == 0.f) {
				return;
			}

			const auto dir = gse::renderer::camera().direction_relative_to_origin(
				gse::unitless::vec3(v.x(), 0.f, v.y())
			);

			const auto f = m_jetpack_side_force + boost_force;

			apply_force(
				motion_component,
				gse::vec3<gse::force>(f * dir.x(), 0.f, f * dir.z())
			);
		}
	private:
		gse::force m_jetpack_force = gse::newtons(1000.f);
		gse::force m_jetpack_side_force = gse::newtons(500.f);

		int m_boost_fuel = 1000;
		bool m_jetpack = false;

		gse::actions::button_channel m_toggle_channel;
		gse::actions::button_channel m_thrust_channel;
		gse::actions::button_channel m_boost_channel;
		gse::actions::axis2_channel m_move_axis_channel;
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
				.update_orientation = false
			});

			gse::length height = gse::feet(6.0f);
			gse::length width = gse::feet(3.0f);

			add_component<gse::physics::collision_component>({
				.bounding_box = {
					gse::vec3<gse::length>(-10.f, -10.f, -10.f),
					{ width, height, width }
				}
			});

			add_component<gse::render_component>({});

			const auto w = gse::actions::add<"Player_Move_Forward">(gse::key::w);
			const auto a = gse::actions::add<"Player_Move_Left">(gse::key::a);
			const auto s = gse::actions::add<"Player_Move_Backward">(gse::key::s);
			const auto d = gse::actions::add<"Player_Move_Right">(gse::key::d);

			gse::actions::bind_button_channel<"Player_Sprint">(
				owner_id(),
				gse::key::left_shift,
				m_shift_channel
			);

			gse::actions::bind_button_channel<"Player_Jump">(
				owner_id(),
				gse::key::space,
				m_jump_channel
			);

			gse::actions::bind_axis2_channel(
				owner_id(),
				gse::actions::pending_axis2_info{
					.left = a,
					.right = d,
					.back = s,
					.fwd = w,
					.scale = 1.f
				},
				m_move_axis_channel,
				gse::find_or_generate_id("Player_Move")
			);
		}

		auto update() -> void override {
			auto& motion_component = component_write<gse::physics::motion_component>();

			const auto v = m_move_axis_channel.value;

			if (!motion_component.airborne && (v.x() != 0.f || v.y() != 0.f)) {
				const auto dir = gse::renderer::camera().direction_relative_to_origin(
					gse::unitless::vec3(v.x(), 0.f, v.y())
				);

				apply_force(
					motion_component,
					m_move_force * gse::unitless::vec3(dir.x(), 0.f, dir.z())
				);
			}

			motion_component.max_speed = m_shift_channel.held ? m_shift_max_speed : m_max_speed;

			if (m_jump_channel.pressed && !motion_component.airborne) {
				apply_impulse(
					motion_component,
					gse::vec3<gse::force>(0.f, m_jump_force, 0.f),
					gse::seconds(0.5f)
				);
			}

			gse::renderer::camera().set_position(
				motion_component.current_position + gse::vec3<gse::length>(
					gse::feet(0.f),
					gse::feet(6.f),
					gse::feet(0.f)
				)
			);
		}
	private:
		gse::velocity m_max_speed = gse::miles_per_hour(20.f);
		gse::velocity m_shift_max_speed = gse::miles_per_hour(40.f);

		gse::force m_move_force = gse::newtons(100000.f);
		gse::force m_jump_force = gse::newtons(1000.f);

		gse::actions::button_channel m_shift_channel;
		gse::actions::button_channel m_jump_channel;
		gse::actions::axis2_channel m_move_axis_channel;
	};

	class player final : public gse::hook<gse::entity> {
	public:
		struct params {
			gse::vec3<gse::length> initial_position;
		};

		explicit player(const params& p) : m_initial_position(p.initial_position) {}

		auto initialize() -> void override {
			add_hook<jetpack_hook>();
			add_hook<player_hook>();
		}
	private:
		gse::vec3<gse::length> m_initial_position;
	};
}

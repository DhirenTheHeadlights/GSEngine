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

			const auto dir = gse::camera_direction_relative_to_origin(
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

			/*add_component<gse::render_component>({
				.skinned_models = {
					gse::get<gse::skinned_model>("SkinnedModels/player")
				}
			});

			add_component<gse::animation_component>({
				.skeleton_id = gse::find("Skeletons/player")
			});*/

			add_component<gse::camera::follow_component>({
				.offset = gse::vec3<gse::length>(gse::feet(0.f), gse::feet(6.f), gse::feet(0.f)),
				.priority = 50,
				.blend_in_duration = gse::milliseconds(300),
				.active = true,
				.use_entity_position = false
			});

			setup_input();
			//setup_animation();
		}

		auto update() -> void override {
			auto& motion = component_write<gse::physics::motion_component>();

			const auto v = m_move_axis_channel.value;
			const bool moving = v.x() != 0.f || v.y() != 0.f;
			motion.max_speed = m_shift_channel.held ? m_sprint_speed : m_max_speed;

			const auto dt = gse::system_clock::constant_update_time<gse::time_t<float, gse::seconds>>();

			if (!motion.airborne) {
				if (moving) {
					const auto dir = gse::camera_direction_relative_to_origin(
						gse::unitless::vec3(v.x(), 0.f, v.y())
					);
					const auto target = motion.max_speed * gse::unitless::vec3(dir.x(), 0.f, dir.z());
					const float t = std::min(1.f, m_ground_response * dt.as<gse::seconds>());
					motion.current_velocity.x() += (target.x() - motion.current_velocity.x()) * t;
					motion.current_velocity.z() += (target.z() - motion.current_velocity.z()) * t;
				} else {
					const float t = std::min(1.f, m_ground_stopping * dt.as<gse::seconds>());
					motion.current_velocity.x() *= (1.f - t);
					motion.current_velocity.z() *= (1.f - t);
				}
			} else if (moving) {
				// Air control: steer existing momentum toward desired direction, no speed gain
				const auto dir = gse::camera_direction_relative_to_origin(
					gse::unitless::vec3(v.x(), 0.f, v.y())
				);
				const auto hspeed = gse::magnitude(gse::vec3<gse::velocity>(
					motion.current_velocity.x(), gse::meters_per_second(0.f), motion.current_velocity.z()
				));
				const auto target = hspeed * gse::unitless::vec3(dir.x(), 0.f, dir.z());
				const float t = std::min(1.f, m_air_steering * dt.as<gse::seconds>());
				motion.current_velocity.x() += (target.x() - motion.current_velocity.x()) * t;
				motion.current_velocity.z() += (target.z() - motion.current_velocity.z()) * t;
			}

			// Wall-sliding: remove velocity component going into active collisions
			{
				const auto& coll = component_read<gse::physics::collision_component>();
				if (coll.collision_information.colliding) {
					const auto& n = coll.collision_information.collision_normal;
					const auto vn = n.x() * motion.current_velocity.x() + n.z() * motion.current_velocity.z();
					if (vn < gse::meters_per_second(0.f)) {
						motion.current_velocity.x() -= vn * n.x();
						motion.current_velocity.z() -= vn * n.z();
					}
				}
			}

			if (m_jump_channel.pressed && !motion.airborne) {
				motion.current_velocity.y() = m_jump_speed;
				motion.airborne = true;
			}

			// Clamp horizontal speed to max_speed (prevents runaway when airborne)
			{
				const auto hspeed = gse::magnitude(gse::vec3<gse::velocity>(
					motion.current_velocity.x(), gse::meters_per_second(0.f), motion.current_velocity.z()
				));
				if (hspeed > motion.max_speed) {
					const float scale = motion.max_speed / hspeed;
					motion.current_velocity.x() *= scale;
					motion.current_velocity.z() *= scale;
				}
			}

			m_bindings.update();

			auto& cam_follow = component_write<gse::camera::follow_component>();
			cam_follow.position = motion.current_position;
		}

	private:
		auto setup_animation() -> void {
			auto idle = gse::animation::state_handle{
				.name = "idle",
				.clip = gse::find("Clips/idle"),
				.loop = true
			};

			auto walk = gse::animation::state_handle{
				.name = "walk",
				.clip = gse::find("Clips/walk"),
				.loop = true
			};

			auto run  = gse::animation::state_handle{
				.name = "run",
				.clip = gse::find("Clips/run"),
				.loop = true
			};

			auto jump = gse::animation::state_handle{
				.name = "jump",
				.clip = gse::find("Clips/jump")
			};

			auto speed = gse::animation::float_param{ "speed" };
			auto airborne = gse::animation::bool_param{ "airborne" };
			auto sprinting = gse::animation::bool_param{ "sprinting" };
			auto jump_trig = gse::animation::trigger_handle{ "jump" };

			add_component<gse::controller_component>({
				.graph_id = gse::animation::build(
					graph("player_locomotion", idle)
						.transitions(
							idle >> walk && speed > 0.1f && !airborne,
							walk >> idle && speed < 0.1f,
							walk >> run  && sprinting,
							run  >> walk && !sprinting,
							run  >> idle && speed < 0.1f,
							gse::animation::any  >> jump && jump_trig,
							jump >> idle && !airborne && gse::animation::after(0.5f)
						)
				)
			});

			auto& motion = component_read<gse::physics::motion_component>();
			auto& ctrl = component_write<gse::controller_component>();

			m_bindings = gse::animation::bind(ctrl)
				.bind(speed, [&motion] {
					return gse::magnitude(motion.current_velocity).as<gse::meters_per_second>();
				})
				.bind(airborne, motion.airborne)
				.bind(sprinting, m_shift_channel.held)
				.on_trigger(jump_trig, [this] {
					auto& m = component_read<gse::physics::motion_component>();
					return m_jump_channel.pressed && !m.airborne;
				});
		}

		auto setup_input() -> void {
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

		gse::animation::bindings m_bindings;

		gse::velocity m_max_speed = gse::miles_per_hour(20.f);
		gse::velocity m_sprint_speed = gse::miles_per_hour(40.f);

		float m_ground_response = 25.f;
		float m_ground_stopping = 20.f;
		float m_air_steering = 3.f;
		gse::velocity m_jump_speed = gse::meters_per_second(7.f);

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

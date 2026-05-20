export module gs:player;

import std;
import gse;

import :balance;
import :controlled_joint;

export namespace gs::player {
	enum class swing_leg : std::uint8_t { left, right };
	enum class gait_state : std::uint8_t { idle, walking };

	struct component {
		gse::vec3<gse::position> initial_position;
		gse::id pelvis_id;
		gse::id hip_l_id;
		gse::id knee_l_id;
		gse::id hip_r_id;
		gse::id knee_r_id;
		gse::id foot_l_id;
		gse::id foot_r_id;
		gait_state gait = gait_state::idle;
		swing_leg current_swing = swing_leg::left;
		gse::time swing_elapsed = gse::seconds(0.f);
		gse::time swing_duration = gse::seconds(0.30f);
		float swing_hip_start = 0.f;
		float swing_hip_end = 0.f;
		float gait_intensity = 0.f;
		gse::velocity walk_speed = gse::meters_per_second(1.5f);
		gse::velocity sprint_speed = gse::meters_per_second(3.0f);
		gse::velocity jump_speed = gse::meters_per_second(7.f);
		gse::force jetpack_thrust = gse::newtons(1000.f);
		gse::force jetpack_side_force = gse::newtons(500.f);
		int boost_fuel_max = 1000;
		int boost_fuel = 1000;
		bool jetpack_enabled = false;
		gse::angle yaw = gse::degrees(-90.f);
		gse::angle pitch = gse::degrees(0.f);
		float mouse_sensitivity = 0.1f;
	};

	constexpr std::array<float, 4> camera_distance_levels_m = { 0.f, 2.f, 4.f, 6.f };

	struct bindings {
		gse::actions::handle shift;
		gse::actions::handle jump;
		gse::actions::handle jetpack_toggle;
		gse::actions::handle jetpack_thrust;
		gse::actions::handle jetpack_boost;
		gse::id move_axis_id;
		gse::id jetpack_move_axis_id;
	};

	struct system {
		struct[[= gse::settings::category<"Player">{}]] data {
			std::unordered_map<gse::id, bindings> bindings_by_owner;

			[[
				= gse::settings::describe<"Camera follow distance level (0=first person, 1-3=third person).">{},
				= gse::settings::range<0, 3>{}
			]] int camera_level = 0;

			gse::interval_timer<float> gait_log_timer{ gse::seconds(0.3f) };
		};

		static auto run(
			gse::run_context& ctx,
			data& d,
			const gse::actions::system::data& as,
			const gse::input::system::data& input_s,
			const gse::camera::system::data& cam_s,
			const gse::physics::system::data& phys_s
		) -> gse::async::task<>;
	};
}

auto gs::player::system::run(
	gse::run_context& ctx,
	data& d,
	const gse::actions::system::data& as,
	const gse::input::system::data& input_s,
	const gse::camera::system::data& cam_s,
	const gse::physics::system::data& phys_s
) -> gse::async::task<> {
	while (true) {
		{
			auto [players, transforms, motions, motors, follows, ctrls, balances] = co_await ctx.acquire_with(
				gse::write_v<component>,
				gse::write_v<gse::physics::transform_component>,
				gse::read_v<gse::physics::motion_component>,
				gse::write_v<gse::physics::motor_component>,
				gse::write_v<gse::camera::follow_component>,
				gse::write_v<gs::controlled_joint_component>,
				gse::read_v<gs::balance::component>
			);

			const auto player_ids = players.owner_ids();
			for (std::size_t i = 0; i < players.size(); ++i) {
				auto& p = players[i];
				const auto owner_id = player_ids[i];
				auto [it, inserted] = d.bindings_by_owner.try_emplace(owner_id);
				if (!inserted) {
					continue;
				}
				auto& b = it->second;

				const auto w = gse::actions::add<"Player_Move_Forward">(ctx.channels, gse::key::w);
				const auto a = gse::actions::add<"Player_Move_Left">(ctx.channels, gse::key::a);
				const auto s_key = gse::actions::add<"Player_Move_Backward">(ctx.channels, gse::key::s);
				const auto d = gse::actions::add<"Player_Move_Right">(ctx.channels, gse::key::d);

				b.shift = gse::actions::add<"Player_Sprint">(ctx.channels, gse::key::left_shift);
				b.jump = gse::actions::add<"Player_Jump">(ctx.channels, gse::key::space);
				b.move_axis_id = gse::actions::bind_axis2(
					ctx.channels,
					gse::actions::pending_axis2_info{
						.left = a,
						.right = d,
						.back = s_key,
						.fwd = w,
						.scale = 1.f,
					},
					gse::trace_id<"Player_Move">()
				);

				b.jetpack_toggle = gse::actions::add<"Toggle_Jetpack">(ctx.channels, gse::key::j);
				b.jetpack_thrust = gse::actions::add<"Jetpack_Thrust">(ctx.channels, gse::key::space);
				b.jetpack_boost = gse::actions::add<"Jetpack_Boost">(ctx.channels, gse::key::left_shift);

				const auto jw = gse::actions::add<"Jetpack_Move_Forward">(ctx.channels, gse::key::w);
				const auto ja = gse::actions::add<"Jetpack_Move_Left">(ctx.channels, gse::key::a);
				const auto js = gse::actions::add<"Jetpack_Move_Backward">(ctx.channels, gse::key::s);
				const auto jd = gse::actions::add<"Jetpack_Move_Right">(ctx.channels, gse::key::d);

				b.jetpack_move_axis_id = gse::actions::bind_axis2(
					ctx.channels,
					gse::actions::pending_axis2_info{
						.left = ja,
						.right = jd,
						.back = js,
						.fwd = jw,
						.scale = 1.f,
					},
					gse::trace_id<"Jetpack_Move">()
				);

				const gse::quat initial_orientation = gse::normalize(
					gse::quat(gse::vec3f(0.f, 1.f, 0.f), p.yaw) * gse::quat(gse::vec3f(1.f, 0.f, 0.f), p.pitch)
				);

				ctx.add_component<gse::camera::follow_component>(
					owner_id,
					{
						.offset = gse::vec3<gse::length>(gse::meters(0.f)),
						.priority = 50,
						.blend_in_duration = gse::milliseconds(300),
						.active = true,
						.use_entity_position = false,
						.orientation = initial_orientation,
					}
				);
			}

			const auto& cs = gse::actions::system::current_state(as);
			const auto& in = gse::input::system::current_state(input_s);

			for (std::size_t i = 0; i < players.size(); ++i) {
				auto& p = players[i];
				const auto owner_id = player_ids[i];
				const auto& b = d.bindings_by_owner[owner_id];

				auto* motor = motors.find(p.pelvis_id);
				auto* pelvis_tc = transforms.find(p.pelvis_id);
				if (!motor || !pelvis_tc) {
					continue;
				}

				const gse::quat target_orientation = gse::quat(gse::vec3f(0.f, 1.f, 0.f), p.yaw);
				pelvis_tc->orientation =
					gse::normalize(gse::slerp(pelvis_tc->orientation, target_orientation, 0.15f));

				const bool is_active_view = cam_s.active_controller_entity == owner_id;
				if (is_active_view && !cam_s.ui_focus) {
					const auto delta = in.mouse_delta();
					p.yaw -= gse::degrees(delta.x() * p.mouse_sensitivity);
					p.pitch -= gse::degrees(delta.y() * p.mouse_sensitivity);
					p.pitch = std::clamp(p.pitch, gse::degrees(-89.f), gse::degrees(89.f));

					const auto scroll = in.scroll_delta();
					if (scroll.y() > 0.f) {
						d.camera_level = std::max(0, d.camera_level - 1);
					}
					else if (scroll.y() < 0.f) {
						d.camera_level =
							std::min(static_cast<int>(camera_distance_levels_m.size()) - 1, d.camera_level + 1);
					}
				}

				const gse::quat orientation = gse::normalize(
					gse::quat(gse::vec3f(0.f, 1.f, 0.f), p.yaw) * gse::quat(gse::vec3f(1.f, 0.f, 0.f), p.pitch)
				);

				const bool shift_held = gse::actions::held(b.shift, cs, as);
				const auto speed = shift_held ? p.sprint_speed : p.walk_speed;
				const auto v = cs.axis2_v(static_cast<std::uint16_t>(b.move_axis_id.number()));

				gse::vec3<gse::velocity> target_velocity{};
				if (const auto* bal = balances.find(p.pelvis_id)) {
					target_velocity = bal->correction_velocity;
				}

				constexpr float motor_smoothing = 0.15f;
				motor->velocity_drive_target =
					motor->velocity_drive_target + (target_velocity - motor->velocity_drive_target) * motor_smoothing;

				const float input_magnitude = std::sqrt(v.x() * v.x() + v.y() * v.y());
				const float input_intensity = std::clamp(input_magnitude, 0.f, 1.f);
				constexpr float intensity_smoothing = 0.1f;
				p.gait_intensity += (input_intensity - p.gait_intensity) * intensity_smoothing;
				const float walking_intensity = p.gait_intensity;

				constexpr float leg_length_m = 0.85f;
				constexpr float placement_gain = 1.0f / leg_length_m;
				constexpr float placement_clamp = 0.35f;
				constexpr float intensity_start_threshold = 0.05f;
				constexpr float intensity_stop_threshold = 0.02f;
				constexpr float knee_lift = 0.30f;

				const float walking_step_bias = walking_intensity * 0.1f;
				float step_bias = walking_step_bias;
				if (const auto* bal = balances.find(p.pelvis_id)) {
					step_bias += static_cast<float>(bal->stepping_bias_forward) * placement_gain;
				}
				step_bias = std::clamp(step_bias, -placement_clamp, placement_clamp);

				const auto step_dt = gse::system_clock::fixed_dt<gse::time>();
				const int steps = gse::system_clock::fixed_steps_this_frame();
				const auto frame_dt = step_dt * static_cast<float>(steps);

				constexpr float balance_step_threshold = 0.22f;
				const bool needs_balance_step = std::abs(step_bias) > balance_step_threshold;
				const bool wants_walk = walking_intensity > intensity_start_threshold || needs_balance_step;
				const auto begin_swing = [&](swing_leg leg, float hip_start) {
					p.current_swing = leg;
					p.gait = gait_state::walking;
					p.swing_elapsed = gse::seconds(0.f);
					p.swing_hip_start = hip_start;
					p.swing_hip_end = step_bias;
				};

				const auto swing_progress = [&]() -> float {
					return std::clamp(p.swing_elapsed / p.swing_duration, 0.f, 1.f);
				};

				if (p.gait == gait_state::idle) {
					if (wants_walk) {
						begin_swing(swing_leg::left, 0.f);
					}
				}
				else {
					p.swing_elapsed += frame_dt;
					const float swing_t = swing_progress();

					const auto swing_foot_id =
						p.current_swing == swing_leg::left ? p.foot_l_id : p.foot_r_id;
					const bool foot_planted = !gse::physics::system::is_airborne(phys_s, swing_foot_id);
					const bool swing_finished = swing_t >= 1.f || (swing_t > 0.5f && foot_planted);

					if (swing_finished) {
						const bool continue_walking =
							wants_walk || walking_intensity > intensity_stop_threshold;
						if (continue_walking) {
							const swing_leg next =
								p.current_swing == swing_leg::left ? swing_leg::right : swing_leg::left;
							begin_swing(next, 0.f);
						}
						else {
							p.gait = gait_state::idle;
							p.swing_elapsed = gse::seconds(0.f);
						}
					}
				}

				float hip_l_target = 0.f;
				float hip_r_target = 0.f;
				float knee_l_target = 0.f;
				float knee_r_target = 0.f;

				if (p.gait == gait_state::walking) {
					const float swing_t = swing_progress();
					const float swing_hip = std::lerp(p.swing_hip_start, p.swing_hip_end, swing_t);
					const float swing_knee = -knee_lift * 4.f * swing_t * (1.f - swing_t);

					if (p.current_swing == swing_leg::left) {
						hip_l_target = swing_hip;
						knee_l_target = swing_knee;
					}
					else {
						hip_r_target = swing_hip;
						knee_r_target = swing_knee;
					}
				}

				bool found_hip_l = false;
				bool found_knee_l = false;
				bool found_hip_r = false;
				bool found_knee_r = false;
				if (auto* cj = ctrls.find(p.hip_l_id)) {
					cj->target_angle = gse::radians(hip_l_target);
					found_hip_l = true;
				}
				if (auto* cj = ctrls.find(p.knee_l_id)) {
					cj->target_angle = gse::radians(knee_l_target);
					found_knee_l = true;
				}
				if (auto* cj = ctrls.find(p.hip_r_id)) {
					cj->target_angle = gse::radians(hip_r_target);
					found_hip_r = true;
				}
				if (auto* cj = ctrls.find(p.knee_r_id)) {
					cj->target_angle = gse::radians(knee_r_target);
					found_knee_r = true;
				}

				if (d.gait_log_timer.tick() && (walking_intensity > 0.01f || p.gait != gait_state::idle)) {
					const char* state_name = p.gait == gait_state::idle ? "idle" : "walk";
					const char* swing_name = p.current_swing == swing_leg::left ? "L" : "R";
					gse::log::println(
						"gait: state={} swing={} t={:.2f} intensity={:.2f} step_bias={:+.3f} "
						"hip_l={:+.3f}(f={}) knee_l={:+.3f}(f={}) hip_r={:+.3f}(f={}) knee_r={:+.3f}(f={})",
						state_name,
						swing_name,
						swing_progress(),
						walking_intensity,
						step_bias,
						hip_l_target,
						found_hip_l ? 1 : 0,
						knee_l_target,
						found_knee_l ? 1 : 0,
						hip_r_target,
						found_hip_r ? 1 : 0,
						knee_r_target,
						found_knee_r ? 1 : 0
					);
				}

				if (gse::actions::pressed(b.jetpack_toggle, cs, as)) {
					p.jetpack_enabled = !p.jetpack_enabled;
				}

				if (auto* cam_follow = follows.find(owner_id)) {
					const auto level = static_cast<std::size_t>(
						std::clamp(d.camera_level, 0, static_cast<int>(camera_distance_levels_m.size()) - 1)
					);
					const gse::length distance = gse::meters(camera_distance_levels_m[level]);
					const gse::vec3f forward = gse::rotate_vector(orientation, gse::vec3f(0.f, 0.f, -1.f));
					cam_follow->position = pelvis_tc->position - forward * distance;
					cam_follow->orientation = orientation;
				}
			}

			std::erase_if(d.bindings_by_owner, [&players](const auto& entry) {
				return !players.find(entry.first);
			});
		}

		co_await ctx.next_tick();
	}
}

export module gs:player;

import std;
import gse;

import :balance;
import :controlled_joint;

export namespace gs::player {
	enum class swing_leg : std::uint8_t {
		left,
		right
	};
	enum class gait_state : std::uint8_t {
		idle,
		walking
	};

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
		gse::time swing_duration = gse::seconds(0.36f);
		gse::angle swing_hip_start = gse::radians(0.f);
		gse::angle swing_hip_end = gse::radians(0.f);
		gse::angle stance_hip_start = gse::radians(0.f);
		gse::angle stance_hip_end = gse::radians(0.f);
		gse::angle stance_knee_start = gse::radians(0.f);
		float gait_intensity = 0.f;
		float forward_intent = 0.f;
		bool fallen = false;
		gse::velocity walk_speed = gse::meters_per_second(0.25f);
		gse::velocity sprint_speed = gse::meters_per_second(0.7f);
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
		struct [[= gse::settings::category<"Player">{}]] data {
			std::unordered_map<gse::id, bindings> bindings_by_owner;

			[[
				= gse::settings::describe<"Camera follow distance level (0=first person, 1-3=third person).">{},
				= gse::settings::range<0, 3>{}
			]]
			int camera_level = 0;

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

				auto* pelvis_tc = transforms.find(p.pelvis_id);
				if (!pelvis_tc) {
					continue;
				}
				auto* motor = motors.find(p.pelvis_id);

				constexpr gse::position fall_threshold_y = gse::meters(0.25f);
				constexpr gse::velocity runaway_speed_threshold = gse::meters_per_second(3.5f);
				constexpr gse::displacement runaway_lean_threshold = gse::meters(0.9f);

				const auto pelvis_y = pelvis_tc->position.y();
				const auto* pelvis_mc = motions.find(p.pelvis_id);
				gse::velocity horizontal_speed = {};
				if (pelvis_mc) {
					const auto horizontal_v = gse::vec3<gse::velocity>(
						pelvis_mc->current_velocity.x(),
						gse::meters_per_second(0.f),
						pelvis_mc->current_velocity.z()
					);
					horizontal_speed = gse::magnitude(horizontal_v);
				}
				const bool runaway_speed = horizontal_speed > runaway_speed_threshold;
				const auto* bal_snapshot = balances.find(p.pelvis_id);
				const bool runaway_lean = bal_snapshot &&
					(bal_snapshot->stepping_bias_forward > runaway_lean_threshold ||
					 bal_snapshot->stepping_bias_forward < -runaway_lean_threshold);
				if (!p.fallen && (pelvis_y < fall_threshold_y || runaway_speed || runaway_lean)) {
					p.fallen = true;
					gse::log::println(
						"player: ENTER FALLEN  pelvis_y={:.2f}m  speed={:.2f}m/s  step_bias_fwd={:+.3f}m  "
						"trigger={}{}{}",
						static_cast<float>(pelvis_y),
						static_cast<float>(horizontal_speed),
						bal_snapshot ? static_cast<float>(bal_snapshot->stepping_bias_forward) : 0.f,
						pelvis_y < fall_threshold_y ? "y " : "",
						runaway_speed ? "speed " : "",
						runaway_lean ? "lean" : ""
					);
				}

				const bool controllers_active = !p.fallen;
				for (std::size_t ci = 0; ci < ctrls.size(); ++ci) {
					ctrls[ci].enabled = controllers_active;
				}

				const bool foot_l_grounded = !gse::physics::system::is_airborne(phys_s, p.foot_l_id);
				const bool foot_r_grounded = !gse::physics::system::is_airborne(phys_s, p.foot_r_id);
				const bool any_foot_grounded = foot_l_grounded || foot_r_grounded;
				const bool both_feet_grounded = foot_l_grounded && foot_r_grounded;

				if (!p.fallen && any_foot_grounded) {
					const gse::quat target_orientation = gse::quat(gse::vec3f(0.f, 1.f, 0.f), p.yaw);
					const float turn_rate = both_feet_grounded ? 0.10f : 0.04f;
					pelvis_tc->orientation =
						gse::normalize(gse::slerp(pelvis_tc->orientation, target_orientation, turn_rate));
				}

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

				const bool sprint_held = gse::actions::held(b.shift, cs, as);
				const auto v = cs.axis2_v(static_cast<std::uint16_t>(b.move_axis_id.number()));

				const float input_magnitude = std::sqrt(v.x() * v.x() + v.y() * v.y());
				const float input_intensity = std::clamp(input_magnitude, 0.f, 1.f);
				constexpr float intensity_smoothing = 0.1f;
				p.gait_intensity += (input_intensity - p.gait_intensity) * intensity_smoothing;
				const float walking_intensity = p.gait_intensity;
				const float raw_forward_intent = std::clamp(-v.y(), -1.f, 1.f);
				p.forward_intent += (raw_forward_intent - p.forward_intent) * intensity_smoothing;
				const float forward_intent = p.forward_intent;
				const float strafe_intent = std::clamp(v.x(), -1.f, 1.f);

				if (motor != nullptr) {
					if (p.fallen) {
						motor->horizontal_only = true;
						motor->velocity_drive_target = {};
					}
					else {
						const auto walk = sprint_held ? p.sprint_speed : p.walk_speed;
						float lean_brake = 1.f;
						if (const auto* bal = balances.find(p.pelvis_id)) {
							const float forward_lean_m =
								std::max(static_cast<float>(bal->stepping_bias_forward) * forward_intent, 0.f);
							lean_brake = std::clamp(1.f - forward_lean_m / 0.25f, 0.f, 1.f);
						}
						const gse::vec3f facing_forward =
							gse::rotate_vector(pelvis_tc->orientation, gse::vec3f(0.f, 0.f, -1.f));
						const gse::vec3f facing_right =
							gse::rotate_vector(pelvis_tc->orientation, gse::vec3f(1.f, 0.f, 0.f));
						auto input_velocity = facing_forward * (walk * forward_intent * lean_brake) +
							facing_right * (walk * strafe_intent);
						input_velocity = gse::vec3<gse::velocity>(
							input_velocity.x(),
							gse::meters_per_second(0.f),
							input_velocity.z()
						);

						gse::vec3<gse::velocity> balance_velocity{};
						if (const auto* bal = balances.find(p.pelvis_id)) {
							const float balance_gain = 1.f - std::clamp(walking_intensity, 0.f, 1.f);
							balance_velocity = bal->correction_velocity * balance_gain;
						}

						gse::vec3<gse::velocity> gait_velocity{};
						bool vertical_motor_active = false;
						if (p.gait == gait_state::walking) {
							const float swing_t = std::clamp(p.swing_elapsed / p.swing_duration, 0.f, 1.f);
							const float lift_curve = 4.f * swing_t * (1.f - swing_t);
							const float weight_shift_curve = std::clamp(1.f - swing_t / 0.55f, 0.f, 1.f);
							const auto stance_foot_id = p.current_swing == swing_leg::left ? p.foot_r_id : p.foot_l_id;
							if (const auto* stance_tc = transforms.find(stance_foot_id)) {
								const auto pelvis_to_stance = stance_tc->position - pelvis_tc->position;
								const auto body_right =
									gse::rotate_vector(pelvis_tc->orientation, gse::vec3f(1.f, 0.f, 0.f));
								auto lateral_speed = gse::dot(body_right, pelvis_to_stance) / gse::seconds(0.18f);
								constexpr gse::velocity max_lateral_speed = gse::meters_per_second(0.20f);
								lateral_speed = std::clamp(lateral_speed, -max_lateral_speed, max_lateral_speed);
								gait_velocity += body_right * (lateral_speed * walking_intensity * weight_shift_curve);
							}

							constexpr gse::velocity max_lift_speed = gse::meters_per_second(0.42f);
							const auto lift_speed = max_lift_speed * lift_curve * walking_intensity;
							gait_velocity += gse::vec3<gse::velocity>(
								gse::meters_per_second(0.f),
								lift_speed,
								gse::meters_per_second(0.f)
							);
							vertical_motor_active = lift_curve > 0.05f && walking_intensity > 0.05f;
						}

						const auto target_velocity = input_velocity + balance_velocity + gait_velocity;
						motor->horizontal_only = !vertical_motor_active;

						constexpr float motor_smoothing = 0.35f;
						motor->velocity_drive_target = motor->velocity_drive_target +
							(target_velocity - motor->velocity_drive_target) * motor_smoothing;
					}
				}

				gse::angle hip_l_target = gse::radians(0.f);
				gse::angle hip_r_target = gse::radians(0.f);
				gse::angle knee_l_target = gse::radians(0.f);
				gse::angle knee_r_target = gse::radians(0.f);
				gse::angle stance_hip_target = gse::radians(0.f);
				bool found_hip_l = false;
				bool found_knee_l = false;
				bool found_hip_r = false;
				bool found_knee_r = false;
				gse::angle step_bias = gse::radians(0.f);

				if (p.fallen) {
					p.gait = gait_state::idle;
					p.swing_elapsed = gse::seconds(0.f);
				}
				else {
					constexpr gse::length leg_length = gse::meters(0.85f);
					constexpr gse::angle placement_clamp = gse::radians(0.6f);
					constexpr float intensity_start_threshold = 0.05f;
					constexpr float intensity_stop_threshold = 0.02f;
					constexpr gse::angle knee_lift = gse::radians(0.55f);
					constexpr gse::angle balance_step_threshold = gse::radians(0.10f);
					constexpr gse::angle max_swing_hip = gse::radians(0.45f);
					constexpr gse::angle max_stance_extension = gse::radians(0.15f);
					constexpr gse::impulse swing_lift_impulse = gse::newton_seconds(6.f);
					constexpr gse::impulse swing_unstick_up_impulse = gse::newton_seconds(0.45f);
					constexpr gse::impulse swing_unstick_step_impulse = gse::newton_seconds(0.15f);
					const float sprint_scale = sprint_held ? 1.5f : 1.0f;
					const gse::angle min_hip_start = -max_stance_extension * sprint_scale;
					const gse::angle max_hip_start = max_swing_hip * sprint_scale;
					const gse::angle min_knee_start = -knee_lift;
					p.swing_duration = gse::seconds(sprint_held ? 0.26f : 0.36f);

					const gse::angle walking_step_bias = max_swing_hip * forward_intent * sprint_scale;
					step_bias = walking_step_bias;
					if (const auto* bal = balances.find(p.pelvis_id)) {
						const float balance_step_gain = 1.f - std::clamp(walking_intensity, 0.f, 1.f);
						step_bias += gse::radians(bal->stepping_bias_forward / leg_length) * balance_step_gain;
					}
					step_bias = std::clamp(step_bias, -placement_clamp, placement_clamp);

					stance_hip_target = -max_stance_extension * forward_intent * sprint_scale;

					const auto step_dt = gse::system_clock::fixed_dt<gse::time>();
					const int steps = gse::system_clock::fixed_steps_this_frame();
					const auto frame_dt = step_dt * static_cast<float>(steps);

					const bool needs_balance_step =
						step_bias > balance_step_threshold || step_bias < -balance_step_threshold;
					const bool wants_walk = walking_intensity > intensity_start_threshold || needs_balance_step;
					const auto current_prev = [&](gse::id joint_id) -> gse::angle {
						if (const auto* cj = ctrls.find(joint_id)) {
							return cj->prev_angle;
						}
						return gse::radians(0.f);
					};
					const auto begin_swing = [&](swing_leg leg, const char* reason) {
						p.current_swing = leg;
						p.gait = gait_state::walking;
						p.swing_elapsed = gse::seconds(0.f);
						const auto swing_hip_id = leg == swing_leg::left ? p.hip_l_id : p.hip_r_id;
						const auto stance_hip_id = leg == swing_leg::left ? p.hip_r_id : p.hip_l_id;
						const auto stance_knee_id = leg == swing_leg::left ? p.knee_r_id : p.knee_l_id;
						const auto swing_foot_id = leg == swing_leg::left ? p.foot_l_id : p.foot_r_id;
						p.swing_hip_start = std::clamp(current_prev(swing_hip_id), min_hip_start, max_hip_start);
						p.swing_hip_end = step_bias;
						p.stance_hip_start = std::clamp(current_prev(stance_hip_id), min_hip_start, max_hip_start);
						p.stance_hip_end = stance_hip_target;
						p.stance_knee_start =
							std::clamp(current_prev(stance_knee_id), min_knee_start, gse::radians(0.f));
						ctx.channels.push<gse::physics::impulse_request>({
							.target = swing_foot_id,
							.impulse = gse::vec3<gse::impulse>(
								gse::newton_seconds(0.f),
								swing_lift_impulse,
								gse::newton_seconds(0.f)
							),
						});
						const auto* foot_tc = transforms.find(swing_foot_id);
						gse::log::println(
							"gait: SWING START {} {}  swing_hip {:+.3f}->{:+.3f}  stance_hip {:+.3f}->{:+.3f}  "
							"pelvis_xz=({:+.2f},{:+.2f}) vel_xz=({:+.2f},{:+.2f}) intensity={:.2f} fwd={:+.2f} "
							"foot_pos_xz=({:+.2f},{:+.2f})",
							reason,
							leg == swing_leg::left ? "L" : "R",
							static_cast<float>(p.swing_hip_start),
							static_cast<float>(p.swing_hip_end),
							static_cast<float>(p.stance_hip_start),
							static_cast<float>(p.stance_hip_end),
							static_cast<float>(pelvis_tc->position.x()),
							static_cast<float>(pelvis_tc->position.z()),
							pelvis_mc ? static_cast<float>(pelvis_mc->current_velocity.x()) : 0.f,
							pelvis_mc ? static_cast<float>(pelvis_mc->current_velocity.z()) : 0.f,
							walking_intensity,
							forward_intent,
							foot_tc ? static_cast<float>(foot_tc->position.x()) : 0.f,
							foot_tc ? static_cast<float>(foot_tc->position.z()) : 0.f
						);
					};

					const auto swing_progress = [&]() -> float {
						return std::clamp(p.swing_elapsed / p.swing_duration, 0.f, 1.f);
					};

					if (p.gait == gait_state::idle) {
						if (wants_walk) {
							begin_swing(
								swing_leg::left,
								walking_intensity > intensity_start_threshold ? "INPUT" : "BALANCE"
							);
						}
					}
					else {
						p.swing_elapsed += frame_dt;
						const float swing_t = swing_progress();

						const auto swing_foot_id = p.current_swing == swing_leg::left ? p.foot_l_id : p.foot_r_id;
						const bool foot_planted = !gse::physics::system::is_airborne(phys_s, swing_foot_id);
						if (swing_t < 0.45f && foot_planted) {
							const auto facing_forward =
								gse::rotate_vector(pelvis_tc->orientation, gse::vec3f(0.f, 0.f, -1.f));
							const float step_direction = std::clamp(p.swing_hip_end / max_swing_hip, -1.f, 1.f);
							ctx.channels.push<gse::physics::impulse_request>({
								.target = swing_foot_id,
								.impulse = facing_forward * (swing_unstick_step_impulse * step_direction) +
									gse::vec3<gse::impulse>(
										gse::newton_seconds(0.f),
										swing_unstick_up_impulse,
										gse::newton_seconds(0.f)
									),
							});
						}
						const bool early_plant = swing_t > 0.5f && foot_planted;
						const bool swing_finished = swing_t >= 1.f || early_plant;

						if (swing_finished) {
							const auto* foot_tc = transforms.find(swing_foot_id);
							gse::log::println(
								"gait: SWING END   {} {}  t={:.2f}  pelvis_xz=({:+.2f},{:+.2f})  "
								"foot_pos_xz=({:+.2f},{:+.2f})  intended_hip={:+.3f}rad  achieved_hip={:+.3f}rad",
								early_plant ? "early-plant" : "timeout",
								p.current_swing == swing_leg::left ? "L" : "R",
								swing_t,
								static_cast<float>(pelvis_tc->position.x()),
								static_cast<float>(pelvis_tc->position.z()),
								foot_tc ? static_cast<float>(foot_tc->position.x()) : 0.f,
								foot_tc ? static_cast<float>(foot_tc->position.z()) : 0.f,
								static_cast<float>(p.swing_hip_end),
								static_cast<float>(
									current_prev(p.current_swing == swing_leg::left ? p.hip_l_id : p.hip_r_id)
								)
							);

							const bool continue_walking = wants_walk || walking_intensity > intensity_stop_threshold;
							if (continue_walking) {
								const swing_leg next =
									p.current_swing == swing_leg::left ? swing_leg::right : swing_leg::left;
								begin_swing(next, walking_intensity > intensity_start_threshold ? "INPUT" : "BALANCE");
							}
							else {
								gse::log::println(
									"gait: IDLE  pelvis_xz=({:+.2f},{:+.2f})",
									static_cast<float>(pelvis_tc->position.x()),
									static_cast<float>(pelvis_tc->position.z())
								);
								p.gait = gait_state::idle;
								p.swing_elapsed = gse::seconds(0.f);
							}
						}
					}

					const bool left_is_swing = p.gait == gait_state::walking && p.current_swing == swing_leg::left;

					if (p.gait == gait_state::walking) {
						const float swing_t = swing_progress();
						const float lift_curve = 4.f * swing_t * (1.f - swing_t);
						constexpr gse::angle stance_absorb = gse::radians(-0.20f);
						const gse::angle swing_hip =
							p.swing_hip_start + (p.swing_hip_end - p.swing_hip_start) * swing_t;
						const gse::angle swing_knee = -knee_lift * lift_curve;
						const gse::angle stance_hip =
							p.stance_hip_start + (p.stance_hip_end - p.stance_hip_start) * swing_t;
						const gse::angle stance_knee =
							p.stance_knee_start * (1.f - swing_t) + stance_absorb * lift_curve;

						if (left_is_swing) {
							hip_l_target = swing_hip;
							knee_l_target = swing_knee;
							hip_r_target = stance_hip;
							knee_r_target = stance_knee;
						}
						else {
							hip_r_target = swing_hip;
							knee_r_target = swing_knee;
							hip_l_target = stance_hip;
							knee_l_target = stance_knee;
						}
					}

					if (auto* cj = ctrls.find(p.hip_l_id)) {
						cj->target_angle = p.gait == gait_state::walking ? hip_l_target : gse::radians(0.f);
						found_hip_l = true;
					}
					if (auto* cj = ctrls.find(p.knee_l_id)) {
						cj->target_angle = knee_l_target;
						found_knee_l = true;
					}
					if (auto* cj = ctrls.find(p.hip_r_id)) {
						cj->target_angle = p.gait == gait_state::walking ? hip_r_target : gse::radians(0.f);
						found_hip_r = true;
					}
					if (auto* cj = ctrls.find(p.knee_r_id)) {
						cj->target_angle = knee_r_target;
						found_knee_r = true;
					}
				}

				if (d.gait_log_timer.tick() && (p.fallen || walking_intensity > 0.01f || p.gait != gait_state::idle)) {
					const char* state_name = p.fallen ? "fallen" : (p.gait == gait_state::idle ? "idle" : "walk");
					const char* swing_name = p.current_swing == swing_leg::left ? "L" : "R";
					const float swing_t = std::clamp(p.swing_elapsed / p.swing_duration, 0.f, 1.f);
					gse::log::println(
						"gait: state={} swing={} t={:.2f} intensity={:.2f} fwd={:+.2f} step_bias={:+.3f}rad "
						"stance_hip={:+.3f}rad "
						"hip_l={:+.3f}rad(f={}) knee_l={:+.3f}rad(f={}) hip_r={:+.3f}rad(f={}) knee_r={:+.3f}rad(f={})",
						state_name,
						swing_name,
						swing_t,
						walking_intensity,
						forward_intent,
						static_cast<float>(step_bias),
						static_cast<float>(stance_hip_target),
						static_cast<float>(hip_l_target),
						found_hip_l ? 1 : 0,
						static_cast<float>(knee_l_target),
						found_knee_l ? 1 : 0,
						static_cast<float>(hip_r_target),
						found_hip_r ? 1 : 0,
						static_cast<float>(knee_r_target),
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

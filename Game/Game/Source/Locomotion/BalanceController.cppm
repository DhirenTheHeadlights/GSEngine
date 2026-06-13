export module gs:balance_controller;

import std;
import gse;

import :locomotion_types;

export namespace gs::locomotion {
	struct balance_controller {
		struct [[= gse::settings::category<"Balance">{}]] data {
			[[
				= gse::settings::describe<"Forward pelvis assist speed at full input.">{}
			]]
			gse::velocity forward_speed = gse::meters_per_second(0.22f);

			[[
				= gse::settings::describe<"Forward pelvis assist speed at full input while sprinting.">{}
			]]
			gse::velocity sprint_forward_speed = gse::meters_per_second(0.60f);

			[[
				= gse::settings::describe<"Maximum forward pelvis assist speed.">{}
			]]
			gse::velocity max_forward_speed = gse::meters_per_second(0.85f);

			[[
				= gse::settings::describe<"Speed above the forward assist target the legs may add before the motor governs.">{}
			]]
			gse::velocity ride_headroom = gse::meters_per_second(0.04f);

			[[
				= gse::settings::describe<"Maximum lateral pelvis assist speed.">{}
			]]
			gse::velocity max_lateral_speed = gse::meters_per_second(0.35f);

			[[
				= gse::settings::describe<"Gain from support-center error to pelvis assist.">{}
			]]
			gse::inverse_length support_gain = gse::per_meter(0.85f);

			[[
				= gse::settings::describe<"Gain from capture error to pelvis assist.">{}
			]]
			gse::inverse_length capture_gain = gse::per_meter(1.80f);

			[[
				= gse::settings::describe<"Forward capture below this magnitude is treated as normal gait lean and not resisted.">{}
			]]
			gse::displacement forward_capture_deadzone = gse::meters(0.30f);

			[[
				= gse::settings::describe<"Backward capture deadzone when settled/standing (a slow backward lean is never normal standing, so catch it; blends to the forward deadzone while moving so normal gait backward-capture is untouched).">{}
			]]
			gse::displacement settled_backward_capture_deadzone = gse::meters(0.10f);

			[[
				= gse::settings::describe<"Horizontal speed above which the backward capture deadzone is the normal (forward) value; below it blends toward the settled value.">{}
			]]
			gse::velocity settled_speed_threshold = gse::meters_per_second(0.40f);

			[[
				= gse::settings::describe<"Maximum force used by the pelvis balance motor.">{}
			]]
			gse::force max_force = gse::newtons(400.f);

			gse::interval_timer<float> log_timer{ gse::seconds(0.3f) };
		};

		static auto run(
			data& d,
			gse::read<skeleton_refs> refs,
			gse::read<intent> intents,
			gse::read<state> states,
			gse::read<gait> gaits,
			gse::write<gse::physics::motor_component> motors
		) -> gse::async::task<>;
	};
}

namespace gs::locomotion {
	auto horizontal_axis(
		gse::vec3f axis
	) -> gse::vec3f;

	auto deadzoned(
		gse::displacement value,
		gse::displacement deadzone
	) -> gse::displacement;
	
	auto balance_velocity_target(
		const state& s,
		const intent& it,
		const balance_controller::data& d
	) -> gse::vec3<gse::velocity>;
}

auto gs::locomotion::deadzoned(const gse::displacement value, const gse::displacement deadzone) -> gse::displacement {
	if (value > deadzone) {
		return value - deadzone;
	}
	if (value < -deadzone) {
		return value + deadzone;
	}
	return gse::meters(0.f);
}

auto gs::locomotion::horizontal_axis(gse::vec3f axis) -> gse::vec3f {
	axis.y() = 0.f;
	const float len = gse::magnitude(axis);
	if (len <= 0.0001f) {
		return {};
	}
	return axis / len;
}

auto gs::locomotion::balance_velocity_target(const state& s, const intent& it, const balance_controller::data& d) -> gse::vec3<gse::velocity> {
	const auto forward = horizontal_axis(s.pelvis_forward);
	const auto right = horizontal_axis(s.pelvis_right);
	const auto support_error = s.support_center - s.pelvis_position;
	const auto support_forward = gse::dot(forward, support_error);
	const auto support_right = gse::dot(right, support_error);

	const auto settled = std::clamp((d.settled_speed_threshold - s.horizontal_speed) / d.settled_speed_threshold, 0.f, 1.f);
	const auto backward_deadzone = d.forward_capture_deadzone + (d.settled_backward_capture_deadzone - d.forward_capture_deadzone) * settled;
	const auto capture_forward_resisted = s.capture_forward >= gse::meters(0.f)
		? deadzoned(s.capture_forward, d.forward_capture_deadzone)
		: deadzoned(s.capture_forward, backward_deadzone);
	const float push_fade = std::clamp((gse::meters(0.35f) - s.capture_forward) / gse::meters(0.25f), 0.f, 1.f);
	const float launch_ramp = std::clamp(0.35f + s.horizontal_speed / gse::meters_per_second(0.25f), 0.f, 1.f);
	const float sprint_blend = std::clamp(it.sprint_blend, 0.f, 1.f);
	const float drive_intent = std::clamp(it.forward * it.intensity, -1.f, 1.f);
	const auto push_speed = d.forward_speed + (d.sprint_forward_speed - d.forward_speed) * sprint_blend;
	auto forward_drive = push_speed * (push_fade * launch_ramp) * drive_intent;
	if (drive_intent > 0.f) {
		const auto measured_forward = gse::dot(forward, s.velocity_world);
		const auto ride_ceiling = push_speed + d.ride_headroom * (1.f - sprint_blend);
		forward_drive = std::max(forward_drive, std::min(measured_forward, ride_ceiling));
	}
	auto forward_velocity = forward_drive + gse::meters_per_second(1.f) * (d.support_gain * support_forward - d.capture_gain * capture_forward_resisted);
	forward_velocity = std::clamp(forward_velocity, -d.max_forward_speed, d.max_forward_speed);

	auto lateral_velocity = gse::meters_per_second(1.f) * (d.support_gain * support_right - d.capture_gain * s.capture_right);
	lateral_velocity = std::clamp(lateral_velocity, -d.max_lateral_speed, d.max_lateral_speed);

	return forward * forward_velocity + right * lateral_velocity;
}

auto gs::locomotion::balance_controller::run(data& d, gse::read<skeleton_refs> refs, gse::read<intent> intents, gse::read<state> states, gse::read<gait> gaits, gse::write<gse::physics::motor_component> motors) -> gse::async::task<> {
	const bool log_now = d.log_timer.tick();
	const auto owner_ids = refs.owner_ids();
	for (std::size_t i = 0; i < refs.size(); ++i) {
		const auto owner = owner_ids[i];
		const auto& r = refs[i];
		auto* motor = motors.find(r.pelvis_id);
		if (!motor) {
			continue;
		}

		const auto* it = intents.find(owner);
		const auto* s = states.find(owner);
		const auto* g = gaits.find(owner);

		motor->horizontal_only = true;
		motor->requires_ground_contact = false;

		if (!it || !s || !g || !s->valid || g->fallen) {
			motor->velocity_drive_target = {};
			motor->max_force = gse::newtons(0.f);
			continue;
		}

		motor->max_force = d.max_force;
		motor->velocity_drive_target = balance_velocity_target(*s, *it, d);

		if (log_now) {
			gse::log::println(
				"balance_controller: owner={} target_v=(right={:+.2f},fwd={:+.2f}) capture_right={:+.3f} "
				"support_center=({:+.2f},{:+.2f})",
				owner.number(),
				gse::dot(
					horizontal_axis(s->pelvis_right),
					motor->velocity_drive_target
				),
				gse::dot(
					horizontal_axis(s->pelvis_forward),
					motor->velocity_drive_target
				),
				s->capture_right,
				s->support_center.x(),
				s->support_center.z()
			);
		}
	}

	return {};
}

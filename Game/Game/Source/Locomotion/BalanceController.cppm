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
			gse::velocity forward_speed = gse::meters_per_second(0.35f);

			[[
				= gse::settings::describe<"Maximum forward pelvis assist speed.">{}
			]]
			gse::velocity max_forward_speed = gse::meters_per_second(0.60f);

			[[
				= gse::settings::describe<"Maximum lateral pelvis assist speed.">{}
			]]
			gse::velocity max_lateral_speed = gse::meters_per_second(0.80f);

			[[
				= gse::settings::describe<"Gain from support-center error to pelvis assist.">{}
			]]
			gse::inverse_length support_gain = gse::per_meter(1.25f);

			[[
				= gse::settings::describe<"Gain from capture error to pelvis assist.">{}
			]]
			gse::inverse_length capture_gain = gse::per_meter(2.00f);

			[[
				= gse::settings::describe<"Maximum force used by the pelvis balance motor.">{}
			]]
			gse::force max_force = gse::newtons(180.f);

			gse::interval_timer<float> log_timer{ gse::seconds(0.3f) };
		};

		static auto run(
			gse::run_context& ctx,
			data& d
		) -> gse::async::task<>;
	};
}

namespace gs::locomotion {
	auto horizontal_axis(
		gse::vec3f axis
	) -> gse::vec3f;
	auto balance_velocity_target(
		const state& s,
		const intent& it,
		const balance_controller::data& d
	) -> gse::vec3<gse::velocity>;
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

	auto forward_velocity = d.forward_speed * std::clamp(it.forward * it.intensity, -1.f, 1.f) +
		gse::meters_per_second(1.f) * (d.support_gain * support_forward - d.capture_gain * s.capture_forward);
	forward_velocity = std::clamp(forward_velocity, -d.max_forward_speed, d.max_forward_speed);

	auto lateral_velocity =
		gse::meters_per_second(1.f) * (d.support_gain * support_right - d.capture_gain * s.capture_right);
	lateral_velocity = std::clamp(lateral_velocity, -d.max_lateral_speed, d.max_lateral_speed);

	return forward * forward_velocity + right * lateral_velocity;
}

auto gs::locomotion::balance_controller::run(gse::run_context& ctx, data& d) -> gse::async::task<> {
	while (true) {
		{
			auto [refs, intents, states, gaits, motors] = co_await ctx.acquire_with(
				gse::read_v<skeleton_refs>,
				gse::read_v<intent>,
				gse::read_v<state>,
				gse::read_v<gait>,
				gse::write_v<gse::physics::motor_component>
			);

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
						gse::dot(horizontal_axis(s->pelvis_right), motor->velocity_drive_target),
						gse::dot(horizontal_axis(s->pelvis_forward), motor->velocity_drive_target),
						s->capture_right,
						s->support_center.x(),
						s->support_center.z()
					);
				}
			}
		}

		co_await ctx.next_tick();
	}
}

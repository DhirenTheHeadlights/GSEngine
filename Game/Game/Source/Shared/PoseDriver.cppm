export module gs:pose_driver;

import std;
import gse;

import :controlled_joint;

export namespace gs::pose_driver {
	struct [[= gse::system_state<"PoseDriver">{}]] data {
		gse::interval_timer<float> log_timer{ gse::seconds(0.5f) };
	};

	[[= gse::system_run<>{}]]
	auto run(
		data& state,
		gse::read<gse::physics::joint_spec> specs,
		gse::read<gse::physics::transform_component> transforms,
		gse::write<controlled_joint_component> ctrls,
		gse::write<gse::physics::muscle_component> muscles
	) -> gse::async::task<>;
}

auto gs::pose_driver::run(data& state, gse::read<gse::physics::joint_spec> specs, gse::read<gse::physics::transform_component> transforms, gse::write<controlled_joint_component> ctrls, gse::write<gse::physics::muscle_component> muscles) -> gse::async::task<> {
	const auto dt_seconds = gse::system_clock::fixed_dt<gse::time>().as<gse::seconds>();
	const auto inv_dt = dt_seconds > 0.f ? 1.f / dt_seconds : 0.f;
	const bool log_now = state.log_timer.tick();

	const auto ctrl_owners = ctrls.owner_ids();
	for (std::size_t i = 0; i < ctrls.size(); ++i) {
		auto& cj = ctrls[i];
		const auto eid = ctrl_owners[i];

		if (!cj.enabled) {
			if (auto* m = muscles.find(cj.flexor_muscle)) {
				m->activation = 0.f;
			}
			if (auto* m = muscles.find(cj.extensor_muscle)) {
				m->activation = 0.f;
			}
			continue;
		}

		const auto* spec = specs.find(eid);
		if (!spec) {
			continue;
		}

		const auto* trans_a = transforms.find(spec->entity_a);
		const auto* trans_b = transforms.find(spec->entity_b);
		if (!trans_a || !trans_b) {
			continue;
		}

		const auto& q_a = trans_a->orientation;
		const auto& q_b = trans_b->orientation;
		const auto q_rel = q_b * gse::conjugate(q_a);

		constexpr std::uint32_t rest_settle_required_ticks = 30;
		if (cj.rest_settle_ticks < rest_settle_required_ticks) {
			cj.rest_orientation = q_rel;
			cj.prev_angle = gse::radians(0.f);
			++cj.rest_settle_ticks;
			if (auto* m = muscles.find(cj.flexor_muscle)) {
				m->activation = 0.f;
			}
			if (auto* m = muscles.find(cj.extensor_muscle)) {
				m->activation = 0.f;
			}
			continue;
		}

		const auto q_diff = q_rel * gse::conjugate(cj.rest_orientation);
		const auto theta = gse::to_axis_angle(q_diff);
		const auto axis_world = gse::rotate_vector(q_a, cj.hinge_axis);
		const auto current_angle = gse::dot(axis_world, theta);

		const auto error = cj.target_angle - current_angle;
		const auto error_radians = static_cast<float>(error);
		const auto angle_velocity_radps = static_cast<float>(current_angle - cj.prev_angle) * inv_dt;
		cj.prev_angle = current_angle;

		const float pd = cj.gain * error_radians - cj.damping * angle_velocity_radps;
		const float activation_limit = std::clamp(cj.max_activation, 0.f, 1.f);
		const float activation_signed = std::clamp(pd, -activation_limit, activation_limit);

		const float flexor_act = std::max(activation_signed, 0.f);
		const float extensor_act = std::max(-activation_signed, 0.f);

		if (auto* m = muscles.find(cj.flexor_muscle)) {
			m->activation = flexor_act;
		}
		if (auto* m = muscles.find(cj.extensor_muscle)) {
			m->activation = extensor_act;
		}

		if (log_now) {
			gse::log::println(
				"pose_driver joint={} curr={:+.3f}rad targ={:+.3f}rad err={:+.3f}rad vel={:+.3f}rad/s "
				"flex={:.2f} ext={:.2f}",
				eid.number(),
				static_cast<float>(current_angle),
				static_cast<float>(cj.target_angle),
				error_radians,
				angle_velocity_radps,
				flexor_act,
				extensor_act
			);
		}
	}

	return {};
}

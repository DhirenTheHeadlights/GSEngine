export module gs:state_estimator;

import std;
import gse;

import :locomotion_types;

export namespace gs::locomotion::state_estimator {
	struct [[= gse::system_state<"State Estimator">{}]] data {
		[[
			= gse::settings::
				describe<"Y height of the ground plane (foot AABB min Y below this counts as grounded).">{}
		]]
		gse::position ground_y = gse::meters(0.f);

		[[
			= gse::settings::describe<"Vertical tolerance for foot-ground contact.">{}
		]]
		gse::displacement ground_tolerance = gse::meters(0.02f);

		gse::interval_timer<float> log_timer{ gse::seconds(0.5f) };
	};

	[[= gse::system_run<>{}]]
	auto run(
		data& d,
		gse::read<skeleton_refs> refs,
		gse::read<gse::physics::transform_component> transforms,
		gse::read<gse::physics::motion_component> motions,
		gse::read<gse::physics::collision_component> collisions,
		gse::read<gait> gaits,
		gse::write<state> states
	) -> gse::async::task<>;
}

namespace gs::locomotion {
	auto excess_past_edge(
		gse::position value,
		gse::position lo,
		gse::position hi
	) -> gse::displacement;

	auto reset_state(
		state& s
	) -> void;

	auto hinge_angle_about_x(
		const gse::quat& parent,
		const gse::quat& child
	) -> gse::angle;
}

auto gs::locomotion::hinge_angle_about_x(const gse::quat& parent, const gse::quat& child) -> gse::angle {
	const auto rel = child * gse::conjugate(parent);
	const auto theta = gse::to_axis_angle(rel);
	const auto axis_world = gse::rotate_vector(parent, gse::vec3f(1.f, 0.f, 0.f));
	return gse::dot(axis_world, theta);
}

auto gs::locomotion::excess_past_edge(const gse::position value, const gse::position lo, const gse::position hi) -> gse::displacement {
	if (value < lo) {
		return value - lo;
	}
	if (value > hi) {
		return value - hi;
	}
	return {};
}

auto gs::locomotion::reset_state(state& s) -> void {
	s.valid = false;
	s.foot_grounded_l = false;
	s.foot_grounded_r = false;
	s.any_foot_grounded = false;
	s.double_support = false;
	s.lean_world = {};
	s.lean_body = {};
	s.velocity_world = {};
	s.velocity_body = {};
	s.capture_offset_world = {};
	s.capture_offset_body = {};
	s.capture_right = {};
	s.capture_forward = {};
	s.horizontal_speed = {};
}

auto gs::locomotion::state_estimator::run(data& d, gse::read<skeleton_refs> refs, gse::read<gse::physics::transform_component> transforms, gse::read<gse::physics::motion_component> motions, gse::read<gse::physics::collision_component> collisions, gse::read<gait> gaits, gse::write<state> states) -> gse::async::task<> {
	const bool log_now = d.log_timer.tick();
	const auto owner_ids = states.owner_ids();
	for (std::size_t i = 0; i < states.size(); ++i) {
		auto& s = states[i];
		const auto owner = owner_ids[i];
		const auto* r = refs.find(owner);
		if (!r) {
			reset_state(s);
			continue;
		}

		const auto* pelvis_tc = transforms.find(r->pelvis_id);
		const auto* pelvis_mc = motions.find(r->pelvis_id);
		const auto* foot_l_tc = transforms.find(r->foot_l_id);
		const auto* foot_r_tc = transforms.find(r->foot_r_id);
		const auto* foot_l_cc = collisions.find(r->foot_l_id);
		const auto* foot_r_cc = collisions.find(r->foot_r_id);
		const auto* g = gaits.find(owner);

		if (!pelvis_tc || !pelvis_mc || !foot_l_tc || !foot_r_tc || !foot_l_cc || !foot_r_cc) {
			reset_state(s);
			if (log_now) {
				gse::log::println(
					"state_estimator: owner={} missing: pelvis_tc={} pelvis_mc={} "
					"foot_l_tc={} foot_r_tc={} foot_l_cc={} foot_r_cc={}",
					owner.number(),
					pelvis_tc != nullptr,
					pelvis_mc != nullptr,
					foot_l_tc != nullptr,
					foot_r_tc != nullptr,
					foot_l_cc != nullptr,
					foot_r_cc != nullptr
				);
			}
			continue;
		}

		s.pelvis_position = pelvis_tc->position;
		s.pelvis_orientation = pelvis_tc->orientation;
		s.pelvis_velocity = pelvis_mc->current_velocity;
		s.pelvis_forward = gse::rotate_vector(pelvis_tc->orientation, gse::vec3f(0.f, 0.f, -1.f));
		s.pelvis_right = gse::rotate_vector(pelvis_tc->orientation, gse::vec3f(1.f, 0.f, 0.f));

		s.foot_position_l = foot_l_tc->position;
		s.foot_position_r = foot_r_tc->position;

		auto l_aabb = gse::physics::world_aabb_of(*foot_l_tc, *foot_l_cc);
		auto r_aabb = gse::physics::world_aabb_of(*foot_r_tc, *foot_r_cc);
		const auto* toe_l_tc = transforms.find(r->toe_l_id);
		const auto* toe_l_cc = collisions.find(r->toe_l_id);

		if (toe_l_tc && toe_l_cc) {
			const auto toe_aabb = gse::physics::world_aabb_of(*toe_l_tc, *toe_l_cc);
			l_aabb.min = gse::min(l_aabb.min, toe_aabb.min);
			l_aabb.max = gse::max(l_aabb.max, toe_aabb.max);
		}

		const auto* toe_r_tc = transforms.find(r->toe_r_id);
		const auto* toe_r_cc = collisions.find(r->toe_r_id);

		if (toe_r_tc && toe_r_cc) {
			const auto toe_aabb = gse::physics::world_aabb_of(*toe_r_tc, *toe_r_cc);
			r_aabb.min = gse::min(r_aabb.min, toe_aabb.min);
			r_aabb.max = gse::max(r_aabb.max, toe_aabb.max);
		}

		const auto contact_y = d.ground_y + d.ground_tolerance;
		s.foot_grounded_l = l_aabb.min.y() <= contact_y;
		s.foot_grounded_r = r_aabb.min.y() <= contact_y;
		s.any_foot_grounded = s.foot_grounded_l || s.foot_grounded_r;
		s.double_support = s.foot_grounded_l && s.foot_grounded_r;

		const bool swing_support = g && g->current == phase::swing;

		if (swing_support && g->swing_leg == leg::right) {
			s.support_min = l_aabb.min;
			s.support_max = l_aabb.max;
		}
		else if (swing_support && g->swing_leg == leg::left) {
			s.support_min = r_aabb.min;
			s.support_max = r_aabb.max;
		}
		else if (s.double_support || !s.any_foot_grounded) {
			s.support_min = gse::min(l_aabb.min, r_aabb.min);
			s.support_max = gse::max(l_aabb.max, r_aabb.max);
		}
		else if (s.foot_grounded_l) {
			s.support_min = l_aabb.min;
			s.support_max = l_aabb.max;
		}
		else {
			s.support_min = r_aabb.min;
			s.support_max = r_aabb.max;
		}

		s.support_center = gse::lerp(s.support_min, s.support_max, 0.5f);

		const auto* torso_tc = transforms.find(r->torso_id);
		const auto* com_thigh_l = transforms.find(r->thigh_l_id);
		const auto* com_shin_l = transforms.find(r->shin_l_id);
		const auto* com_thigh_r = transforms.find(r->thigh_r_id);
		const auto* com_shin_r = transforms.find(r->shin_r_id);

		if (torso_tc && com_thigh_l && com_shin_l && com_thigh_r && com_shin_r) {
			const auto total_mass = r->pelvis_mass + r->upper_body_mass + (r->thigh_mass + r->shin_mass + r->foot_mass) * 2.f;

			auto com_offset = gse::vec3<gse::displacement>{};
			com_offset += (torso_tc->position - pelvis_tc->position) * (r->upper_body_mass / total_mass);
			com_offset += (com_thigh_l->position - pelvis_tc->position) * (r->thigh_mass / total_mass);
			com_offset += (com_shin_l->position - pelvis_tc->position) * (r->shin_mass / total_mass);
			com_offset += (foot_l_tc->position - pelvis_tc->position) * (r->foot_mass / total_mass);
			com_offset += (com_thigh_r->position - pelvis_tc->position) * (r->thigh_mass / total_mass);
			com_offset += (com_shin_r->position - pelvis_tc->position) * (r->shin_mass / total_mass);
			com_offset += (foot_r_tc->position - pelvis_tc->position) * (r->foot_mass / total_mass);
			s.com_world = pelvis_tc->position + com_offset;
		}
		else {
			s.com_world = pelvis_tc->position;
		}

		const auto com_height = std::max<gse::displacement>(s.com_world.y() - d.ground_y, gse::meters(0.30f));
		s.pendulum_time = gse::seconds(std::sqrt(static_cast<float>(com_height) / 9.81f));

		s.lean_world = gse::vec3<gse::displacement>(
			excess_past_edge(s.com_world.x(), s.support_min.x(), s.support_max.x()),
			gse::meters(0.f),
			excess_past_edge(s.com_world.z(), s.support_min.z(), s.support_max.z())
		);

		s.velocity_world = gse::vec3<gse::velocity>(
			pelvis_mc->current_velocity.x(),
			gse::meters_per_second(0.f),
			pelvis_mc->current_velocity.z()
		);

		s.horizontal_speed = gse::magnitude(s.velocity_world);

		const auto inverse_pelvis = gse::conjugate(pelvis_tc->orientation);
		s.lean_body = gse::rotate_vector(inverse_pelvis, s.lean_world);
		s.velocity_body = gse::rotate_vector(inverse_pelvis, s.velocity_world);
		s.capture_offset_world = s.lean_world + s.velocity_world * s.pendulum_time;
		s.capture_offset_body = gse::rotate_vector(inverse_pelvis, s.capture_offset_world);
		s.capture_right = s.capture_offset_body.x();
		s.capture_forward = -s.capture_offset_body.z();

		const auto* thigh_l_tc = transforms.find(r->thigh_l_id);
		const auto* shin_l_tc = transforms.find(r->shin_l_id);
		const auto* thigh_r_tc = transforms.find(r->thigh_r_id);
		const auto* shin_r_tc = transforms.find(r->shin_r_id);

		if (thigh_l_tc && shin_l_tc && thigh_r_tc && shin_r_tc) {
			s.hip_angle_l = hinge_angle_about_x(pelvis_tc->orientation, thigh_l_tc->orientation);
			s.knee_angle_l = hinge_angle_about_x(thigh_l_tc->orientation, shin_l_tc->orientation);
			s.hip_angle_r = hinge_angle_about_x(pelvis_tc->orientation, thigh_r_tc->orientation);
			s.knee_angle_r = hinge_angle_about_x(thigh_r_tc->orientation, shin_r_tc->orientation);
		}

		s.pelvis_pitch = gse::radians(std::asin(std::clamp(s.pelvis_forward.y(), -1.f, 1.f)));
		s.pelvis_pitch_rate = gse::dot(s.pelvis_right, pelvis_mc->angular_velocity);
		s.valid = true;

		if (log_now) {
			gse::log::println(
				"state_estimator: owner={} pelvis={:+.2f} "
				"v={:+.2f} feet_grounded=({},{}) double={} "
				"support_x=[{:+.2f},{:+.2f}] support_z=[{:+.2f},{:+.2f}] center=({:+.2f},{:+.2f}) "
				"capture=(fwd={:+.3f},right={:+.3f}) speed={:.2f} "
				"pitch={:+.3f} hips_meas=({:+.3f},{:+.3f}) knees_meas=({:+.3f},{:+.3f})",
				owner.number(),
				s.pelvis_position,
				s.pelvis_velocity,
				s.foot_grounded_l,
				s.foot_grounded_r,
				s.double_support,
				s.support_min.x(),
				s.support_max.x(),
				s.support_min.z(),
				s.support_max.z(),
				s.support_center.x(),
				s.support_center.z(),
				s.capture_forward,
				s.capture_right,
				s.horizontal_speed,
				s.pelvis_pitch,
				s.hip_angle_l,
				s.hip_angle_r,
				s.knee_angle_l,
				s.knee_angle_r
			);
		}
	}

	return {};
}

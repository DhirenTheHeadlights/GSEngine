export module gse.runtime:physics_api;

import std;

import gse.math;
import gse.physics;

import :core_api;

export namespace gse::physics {
	struct fixed_joint {
		vec3<length> anchor_a;
		vec3<length> anchor_b;
	};

	struct distance_joint {
		length target;
	};

	struct hinge_joint {
		vec3<length> anchor_a;
		vec3<length> anchor_b;
		unitless::vec3 axis = { 0.f, 1.f, 0.f };
		std::optional<std::pair<angle, angle>> limits;
	};

	struct slider_joint {
		unitless::vec3 axis = { 0.f, 1.f, 0.f };
	};

	auto join(
		id a,
		id b,
		const fixed_joint& config
	) -> void;

	auto join(
		id a,
		id b,
		const distance_joint& config
	) -> void;

	auto join(
		id a,
		id b,
		const hinge_joint& config
	) -> void;

	auto join(
		id a,
		id b,
		const slider_joint& config
	) -> void;
}

auto gse::physics::join(const id a, const id b, const fixed_joint& config) -> void {
	defer<state>([a, b, config](state& s) {
		create_joint(s, joint_definition{
			.entity_a = a,
			.entity_b = b,
			.type = vbd::joint_type::fixed,
			.local_anchor_a = config.anchor_a,
			.local_anchor_b = config.anchor_b,
		});
	});
}

auto gse::physics::join(const id a, const id b, const distance_joint& config) -> void {
	defer<state>([a, b, config](state& s) {
		create_joint(s, joint_definition{
			.entity_a = a,
			.entity_b = b,
			.type = vbd::joint_type::distance,
			.target_distance = config.target,
		});
	});
}

auto gse::physics::join(const id a, const id b, const hinge_joint& config) -> void {
	defer<state>([a, b, config](state& s) {
		create_joint(s, joint_definition{
			.entity_a = a,
			.entity_b = b,
			.type = vbd::joint_type::hinge,
			.local_anchor_a = config.anchor_a,
			.local_anchor_b = config.anchor_b,
			.local_axis_a = config.axis,
			.local_axis_b = config.axis,
			.limit_lower = config.limits ? config.limits->first : radians(-std::numbers::pi_v<float>),
			.limit_upper = config.limits ? config.limits->second : radians(std::numbers::pi_v<float>),
			.limits_enabled = config.limits.has_value(),
		});
	});
}

auto gse::physics::join(const id a, const id b, const slider_joint& config) -> void {
	defer<state>([a, b, config](state& s) {
		create_joint(s, joint_definition{
			.entity_a = a,
			.entity_b = b,
			.type = vbd::joint_type::slider,
			.local_axis_a = config.axis,
			.local_axis_b = config.axis,
		});
	});
}

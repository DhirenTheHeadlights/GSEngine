export module gse.physics:joint_spec;

import std;

import gse.core;
import gse.ecs;
import gse.math;

export namespace gse::physics {
	struct fixed_joint {
		vec3<displacement> anchor_a;
		vec3<displacement> anchor_b;
	};

	struct distance_joint {
		length target;
	};

	struct hinge_joint {
		vec3<displacement> anchor_a;
		vec3<displacement> anchor_b;
		vec3f axis = { 0.f, 1.f, 0.f };
		std::optional<std::pair<angle, angle>> limits;
	};

	struct slider_joint {
		vec3f axis = { 0.f, 1.f, 0.f };
	};

	struct spring_joint {
		length target;
		inverse_mass compliance = per_kilograms(0.01f);
		float damping = 0.5f;
	};

	struct muscle_joint {
		vec3<displacement> anchor_a;
		vec3<displacement> anchor_b;
		length rest_length;
		inverse_mass compliance = per_kilograms(0.001f);
		float damping = 2.5f;
		force max_force = newtons(3200.f);
	};

	struct ball_joint {
		vec3<displacement> anchor_a;
		vec3<displacement> anchor_b;
		vec3<angular_stiffness> rest_stiffness = {};
	};

	struct universal_joint {
		vec3<displacement> anchor_a;
		vec3<displacement> anchor_b;
		vec3f twist_axis = { 0.f, 1.f, 0.f };
	};

	using joint_config = std::variant<fixed_joint, distance_joint, hinge_joint, slider_joint, spring_joint, muscle_joint, ball_joint, universal_joint>;

	struct joint_spec {
		[[= networked]] id entity_a;
		[[= networked]] id entity_b;
		[[= networked]] joint_config config;
		bool resolved = false;
	};
}

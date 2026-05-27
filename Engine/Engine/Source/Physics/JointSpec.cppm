export module gse.physics:joint_spec;

import std;

import :system;

import gse.core;
import gse.ecs;
import gse.math;

export namespace gse::physics {
	using joint_config = std::variant<fixed_joint, distance_joint, hinge_joint, slider_joint, spring_joint, muscle_joint, ball_joint, universal_joint>;

	struct joint_spec {
		[[= networked]] id entity_a;
		[[= networked]] id entity_b;
		[[= networked]] joint_config config;
		bool resolved = false;
	};
}

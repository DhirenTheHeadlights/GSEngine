export module gse.physics:joint_spec;

import std;

import gse.core;
import gse.ecs;
import gse.math;

export namespace gse::physics {
	struct joint_spec {
		[[= networked]] id entity_a;
		[[= networked]] id entity_b;
		[[= networked]] joint_config config;
		bool resolved = false;
	};
}

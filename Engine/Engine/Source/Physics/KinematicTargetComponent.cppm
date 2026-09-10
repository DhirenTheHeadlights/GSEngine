export module gse.physics:kinematic_target_component;

import gse.ecs;
import gse.math;

export namespace gse::physics {
	struct kinematic_target_component {
		[[= networked]] vec3<current_position> position;
		[[= networked]] quat orientation = quat(1.f, 0.f, 0.f, 0.f);
	};
}
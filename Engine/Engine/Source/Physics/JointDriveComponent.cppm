export module gse.physics:joint_drive_component;

import gse.ecs;
import gse.math;

export namespace gse::physics {
	struct joint_drive_component {
		[[= networked]] vec3<angle> target;
		[[= networked]] vec3<angular_stiffness> stiffness;
		[[= networked]] float damping = 0.f;
		[[= networked]] torque max_torque = newton_meters(0.f);
		[[= networked]] bool enabled = true;
	};
}
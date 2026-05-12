export module gse.physics:motor_component;

import std;

import gse.core;
import gse.ecs;
import gse.math;
import gse.meta;

export namespace gse::physics {
	struct motor_component {
		[[= networked]] vec3<velocity> velocity_drive_target;
		[[= networked]] bool horizontal_only = true;
	};
}

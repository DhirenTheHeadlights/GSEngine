export module gse.physics:muscle_component;

import std;

import gse.core;
import gse.ecs;
import gse.math;
import gse.meta;

export namespace gse::physics {
	struct muscle_component {
		[[= networked]] float activation = 0.f;
	};
}

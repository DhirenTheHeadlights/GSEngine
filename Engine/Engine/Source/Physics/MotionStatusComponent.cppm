export module gse.physics:motion_status_component;

import std;

import gse.core;
import gse.ecs;
import gse.meta;

export namespace gse::physics {
	struct motion_status_component {
		bool airborne = true;
		bool sleeping = false;
	};
}

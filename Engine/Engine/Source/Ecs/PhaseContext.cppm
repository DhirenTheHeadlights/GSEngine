export module gse.ecs:phase_context;

import std;

import gse.core;

import :registry;

export namespace gse {
	struct shutdown_context {
		registry& reg;
	};
}

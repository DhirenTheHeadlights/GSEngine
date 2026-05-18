export module gse.graphics:primitive_resolver;

import std;

import :primitives;

import gse.concurrency;
import gse.core;
import gse.ecs;

export namespace gse::primitive_resolver {
	struct system {
		static auto run(
			run_context& ctx,
			const primitives::data& prims
		) -> async::task<>;
	};
}

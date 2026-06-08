export module gse.graphics:primitive_resolver;

import std;

import :primitives;
import :primitive_specs;
import :render_component;

import gse.concurrency;
import gse.core;
import gse.ecs;

export namespace gse::primitive_resolver {
	struct system {
		struct run {
			static auto ensure_renders(
				run_context& ctx,
				structural<render_component> renders
			) -> async::task<>;

			static auto populate(
				run_context& ctx,
				const primitives::data& prims,
				write<primitive_box_spec> boxes,
				write<primitive_sphere_spec> spheres,
				write<render_component> renders
			) -> async::task<>;
		};
	};
}

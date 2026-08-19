export module gse.graphics:primitive_resolver;

import std;

import :primitives;
import :primitive_specs;
import :render_component;

import gse.assets;
import gse.concurrency;
import gse.core;
import gse.ecs;

export namespace gse::primitive_resolver {
	struct [[= system_state<"PrimitiveResolver">{}]] data {};

	[[= system_run<>{}]]
	auto ensure_renders(
		write<primitive_box_spec> boxes,
		write<primitive_sphere_spec> spheres,
		write<primitive_cylinder_spec> cylinders,
		write<primitive_mountain_ring_spec> mountain_rings,
		structural<render_component> renders
	) -> async::task<>;

	[[= system_run<1>{}]]
	auto populate(
		context& ctx,
		const primitives::data& prims,
		shared_view<asset::data> assets,
		write<primitive_box_spec> boxes,
		write<primitive_sphere_spec> spheres,
		write<primitive_cylinder_spec> cylinders,
		write<primitive_mountain_ring_spec> mountain_rings,
		write<render_component> renders
	) -> async::task<>;
}

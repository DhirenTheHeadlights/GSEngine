module gse.graphics:primitive_resolver_impl;

import std;

import :primitive_resolver;
import :model;
import :mountain_ring;
import :primitive_specs;
import :primitives;
import :render_component;


import gse.assets;
import gse.concurrency;
import gse.core;
import gse.ecs;
import gse.math;
import gse.meta;

namespace gse::primitive_resolver {
	auto attach_box(
		const primitive_box_spec& spec,
		const resource::handle<model>& handle,
		render_component& render
	) -> void;

	auto attach_sphere(
		const primitive_sphere_spec& spec,
		const resource::handle<model>& handle,
		render_component& render
	) -> void;

	auto attach_cylinder(
		const primitive_cylinder_spec& spec,
		const resource::handle<model>& handle,
		render_component& render
	) -> void;

	auto attach_mountain_ring(
		const primitive_mountain_ring_spec& spec,
		const resource::handle<model>& handle,
		render_component& render
	) -> void;

	auto mountain_ring_asset_name(
		const primitive_mountain_ring_spec& spec
	) -> std::string;
}

auto gse::primitive_resolver::attach_box(const primitive_box_spec& spec, const resource::handle<model>& handle, render_component& render) -> void {
	if (render.model_count >= render_component::max_models) {
		return;
	}
	const auto idx = render.model_count;
	render.models[idx] = handle;
	render.tints[idx] = vec4f(spec.material.base_color, spec.material.opacity);
	render.sizes[idx] = spec.size;
	++render.model_count;
}

auto gse::primitive_resolver::attach_sphere(const primitive_sphere_spec& spec, const resource::handle<model>& handle, render_component& render) -> void {
	if (render.model_count >= render_component::max_models) {
		return;
	}
	const auto idx = render.model_count;
	render.models[idx] = handle;
	render.tints[idx] = vec4f(spec.material.base_color, spec.material.opacity);
	render.sizes[idx] = vec3<length>(spec.radius);
	++render.model_count;
}

auto gse::primitive_resolver::attach_cylinder(const primitive_cylinder_spec& spec, const resource::handle<model>& handle, render_component& render) -> void {
	if (render.model_count >= render_component::max_models) {
		return;
	}
	const auto idx = render.model_count;
	render.models[idx] = handle;
	render.tints[idx] = vec4f(spec.material.base_color, spec.material.opacity);
	render.sizes[idx] = vec3<length>(spec.radius, spec.height, spec.radius);
	++render.model_count;
}

auto gse::primitive_resolver::attach_mountain_ring(const primitive_mountain_ring_spec& spec, const resource::handle<model>& handle, render_component& render) -> void {
	if (render.model_count >= render_component::max_models) {
		return;
	}
	const auto idx = render.model_count;
	render.models[idx] = handle;
	render.tints[idx] = vec4f(1.0f, 1.0f, 1.0f, spec.material.opacity);
	render.sizes[idx] = vec3<length>(meters(1.0f));
	++render.model_count;
}

auto gse::primitive_resolver::mountain_ring_asset_name(const primitive_mountain_ring_spec& spec) -> std::string {
	std::uint64_t key = 1469598103934665603ull;
	key = hash_combine(key, spec.material.base_color.x());
	key = hash_combine(key, spec.material.base_color.y());
	key = hash_combine(key, spec.material.base_color.z());
	key = hash_combine(key, spec.material.roughness);
	key = hash_combine(key, spec.material.metallic);
	key = hash_combine(key, spec.snow_color.x());
	key = hash_combine(key, spec.snow_color.y());
	key = hash_combine(key, spec.snow_color.z());
	key = hash_combine(key, spec.snow_line);
	key = hash_combine(key, spec.snow_slope);
	key = hash_combine(key, spec.snow_roughness);
	key = hash_combine(key, spec.inner_radius);
	key = hash_combine(key, spec.outer_radius);
	key = hash_combine(key, spec.peak_height);
	key = hash_combine(key, spec.ridge_base);
	key = hash_combine(key, spec.massif_depth);
	key = hash_combine(key, spec.massif_cells);
	key = hash_combine(key, spec.angular_segments);
	key = hash_combine(key, spec.radial_segments);
	key = hash_combine(key, spec.ridge_cells);
	key = hash_combine(key, spec.radial_cells);
	key = hash_combine(key, spec.octaves);
	key = hash_combine(key, spec.seed);
	return std::format("Primitives/mountain_ring_{:016x}", key);
}

auto gse::primitive_resolver::ensure_renders(write<primitive_box_spec> boxes, write<primitive_sphere_spec> spheres, write<primitive_cylinder_spec> cylinders, write<primitive_mountain_ring_spec> mountain_rings, structural<render_component> renders) -> async::task<> {
	for (const auto eid : boxes.drain(component_event::added)) {
		if (!renders.contains(eid)) {
			renders.add(eid);
		}
	}
	for (const auto eid : spheres.drain(component_event::added)) {
		if (!renders.contains(eid)) {
			renders.add(eid);
		}
	}
	for (const auto eid : cylinders.drain(component_event::added)) {
		if (!renders.contains(eid)) {
			renders.add(eid);
		}
	}
	for (const auto eid : mountain_rings.drain(component_event::added)) {
		if (!renders.contains(eid)) {
			renders.add(eid);
		}
	}
	co_return;
}

auto gse::primitive_resolver::populate(context& ctx, const primitives::data& prims, shared_view<asset::data> assets, write<primitive_box_spec> boxes, write<primitive_sphere_spec> spheres, write<primitive_cylinder_spec> cylinders, write<primitive_mountain_ring_spec> mountain_rings, write<render_component> renders) -> async::task<> {
	(void)ctx;

	const auto box_owners = boxes.owner_ids();
	for (std::size_t i = 0; i < boxes.size(); ++i) {
		auto& spec = boxes[i];
		if (spec.resolved) {
			continue;
		}
		auto* render = renders.find(box_owners[i]);
		if (!render) {
			continue;
		}
		attach_box(spec, prims.unit_box, *render);
		spec.resolved = true;
	}

	const auto sphere_owners = spheres.owner_ids();
	for (std::size_t i = 0; i < spheres.size(); ++i) {
		auto& spec = spheres[i];
		if (spec.resolved) {
			continue;
		}
		auto* render = renders.find(sphere_owners[i]);
		if (!render) {
			continue;
		}
		const auto& handle = (spec.lod == sphere_lod::lo) ? prims.sphere_lo
			: (spec.lod == sphere_lod::hi)				  ? prims.sphere_hi
														  : prims.sphere_mid;
		attach_sphere(spec, handle, *render);
		spec.resolved = true;
	}

	const auto cylinder_owners = cylinders.owner_ids();
	for (std::size_t i = 0; i < cylinders.size(); ++i) {
		auto& spec = cylinders[i];
		if (spec.resolved) {
			continue;
		}
		auto* render = renders.find(cylinder_owners[i]);
		if (!render) {
			continue;
		}
		attach_cylinder(spec, prims.unit_cylinder, *render);
		spec.resolved = true;
	}

	const auto mountain_ring_owners = mountain_rings.owner_ids();
	for (std::size_t i = 0; i < mountain_rings.size(); ++i) {
		auto& spec = mountain_rings[i];
		if (spec.resolved) {
			continue;
		}
		auto* render = renders.find(mountain_ring_owners[i]);
		if (!render) {
			continue;
		}
		const auto name = mountain_ring_asset_name(spec);
		auto handle = asset::try_get<model>(assets, name);
		if (handle.state() == resource::state::unloaded) {
			handle = asset::queue<model>(assets, name, build_mountain_ring_meshes(spec));
		}
		attach_mountain_ring(spec, handle, *render);
		spec.resolved = true;
	}

	co_return;
}

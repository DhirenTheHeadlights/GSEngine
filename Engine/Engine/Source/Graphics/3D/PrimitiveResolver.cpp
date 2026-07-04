module gse.graphics:primitive_resolver_impl;

import std;

import :primitive_resolver;
import :model;
import :primitive_specs;
import :primitives;
import :render_component;


import gse.concurrency;
import gse.core;
import gse.ecs;
import gse.math;

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
}

auto gse::primitive_resolver::attach_box(const primitive_box_spec& spec, const resource::handle<model>& handle, render_component& render) -> void {
	if (render.model_count >= render_component::max_models) {
		return;
	}
	const auto idx = render.model_count;
	render.models[idx] = handle;
	render.tints[idx] = vec4f(spec.material.base_color.x(), spec.material.base_color.y(), spec.material.base_color.z(), spec.material.opacity);
	render.sizes[idx] = spec.size;
	++render.model_count;
}

auto gse::primitive_resolver::attach_sphere(const primitive_sphere_spec& spec, const resource::handle<model>& handle, render_component& render) -> void {
	if (render.model_count >= render_component::max_models) {
		return;
	}
	const auto idx = render.model_count;
	render.models[idx] = handle;
	render.tints[idx] = vec4f(spec.material.base_color.x(), spec.material.base_color.y(), spec.material.base_color.z(), spec.material.opacity);
	render.sizes[idx] = vec3<length>(spec.radius);
	++render.model_count;
}

auto gse::primitive_resolver::ensure_renders(context& ctx, structural<render_component> renders) -> async::task<> {
	for (const auto eid : ctx.drain_component_adds<primitive_box_spec>()) {
		if (!renders.contains(eid)) {
			renders.add(eid);
		}
	}
	for (const auto eid : ctx.drain_component_adds<primitive_sphere_spec>()) {
		if (!renders.contains(eid)) {
			renders.add(eid);
		}
	}
	co_return;
}

auto gse::primitive_resolver::populate(context& ctx, const primitives::data& prims, write<primitive_box_spec> boxes, write<primitive_sphere_spec> spheres, write<render_component> renders) -> async::task<> {
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

	co_return;
}

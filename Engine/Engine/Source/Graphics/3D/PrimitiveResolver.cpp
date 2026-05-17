module gse.graphics;

import std;

import :model;
import :primitive_resolver;
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
    render.tints[idx] = spec.material.base_color;
    render.sizes[idx] = spec.size;
    ++render.model_count;
}

auto gse::primitive_resolver::attach_sphere(const primitive_sphere_spec& spec, const resource::handle<model>& handle, render_component& render) -> void {
    if (render.model_count >= render_component::max_models) {
        return;
    }
    const auto idx = render.model_count;
    render.models[idx] = handle;
    render.tints[idx] = spec.material.base_color;
    render.sizes[idx] = vec3<length>(spec.radius);
    ++render.model_count;
}

auto gse::primitive_resolver::system::run(run_context& ctx, data&, const primitives::data& prims) -> async::task<> {
    while (true) {
        for (const auto eid : ctx.drain_component_adds<primitive_box_spec>()) {
            if (!ctx.try_component<render_component>(eid)) {
                ctx.add_component<render_component>(eid);
            }
        }
        for (const auto eid : ctx.drain_component_adds<primitive_sphere_spec>()) {
            if (!ctx.try_component<render_component>(eid)) {
                ctx.add_component<render_component>(eid);
            }
        }

        {
            auto [boxes, spheres, renders] = co_await ctx.acquire_with(
                write_v<primitive_box_spec>,
                write_v<primitive_sphere_spec>,
                write_v<render_component>
            );

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
                                   : (spec.lod == sphere_lod::hi) ? prims.sphere_hi
                                                                  : prims.sphere_mid;
                attach_sphere(spec, handle, *render);
                spec.resolved = true;
            }
        }

        co_await ctx.next_tick();
    }
}

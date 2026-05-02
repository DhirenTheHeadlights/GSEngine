module gse.graphics;

import std;

import :render_component;
import :mesh;
import :model;
import :skinned_model;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;

gse::render_component::render_component(const id owner_id, const params& p) : component(owner_id) {
    const auto n = static_cast<std::size_t>(std::min<std::size_t>(p.models.size(), render_component_data::max_models));

    for (std::size_t i = 0; i < n; ++i) {
        networked_data().models[i] = p.models[i];
    }
    networked_data().model_count = static_cast<std::uint32_t>(n);
    networked_data().render = p.render;
    networked_data().render_bounding_boxes = p.render_bounding_boxes;

    model_instances.clear();
    model_instances.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        model_instances.emplace_back(networked_data().models[i]);
    }

    const auto sn = static_cast<std::size_t>(std::min<std::size_t>(p.skinned_models.size(), render_component_data::max_models));

    for (std::size_t i = 0; i < sn; ++i) {
        networked_data().skinned_models[i] = p.skinned_models[i];
    }
    networked_data().skinned_model_count = static_cast<std::uint32_t>(sn);

    skinned_model_instances.clear();
    skinned_model_instances.reserve(sn);
    for (std::size_t i = 0; i < sn; ++i) {
        skinned_model_instances.emplace_back(networked_data().skinned_models[i]);
    }

    center_of_mass = p.center_of_mass;
    has_calculated_com = p.has_calculated_com;
}

gse::render_component::render_component(const id owner_id, const network_data_t& nd) : component(owner_id, nd) {
    const auto n = static_cast<std::size_t>(networked_data().model_count);
    model_instances.clear();
    model_instances.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        model_instances.emplace_back(networked_data().models[i]);
    }

    const auto sn = static_cast<std::size_t>(networked_data().skinned_model_count);
    skinned_model_instances.clear();
    skinned_model_instances.reserve(sn);
    for (std::size_t i = 0; i < sn; ++i) {
        skinned_model_instances.emplace_back(networked_data().skinned_models[i]);
    }
}

auto gse::render_init::try_wire(render_component& rc) -> bool {
    if (rc.has_calculated_com) {
        return true;
    }

    for (std::size_t i = 0; i < rc.networked_data().model_count; ++i) {
        if (!rc.networked_data().models[i].valid()) {
            return false;
        }
    }

    for (std::size_t i = 0; i < rc.networked_data().skinned_model_count; ++i) {
        if (!rc.networked_data().skinned_models[i].valid()) {
            return false;
        }
    }

    if (rc.model_instances.size() != rc.networked_data().model_count) {
        rc.model_instances.clear();
        rc.model_instances.reserve(rc.networked_data().model_count);
        for (std::size_t i = 0; i < rc.networked_data().model_count; ++i) {
            rc.model_instances.emplace_back(rc.networked_data().models[i]);
        }
    }

    if (rc.skinned_model_instances.size() != rc.networked_data().skinned_model_count) {
        rc.skinned_model_instances.clear();
        rc.skinned_model_instances.reserve(rc.networked_data().skinned_model_count);
        for (std::size_t i = 0; i < rc.networked_data().skinned_model_count; ++i) {
            rc.skinned_model_instances.emplace_back(rc.networked_data().skinned_models[i]);
        }
    }

    vec3<length> sum;
    std::size_t count = 0;

    for (std::size_t i = 0; i < rc.networked_data().model_count; ++i) {
        sum += rc.networked_data().models[i]->center_of_mass();
        ++count;
    }

    for (std::size_t i = 0; i < rc.networked_data().skinned_model_count; ++i) {
        sum += rc.networked_data().skinned_models[i]->center_of_mass();
        ++count;
    }

    rc.center_of_mass = (count == 0) ? vec3<length>{} : sum / static_cast<float>(count);
    rc.has_calculated_com = true;
    return true;
}

auto gse::render_init::system::update(update_context& ctx, state& s) -> async::task<> {
    for (const auto& owner : ctx.drain_component_adds<render_component>()) {
        s.pending.insert(owner);
    }

    auto [rcs] = co_await ctx.acquire<write<render_component>>();

    std::erase_if(s.pending, [&rcs](const id owner) {
        auto* rc = rcs.find(owner);
        if (!rc) {
            return true;
        }
        return try_wire(*rc);
    });

    co_return;
}

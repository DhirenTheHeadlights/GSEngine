export module gse.graphics:render_component;

import std;

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

export namespace gse {
    struct render_component_net {
        static constexpr std::size_t max_models = 16;
        std::array<resource::handle<model>, max_models> models{};
        std::uint32_t model_count = 0;
        std::array<resource::handle<skinned_model>, max_models> skinned_models{};
        std::uint32_t skinned_model_count = 0;
        bool render = true;
        bool render_bounding_boxes = true;
    };

    struct render_component_data {
        std::array<resource::handle<model>, render_component_net::max_models> models{};
        std::uint32_t model_count = 0;
        std::array<resource::handle<skinned_model>, render_component_net::max_models> skinned_models{};
        std::uint32_t skinned_model_count = 0;
        bool render = true;
        bool render_bounding_boxes = true;

        std::vector<model_instance> model_instances;
        std::vector<skinned_model_instance> skinned_model_instances;
        vec3<length> center_of_mass;
        bool has_calculated_com = false;
    };

    struct render_component final : component<render_component_data, render_component_net> {
        struct params {
            std::vector<resource::handle<model>> models;
            std::vector<resource::handle<skinned_model>> skinned_models;
            vec3<length> center_of_mass;
            bool render = true;
            bool render_bounding_boxes = true;
            bool has_calculated_com = false;
        };

        render_component(
            id owner_id,
            const params& p
        );

        render_component(
            id owner_id,
            const network_data_t& nd
        );
    };
}

export namespace gse::render_init {
    struct state {
        std::unordered_set<id> pending;
    };

    struct system {
        static auto update(
            update_context& ctx,
            state& s
        ) -> async::task<>;
    };
}

namespace gse::render_init {
    auto try_wire(
        render_component& rc
    ) -> bool;
}

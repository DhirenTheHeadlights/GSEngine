export module gse.graphics:render_component;

import std;

import :mesh;
import :model;
import :skinned_model;

import gse.core;
import gse.ecs;
import gse.meta;

export namespace gse {
    struct render_component {
        static constexpr std::size_t max_models = 16;

        [[= networked]] std::array<resource::handle<model>, max_models> models{};
        [[= networked]] std::uint32_t model_count = 0;
        [[= networked]] std::array<resource::handle<skinned_model>, max_models> skinned_models{};
        [[= networked]] std::uint32_t skinned_model_count = 0;
        [[= networked]] bool render = true;
        [[= networked]] bool render_bounding_boxes = true;
    };
}

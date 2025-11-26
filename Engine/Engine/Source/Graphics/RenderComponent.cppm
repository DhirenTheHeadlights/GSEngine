export module gse.graphics:render_component;

import std;

import :mesh;
import :model;

import gse.utility;
import gse.physics.math;

export namespace gse {
    struct render_component_net {
        static constexpr std::size_t max_models = 128;
        std::array<resource::handle<model>, max_models> models{};
        std::uint32_t model_count = 0;
        bool render = true;
        bool render_bounding_boxes = true;
    };

    struct render_component_data {
        std::array<resource::handle<model>, render_component_net::max_models> models{};
        std::uint32_t model_count = 0;
        bool render = true;
        bool render_bounding_boxes = true;

        std::vector<model_instance> model_instances;
        vec3<length> center_of_mass;
        bool has_calculated_com = false;
    };

    struct render_component final : component<render_component_data, render_component_net> {
        struct params {
            std::vector<resource::handle<model>> models;
            vec3<length> center_of_mass;
            bool render = true;
            bool render_bounding_boxes = true;
            bool has_calculated_com = false;
        };

        render_component(
            const id owner_id,
            const params& p
        ) : component(owner_id) {
            const auto n = static_cast<std::size_t>(std::min<std::size_t>(p.models.size(), render_component_net::max_models));

            // write network prefix
            for (std::size_t i = 0; i < n; ++i) {
                networked_data().models[i] = p.models[i];
            }
            networked_data().model_count = static_cast<std::uint32_t>(n);
            networked_data().render = p.render;
            networked_data().render_bounding_boxes = p.render_bounding_boxes;

            // build runtime instances from handles
            model_instances.clear();
            model_instances.reserve(n);
            for (std::size_t i = 0; i < n; ++i) {
                model_instances.emplace_back(networked_data().models[i]);
            }

            center_of_mass = p.center_of_mass;
            has_calculated_com = p.has_calculated_com;
        }

        render_component(
            const id owner_id,
            const network_data_t& nd
        ) : component(owner_id, nd) {
            const auto n = static_cast<std::size_t>(networked_data().model_count);

            model_instances.clear();
            model_instances.reserve(n);
            for (std::size_t i = 0; i < n; ++i) {
                model_instances.emplace_back(networked_data().models[i]);
            }
        }

        auto on_registry(
            registry* reg
        ) -> void override;
    };
}

auto gse::render_component::on_registry(registry* reg) -> void {
    auto owner_id = this->owner_id();

    reg->add_deferred_action(owner_id, [owner_id](registry& r) -> bool {
        if (auto* rc = r.try_linked_object_write<render_component>(owner_id)) {
            for (std::size_t i = 0; i < rc->networked_data().model_count; ++i) {
                if (!rc->networked_data().models[i].valid()) return false;
            }

            if (rc->model_instances.size() != rc->networked_data().model_count) {
                rc->model_instances.clear();
                rc->model_instances.reserve(rc->networked_data().model_count);
                for (std::size_t i = 0; i < rc->networked_data().model_count; ++i) {
                    rc->model_instances.emplace_back(rc->networked_data().models[i]);
                }
            }

            vec3<length> sum;
            std::size_t count = 0;

            for (std::size_t i = 0; i < rc->networked_data().model_count; ++i) {
                sum += rc->networked_data().models[i]->center_of_mass();
                ++count;
            }

            rc->center_of_mass = (count == 0) ? vec3<length>{} : sum / static_cast<float>(count);
            rc->has_calculated_com = true;
            return true;
        }
        return false;
    });
}

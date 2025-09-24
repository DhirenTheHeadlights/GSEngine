export module gse.graphics:render_component;

import std;

import :mesh;
import :model;

import gse.utility;
import gse.physics.math;

export namespace gse {
    struct render_component_data {
        std::vector<model_instance> models;
        std::vector<mesh_data> bounding_box_meshes;
        vec3<length> center_of_mass;
        bool render = true;
        bool render_bounding_boxes = true;
        bool has_calculated_com = false;
	};

    struct render_component final : component<render_component_data>, non_copyable {
        render_component(const id& owner_id, render_component_data data = {})
    		: component(owner_id),
    	      models(std::move(data.models)),
    	      center_of_mass(data.center_of_mass),
    	      render(data.render),
    	      render_bounding_boxes(data.render_bounding_boxes),
    	      has_calculated_com(data.has_calculated_com) {
	        for (auto& mesh : data.bounding_box_meshes) {
                bounding_box_meshes.emplace_back(std::move(mesh));
			}
        }

        auto on_registry(registry* reg) -> void override;

        std::vector<model_instance> models;
        std::vector<mesh> bounding_box_meshes;

        vec3<length> center_of_mass;

        bool render = true;
        bool render_bounding_boxes = true;
        bool has_calculated_com = false;
    };
}

auto gse::render_component::on_registry(registry* reg) -> void {
	auto owner_id = this->owner_id();

	reg->add_deferred_action(owner_id, [owner_id](registry& r) -> bool {
	    if (auto* rc = r.try_linked_object_write<render_component>(owner_id)) {
	        if (!std::ranges::all_of(rc->models, [](auto& mi){ return mi.handle().valid(); })) {
	            return false; 
	        }

	        vec3<length> sum;

	        for (const auto& mi : rc->models) {
		        sum += mi.handle()->center_of_mass();
	        }

	        rc->center_of_mass = rc->models.empty() ? vec3<length>{} : sum / static_cast<float>(rc->models.size());
	        rc->has_calculated_com = true;
	        return true; 
	    }
	    return false;
	});
}

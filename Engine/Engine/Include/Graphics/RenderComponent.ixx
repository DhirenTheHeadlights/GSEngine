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

    struct render_component : component<render_component_data> {
        render_component(const id& owner_id, const render_component_data& data = {})
    		: component(owner_id),
    	      models(std::move(data.models)),
    	      center_of_mass(data.center_of_mass),
    	      render(data.render),
    	      render_bounding_boxes(data.render_bounding_boxes),
    	      has_calculated_com(data.has_calculated_com) {
	        for (const auto& mesh : data.bounding_box_meshes) {
                bounding_box_meshes.emplace_back(mesh);
			}
        }

        std::vector<model_instance> models;
        std::vector<mesh> bounding_box_meshes;

        vec3<length> center_of_mass;

        bool render = true;
        bool render_bounding_boxes = true;
        bool has_calculated_com = false;
    };
}
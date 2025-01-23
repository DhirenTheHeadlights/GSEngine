#include "Graphics/RenderComponent.h"


auto gse::render_component::update_bounding_box_meshes() -> void {
	for (auto& bounding_box_mesh : bounding_box_meshes) {
		bounding_box_mesh.update();
	}
}

auto gse::render_component::set_mesh_positions(const vec3<length>& position) -> void {
    for (auto& id : model_ids) {
        for (auto& mesh : model_loader::get_model_by_id(id).meshes) {
            mesh.set_position(position);
        }
    }
}

auto gse::render_component::set_all_mesh_material_strings(const std::string& material_string) -> void {
	for (auto& id : model_ids) {
		for (auto& mesh : model_loader::get_model_by_id(id).meshes) {
			mesh.m_material_name = material_string;
		}
	}
}

auto gse::render_component::calculate_center_of_mass() -> void {
	vec3<length> sum;
	for (auto& id : model_ids) {
        for (auto& mesh : model_loader::get_model_by_id(id).meshes) {
            sum += mesh.calculate_center_of_mass();
		}
	}
	int total_mesh_size = 0;
	for (auto& id : model_ids) total_mesh_size = model_loader::get_model_by_id(id).meshes.size();
	center_of_mass = sum / static_cast<float>(total_mesh_size);
}
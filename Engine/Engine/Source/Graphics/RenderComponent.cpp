module gse.graphics.render_component;

import std;

import gse.graphics.model_loader;

gse::render_component::render_component(const std::uint32_t id, class id* model_id) : component(id) {
	models.emplace_back(model_id, model_loader::get_model_by_id(model_id));
	calculate_center_of_mass();
}

auto gse::render_component::update_bounding_box_meshes() -> void {
	for (auto& bounding_box_mesh : bounding_box_meshes) {
		bounding_box_mesh.update();
	}
}

auto gse::render_component::set_model_positions(const vec3<length>& position) -> void {
	for (auto& model_handle : models) {
		model_handle.set_position(position);
	}
}

auto gse::render_component::set_model_material(const std::string& material_string) -> void {
	for (auto& model_handle : models) {
		model_handle.set_material(material_string);
	}
}

auto gse::render_component::calculate_center_of_mass() -> void {
	vec3<length> sum;
	int number_of_meshes = 0;
	for (const auto& model_handle : models) {
		const auto& model = model_loader::get_model_by_id(model_handle.get_model_id());
		for (auto& model_mesh : model.meshes) {
			sum += model_mesh.center_of_mass;
			number_of_meshes += static_cast<int>(model.meshes.size());
		}
	}

	center_of_mass = sum / static_cast<float>(number_of_meshes);
}
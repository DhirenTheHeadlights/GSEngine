#include "Graphics/RenderComponent.h"

void gse::render_component::update_bounding_box_meshes() {
	for (auto& bounding_box_mesh : bounding_box_meshes) {
		bounding_box_mesh.update();
	}
}

void gse::render_component::set_mesh_positions(const vec3<length>& position) {
	for (auto& mesh : meshes) {
		mesh.set_position(position);
	}
}

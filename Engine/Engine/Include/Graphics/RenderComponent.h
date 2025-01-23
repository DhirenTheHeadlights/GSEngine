#pragma once
#include <memory>
#include <vector>

#include "Core/EngineComponent.h"
#include "Graphics/3D/BoundingBoxMesh.h"
#include "Graphics/3D/Mesh.h"
#include "Graphics/3D/ModelLoader.h"
#include "Core/ID.h"

namespace gse {
	struct render_component final : component {
		render_component(const std::uint32_t id) : component(id) {}
		render_component(const std::uint32_t id, class id* model_id) : component(id) { model_ids.push_back(model_id); }

		render_component(render_component&&) noexcept = default;
		auto operator=(render_component&&) noexcept -> render_component& = default;

		render_component(const render_component&) = delete;
		auto operator=(const render_component&) -> render_component& = delete;

		auto update_bounding_box_meshes() -> void;
		auto set_all_mesh_material_strings(const std::string& material_string) -> void;
		auto set_mesh_positions(const vec3<length>& position) -> void;

		std::vector<id*> model_ids;
		std::vector<bounding_box_mesh> bounding_box_meshes;

		auto calculate_center_of_mass() -> void;
		vec3<length> center_of_mass;

		bool render = true;
		bool render_bounding_boxes = true;
	};
}

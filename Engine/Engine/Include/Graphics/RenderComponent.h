#pragma once
#include <memory>
#include <vector>

#include "Core/EngineComponent.h"
#include "Graphics/3D/BoundingBoxMesh.h"
#include "Graphics/3D/Mesh.h"

namespace gse {
	struct render_component final : component {
		render_component(id* id) : component(id) {}
		render_component(id* id, std::vector<mesh> meshes) : component(id), meshes(std::move(meshes)) {}

		render_component(render_component&&) noexcept = default;
		render_component& operator=(render_component&&) noexcept = default;

		render_component(const render_component&) = delete;
		render_component& operator=(const render_component&) = delete;

		void update_bounding_box_meshes();
		void set_mesh_positions(const vec3<length>& position);

		std::vector<mesh> meshes;
		std::vector<bounding_box_mesh> bounding_box_meshes;

		bool render = true;
		bool render_bounding_boxes = true;
	};
}

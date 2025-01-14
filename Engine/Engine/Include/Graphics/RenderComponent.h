#pragma once
#include <memory>
#include <vector>

#include "Core/EngineComponent.h"
#include "Graphics/3D/BoundingBoxMesh.h"
#include "Graphics/3D/Mesh.h"

namespace gse {
	struct render_component final : component {
		render_component(const std::uint32_t id) : component(id) {}
		render_component(const std::uint32_t id, std::vector<mesh> meshes) : component(id), meshes(std::move(meshes)) {}
		render_component(const std::uint32_t id, const std::string& path) : component(id) { load_model(path); }

		render_component(render_component&&) noexcept = default;
		auto operator=(render_component&&) noexcept -> render_component& = default;

		render_component(const render_component&) = delete;
		auto operator=(const render_component&) -> render_component& = delete;

		auto load_model(const std::string& path) -> void;

		auto update_bounding_box_meshes() -> void;
		auto set_mesh_positions(const vec3<length>& position) -> void;
		auto set_all_mesh_material_strings(const std::string& material_string) -> void;

		std::vector<mesh> meshes;
		std::vector<bounding_box_mesh> bounding_box_meshes;

		auto calculate_center_of_mass() -> void;
		vec3<length> center_of_mass;

		bool render = true;
		bool render_bounding_boxes = true;
	};
}

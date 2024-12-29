#pragma once
#include <memory>
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Core/EngineComponent.h"
#include "Graphics/3D/BoundingBoxMesh.h"
#include "Graphics/3D/Mesh.h"

namespace gse {
	class render_component final : public component {
	public:
		render_component(id* id) : component(id) {}
		render_component(id* id, std::vector<std::unique_ptr<mesh>> meshes) : component(id), m_meshes(std::move(meshes)) {}

		render_component(render_component&&) noexcept = default;
		render_component& operator=(render_component&&) noexcept = default;

		render_component(const render_component&) = delete;
		render_component& operator=(const render_component&) = delete;

		void add_mesh(std::unique_ptr<mesh> mesh);
		void add_bounding_box_mesh(std::unique_ptr<bounding_box_mesh> bounding_box_mesh);

		void update_bounding_box_meshes() const;
		void set_render(bool render, bool render_bounding_boxes);

		void set_mesh_positions(const vec3<length>& position) const;

		std::vector<render_queue_entry> get_queue_entries() const;
		std::vector<render_queue_entry> get_bounding_box_queue_entries() const;
		std::vector<mesh*> get_meshes() const;
		std::vector<bounding_box_mesh*> get_bounding_box_meshes() const;
	protected:
		std::vector<std::unique_ptr<mesh>> m_meshes;
		std::vector<std::unique_ptr<bounding_box_mesh>> m_bounding_box_meshes;

		bool m_render = false;
		bool m_render_bounding_boxes = false;
	};
}

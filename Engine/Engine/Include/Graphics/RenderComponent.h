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
		render_component(id* id, const std::vector<std::shared_ptr<mesh>>& meshes) : component(id), m_meshes(meshes) {}

		void add_mesh(const std::shared_ptr<mesh>& mesh);
		void add_bounding_box_mesh(const std::shared_ptr<bounding_box_mesh>& bounding_box_mesh);

		void update_bounding_box_meshes() const;
		void set_render(bool render, bool render_bounding_boxes);

		void set_mesh_positions(const vec3<length>& position) const;

		std::vector<render_queue_entry> get_queue_entries() const;
		std::vector<render_queue_entry> get_bounding_box_queue_entries() const;
		std::vector<std::shared_ptr<mesh>>& get_meshes() { return m_meshes; }
		std::vector<std::shared_ptr<bounding_box_mesh>>& get_bounding_box_meshes() { return m_bounding_box_meshes; }
	protected:
		std::vector<std::shared_ptr<mesh>> m_meshes;
		std::vector<std::shared_ptr<bounding_box_mesh>> m_bounding_box_meshes;

		bool m_render = false;
		bool m_render_bounding_boxes = false;
	};
}

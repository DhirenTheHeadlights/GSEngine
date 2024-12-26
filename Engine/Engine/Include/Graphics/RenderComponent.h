#pragma once
#include <memory>
#include <string>
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Core/EngineComponent.h"
#include "Graphics/3D/BoundingBoxMesh.h"
#include "Graphics/3D/Mesh.h"

namespace gse {
	class render_component final : public component {
	public:
		render_component(id* id) : component(id) {}
		render_component(id* id, const std::vector<std::shared_ptr<mesh>>& meshes) : component(id), m_meshes(meshes) {}
		render_component(id* id, const std::string& file_path) : component(id) { load_model(file_path); }

		void load_model(const std::string& path);
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
		
		void process_node(aiNode* node, const aiScene* scene, std::vector<model_texture>& textures_loaded);
		std::shared_ptr<mesh> process_mesh(aiMesh* mesh, const aiScene* scene, std::vector<model_texture>& textures_loaded);
		std::vector<model_texture> load_model_textures(aiMaterial* mat, aiTextureType type,
			const std::string& typeName, std::vector<model_texture>& textures_loaded);

		bool m_render = false;
		bool m_render_bounding_boxes = false;
	};
}

#include "Graphics/RenderComponent.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace {
	std::vector<gse::model_texture> load_model_textures(const aiMaterial* mat, const aiTextureType type, const std::string& typeName, std::vector<gse::model_texture>& textures_loaded) {
		std::vector<gse::model_texture> textures;
		for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
			aiString str;
			mat->GetTexture(type, i, &str);
			bool skip = false;
			for (auto& j : textures_loaded) {
				if (std::strcmp(j.path.data(), str.C_Str()) == 0) {
					textures.push_back(j);
					skip = true;
					break;
				}
			}
			if (!skip) {
				gse::model_texture texture{
					.id = 0,
					.type = typeName,
					.path = str.C_Str()
				};
				textures.push_back(texture);
				textures_loaded.push_back(texture);
			}
		}
		return textures;
	}

	gse::mesh process_mesh(const aiMesh* mesh, const aiScene* scene, std::vector<gse::model_texture>& textures_loaded) {
		std::vector<gse::vertex> vertices;
		std::vector<unsigned int> indices;
		std::vector<gse::model_texture> textures;
		for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
			auto& mesh_vertex = mesh->mVertices[i];
			auto& mesh_normal = mesh->mNormals[i];

			gse::vertex vertex{
				.position = { mesh_vertex.x, mesh_vertex.y, mesh_vertex.z },
				.normal = { mesh_normal.x, mesh_normal.y, mesh_normal.z },
				.tex_coords = mesh->mTextureCoords[0] ? glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y) : glm::vec2(0.0f)
			};

			vertices.push_back(vertex);
		}

		for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
			const aiFace face = mesh->mFaces[i];
			for (unsigned int j = 0; j < face.mNumIndices; j++) {
				indices.push_back(face.mIndices[j]);
			}
		}

		if (mesh->mMaterialIndex >= 0) {
			const aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
			std::vector<gse::model_texture> diffuse_maps = load_model_textures(material,
				aiTextureType_DIFFUSE, "texture_diffuse", textures_loaded);
			textures.insert(textures.end(), diffuse_maps.begin(), diffuse_maps.end());
			std::vector<gse::model_texture> specular_maps = load_model_textures(material,
				aiTextureType_SPECULAR, "texture_specular", textures_loaded);
			textures.insert(textures.end(), specular_maps.begin(), specular_maps.end());
		}
		return { vertices, indices, textures };
	}

	void process_node(const aiNode* node, const aiScene* scene, std::vector<gse::model_texture>& textures_loaded, std::vector<gse::mesh>& meshes) {
		for (unsigned int i = 0; i < node->mNumMeshes; i++) {
			const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			meshes.push_back(process_mesh(mesh, scene, textures_loaded));
		}

		for (unsigned int i = 0; i < node->mNumChildren; i++) {
			process_node(node->mChildren[i], scene, textures_loaded, meshes);
		}
	}
}

void gse::render_component::load_model(const std::string& path) {
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << '\n';
		return;
	}

	std::vector<model_texture> textures_loaded;
	process_node(scene->mRootNode, scene, textures_loaded, meshes);
}

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

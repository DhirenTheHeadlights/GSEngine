#include "Graphics/RenderComponent.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

void gse::render_component::load_model(const std::string& path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << '\n';
        return;
    }


    std::vector<model_texture> textures_loaded;
    std::vector<mesh> meshes;

    auto load_textures = [&](const aiMaterial* mat, const aiTextureType type, const std::string& typeName) -> std::vector<model_texture> {
        std::vector<model_texture> textures;
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
                model_texture texture{
                	.id= 0, .type= typeName, .path= str.C_Str()
                };
                textures.push_back(texture);
                textures_loaded.push_back(texture);
            }
        }
        return textures;
        };


    auto process_mesh = [&](const aiMesh* assimp_mesh) -> mesh {
        std::vector<vertex> vertices;
        std::vector<unsigned int> indices;
        std::vector<model_texture> textures;

        for (unsigned int i = 0; i < assimp_mesh->mNumVertices; i++) {
            auto& mv = assimp_mesh->mVertices[i];
            auto& mn = assimp_mesh->mNormals[i];
            vertex v{
                .position = { mv.x, mv.y, mv.z },
                .normal = { mn.x, mn.y, mn.z },
                .tex_coords = assimp_mesh->mTextureCoords[0] ? glm::vec2(assimp_mesh->mTextureCoords[0][i].x, assimp_mesh->mTextureCoords[0][i].y) : glm::vec2(0.0f)
            };
            vertices.push_back(v);
        }

        for (unsigned int i = 0; i < assimp_mesh->mNumFaces; i++) {
            const aiFace face = assimp_mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                indices.push_back(face.mIndices[j]);
            }
        }

        if (assimp_mesh->mMaterialIndex >= 0) {
            const aiMaterial* material = scene->mMaterials[assimp_mesh->mMaterialIndex];
            auto diffuse_maps = load_textures(material, aiTextureType_DIFFUSE, "texture_diffuse");
            textures.insert(textures.end(), diffuse_maps.begin(), diffuse_maps.end());
            auto specular_maps = load_textures(material, aiTextureType_SPECULAR, "texture_specular");
            textures.insert(textures.end(), specular_maps.begin(), specular_maps.end());
        }

        return gse::mesh{ vertices, indices, textures };
        };

    std::function<void(const aiNode*)> process_node = [&](const aiNode* node) {
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            const aiMesh* ai_mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(process_mesh(ai_mesh));
        }
        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            process_node(node->mChildren[i]);
        }
        };

    process_node(scene->mRootNode);
    this->meshes = std::move(meshes);
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

void gse::render_component::set_all_mesh_material_strings(const std::string& material_string) {
	for (auto& mesh : meshes) {
		mesh.m_material_name = material_string;
	}
}
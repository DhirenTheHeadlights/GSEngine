#include "Graphics/RenderComponent.h"

void gse::render_component::add_mesh(const std::shared_ptr<mesh>& mesh) {
	mesh->set_up_mesh();
	m_meshes.push_back(mesh);
}

void gse::render_component::add_bounding_box_mesh(const std::shared_ptr<bounding_box_mesh>& bounding_box_mesh) {
	bounding_box_mesh->set_up_mesh();
	m_bounding_box_meshes.push_back(bounding_box_mesh);
}

void gse::render_component::update_bounding_box_meshes() const {
	for (const auto& bounding_box_mesh : m_bounding_box_meshes) {
		bounding_box_mesh->update();
	}
}

std::vector<gse::render_queue_entry> gse::render_component::get_queue_entries() const {
	if (!m_render) return {};

	std::vector<render_queue_entry> entries;
	entries.reserve(m_meshes.size() + m_bounding_box_meshes.size());
	for (const auto& mesh : m_meshes) {
		entries.push_back(mesh->get_queue_entry());
	}

	return entries;
}

std::vector<gse::render_queue_entry> gse::render_component::get_bounding_box_queue_entries() const {
	if (!m_render_bounding_boxes) return {};

	std::vector<render_queue_entry> entries;
	entries.reserve(m_bounding_box_meshes.size());
	for (const auto& bounding_box_mesh : m_bounding_box_meshes) {
		entries.push_back(bounding_box_mesh->get_queue_entry());
	}

	return entries;
}

void gse::render_component::set_render(const bool render, const bool render_bounding_boxes) {
	this->m_render = render;
	this->m_render_bounding_boxes = render_bounding_boxes;
}

void gse::render_component::set_mesh_positions(const vec3<length>& position) const {
	for (const auto& mesh : m_meshes) {
		mesh->set_position(position);
	}
}


std::vector<gse::model_texture> gse::render_component::load_model_textures(aiMaterial* mat, aiTextureType type, const std::string& typeName, std::vector<model_texture>& textures_loaded) {
	std::vector<model_texture> textures;
	for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
		aiString str;
		mat->GetTexture(type, i, &str);
		bool skip = false;
		for (unsigned int j = 0; j < textures_loaded.size(); j++) {
			if (std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0) {
				textures.push_back(textures_loaded[j]);
				skip = true;
				break;
			}
		}
		if (!skip) {
			model_texture texture{
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

std::shared_ptr<gse::mesh> gse::render_component::process_mesh(aiMesh* mesh, const aiScene* scene, std::vector<model_texture>& textures_loaded) {
	std::vector<vertex> vertices;
	std::vector<unsigned int> indices;
	std::vector<gse::model_texture> textures;

	for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
		vertex vertex;
		glm::vec3 vector;
		vector.x = mesh->mVertices[i].x;
		vector.y = mesh->mVertices[i].y;
		vector.z = mesh->mVertices[i].z;
		vertex.position = vector;

		vector.x = mesh->mNormals[i].x;
		vector.y = mesh->mNormals[i].y;
		vector.z = mesh->mNormals[i].z;
		vertex.normal = vector;

		if (mesh->mTextureCoords[0]) { // Does the mesh contain texture coordinates or not
			glm::vec2 vec;
			vec.x = mesh->mTextureCoords[0][i].x;
			vec.y = mesh->mTextureCoords[0][i].y;
			vertex.tex_coords = vec;
		}
		else
			vertex.tex_coords = glm::vec2(0.0f, 0.0f);
		vertices.push_back(vertex);
	}

	for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++) {
			indices.push_back(face.mIndices[j]);
		}
	}

	if (mesh->mMaterialIndex >= 0) {
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		std::vector<gse::model_texture> diffuse_maps = load_model_textures(material,
			aiTextureType_DIFFUSE, "texture_diffuse", textures_loaded);
		textures.insert(textures.end(), diffuse_maps.begin(), diffuse_maps.end());
		std::vector<gse::model_texture> specular_maps = load_model_textures(material,
			aiTextureType_SPECULAR, "texture_specular", textures_loaded);
		textures.insert(textures.end(), specular_maps.begin(), specular_maps.end());
	}

	return std::make_shared<gse::mesh>(vertices, indices, textures);
}

void gse::render_component::process_node(aiNode* node, const aiScene* scene, std::vector<model_texture>& textures_loaded) {
	for (unsigned int i = 0; i < node->mNumMeshes; i++) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		m_meshes.push_back(process_mesh(mesh, scene, textures_loaded));
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		process_node(node->mChildren[i], scene, textures_loaded);
	}
}

void gse::render_component::load_model(const std::string& path) {
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);
	std::vector<model_texture> textures_loaded;
	
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
		return;
	}

	process_node(scene->mRootNode, scene, textures_loaded);
}

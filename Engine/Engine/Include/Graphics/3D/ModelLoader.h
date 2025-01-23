#pragma once
#include "Graphics/3D/Mesh.h"
#include "Core/ID.h"

namespace gse {

	struct model : identifiable {
		model(const std::string& tag) : identifiable(tag) {}
		std::vector<mesh> meshes;

		void set_mesh_positions(const vec3<length>& new_position) {
			for (auto& mesh : meshes) {
				mesh.set_position(new_position);
			}
		}

	};

}



namespace gse::model_loader {
	auto load_obj_file(const std::string& model_path, const std::string& model_name) -> gse::id*;
	auto add_model(std::vector<mesh>&& meshes, const std::string& model_name) -> gse::id*;
	auto get_model_by_name(const std::string& model_path) -> gse::model&;
	auto get_model_by_id(gse::id* model_id) -> gse::model&;
	auto get_models() -> const std::unordered_map<gse::id*, gse::model>&;
	auto set_model_position(gse::id* model_id, const vec3<length>& new_position) -> void;
}
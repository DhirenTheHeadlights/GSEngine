#pragma once
#include "Graphics/3D/Mesh.h"
#include "Core/ID.h"

namespace gse {

	struct model {
		std::vector<mesh> meshes;
	};

}



namespace gse::model_loader {
	auto load_obj_file(const std::string& model_path, const std::string& model_name) -> gse::id*;
	auto add_model(const std::vector<mesh>& meshes, const std::string& model_name) -> gse::id*;
	auto get_model_by_name(const std::string& model_path) -> const gse::model&;
}
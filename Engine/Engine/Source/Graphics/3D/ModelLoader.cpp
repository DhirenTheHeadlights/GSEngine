#include "Graphics/3D/ModelLoader.h"


namespace {

	std::unordered_map<gse::id*, gse::model> models;
	std::unordered_map<gse::id*, std::string> loaded_model_paths;

}

	auto gse::model_loader::load_obj_file(const std::string& model_path, const std::string& model_name) -> gse::id* {}

	auto gse::model_loader::get_model_by_name(const std::string& model_name) -> const gse::model& { 
		return models[gse::get_id(model_name)];
	}

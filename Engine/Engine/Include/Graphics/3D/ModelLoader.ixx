export module gse.graphics.model_loader;

import std;

import gse.core.id;
import gse.graphics.model;

namespace gse::model_loader {
	auto load_obj_file(const std::string& model_path, const std::string& model_name) -> id*;
	auto add_model(std::vector<mesh>&& meshes, const std::string& model_name) -> id*;
	auto get_model_by_name(const std::string& model_name) -> const model&;
	auto get_model_by_id(id* model_id) -> const model&;
	auto get_models() -> const std::unordered_map<id*, model>&;
}
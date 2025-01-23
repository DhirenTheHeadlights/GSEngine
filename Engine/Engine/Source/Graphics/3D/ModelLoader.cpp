#include "Graphics/3D/ModelLoader.h"
#include <iostream>
#include <fstream>
#include <ranges>
#include <string>



namespace {

	std::unordered_map<gse::id*, gse::model> models;
	std::unordered_map<gse::id*, std::string> loaded_model_paths;

}

auto gse::model_loader::load_obj_file(const std::string& model_path, const std::string& model_name) -> gse::id* {
	std::ifstream model_file(model_path);
	if (!model_file.is_open()) {
		std::cerr << "Failed to open model file: " << model_path << std::endl;
		return nullptr;
	}

	//Check if the model has been loaded already; if so, return the ID
	for (const auto& [id, path] : loaded_model_paths) {
		if (path == model_path) {
			return id;
		}
	}
	//Reserve an ID and space for the model
	gse::model model(model_name);


	//Helper function to split a string by a delimiter
	auto split = [](const std::string& str, char delimiter = ' ') -> std::vector<std::string> {
		std::vector<std::string> tokens;
		std::istringstream stream(str);
		std::string token;

		while (std::getline(stream, token, delimiter)) {
			tokens.push_back(token);
		}
		return tokens;
		};

	std::string file_line;
	bool push_back_mesh = false;

	std::vector<glm::vec3> pre_load_vertices;
	std::vector<glm::vec2> pre_load_texcoords;
	std::vector<glm::vec3> pre_load_normals;
	std::vector<gse::vertex> final_vertices;

	while (std::getline(model_file, file_line)) {
		std::vector<std::string> split_line = split(file_line, ' ');
		if (file_line.substr(0, 2) == "v ") {
			if (push_back_mesh) {
				std::vector<unsigned int> final_indices(final_vertices.size());
				for (int i = 0; i < final_vertices.size(); i++) final_indices[i] = i;
				model.meshes.push_back(std::move(gse::mesh(final_vertices, final_indices)));

				//Clear data to load next mesh
				push_back_mesh = false;
				final_vertices.clear();
			}

			pre_load_vertices.emplace_back(std::stof(split_line[1]), std::stof(split_line[2]), std::stof(split_line[3]));
		}
		else if (file_line.substr(0, 3) == "vt ") {
			pre_load_texcoords.emplace_back(std::stof(split_line[1]), std::stof(split_line[2]));
		}
		else if (file_line.substr(0, 3) == "vn ") {
			pre_load_normals.emplace_back(std::stof(split_line[1]), std::stof(split_line[2]), std::stof(split_line[3]));
		}
		else if (file_line.substr(0, 2) == "f ") {
			push_back_mesh = true;
			for (int i = 1; i <= 3; i++) {
				std::vector<std::string> vertex_map = split(split_line[i], '/');
				if (vertex_map.size() == 3) {
					final_vertices.emplace_back(pre_load_vertices[std::stoi(vertex_map[0]) - 1],
						pre_load_normals[std::stoi(vertex_map[2]) - 1],
						pre_load_texcoords[std::stoi(vertex_map[1]) - 1]);
				}
				else {
					if (!pre_load_normals.empty()) {
						final_vertices.emplace_back(pre_load_vertices[std::stoi(vertex_map[0]) - 1],
							pre_load_normals[std::stoi(vertex_map[1]) - 1],
							glm::vec2(0.f));
					}
					else if (!pre_load_texcoords.empty()) {
						final_vertices.emplace_back(pre_load_vertices[std::stoi(vertex_map[0]) - 1],
							glm::vec3(0.0f),
							pre_load_texcoords[std::stoi(vertex_map[1]) - 1]);
					}

				}
			}
		}
	}
	pre_load_vertices.clear();
	pre_load_texcoords.clear();
	pre_load_normals.clear();

	gse::id* id_pointer = model.get_id();
	loaded_model_paths.insert({ id_pointer, model_path });
	models.insert({ id_pointer, std::move(model) });

	return id_pointer;
}

	auto gse::model_loader::add_model(std::vector<gse::mesh>&& meshes, const std::string& model_name) -> gse::id* {
		gse::model model(model_name);
		model.meshes = std::move(meshes);
		gse::id* id_pointer = model.get_id();
		models.insert({ model.get_id(), std::move(model)});
		return id_pointer;
	}

	auto gse::model_loader::get_model_by_name(const std::string& model_name) -> gse::model& { 
		return std::ranges::find_if(models, [&model_name](const auto& model) { return model.first->tag() == model_name; })->second;
	}

	auto gse::model_loader::get_model_by_id(gse::id* model_id) -> gse::model& {
		return models.at(model_id);
	}

	auto gse::model_loader::get_models() -> const std::unordered_map<gse::id*, gse::model>& {
		return models;
	}

	auto gse::model_loader::set_model_position(gse::id* model_id, const gse::vec3<gse::length>& new_position) -> void {
		auto& model = models.at(model_id);
		for (auto& mesh : model.meshes) {
			mesh.set_position(new_position);
		}
	}

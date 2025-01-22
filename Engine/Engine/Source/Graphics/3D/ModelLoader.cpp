#include "Graphics/3D/ModelLoader.h"
#include <iostream>
#include <fstream>
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
		//Reserve an ID and space for the model
		gse::model model;
		

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
					for(int i = 0; i < final_vertices.size(); i++) final_indices[i] = i;
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
					final_vertices.emplace_back(pre_load_vertices[std::stoi(vertex_map[0]) - 1],
												pre_load_normals[std::stoi(vertex_map[2]) - 1],
												pre_load_texcoords[std::stoi(vertex_map[1]) -1]);

				}	
				

			}
		}
		auto id_pointer = gse::generate_id(model_name);
		loaded_model_paths[id_pointer.get()] = model_path;
		models[id_pointer.get()] = std::move(model);
		pre_load_vertices.clear();
		pre_load_texcoords.clear();
		pre_load_normals.clear();
		return id_pointer.get();
	}

	auto gse::model_loader::add_model(const std::vector<gse::mesh>& meshes, const std::string& model_name) -> gse::id* {
		auto id_pointer = gse::generate_id(model_name);
		models[id_pointer.get()] = gse::model{ std::move(meshes) };
		return id_pointer.get();
	}

	auto gse::model_loader::get_model_by_name(const std::string& model_name) -> const gse::model& { 
		return models[gse::get_id(model_name)];
	}


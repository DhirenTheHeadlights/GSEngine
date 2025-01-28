export module gse.graphics.model_loader;

import std;
import glm;

import gse.core.id;
import gse.graphics.model;
import gse.graphics.mesh;

export namespace gse::model_loader {
	auto load_obj_file(const std::string& model_path, const std::string& model_name) -> id*;
	auto add_model(std::vector<mesh>&& meshes, const std::string& model_name) -> id*;
	auto get_model_by_name(const std::string& model_name) -> const model&;
	auto get_model_by_id(id* model_id) -> const model&;
	auto get_models() -> const std::unordered_map<id*, model>&;
}

std::unordered_map<gse::id*, gse::model> g_models;
std::unordered_map<gse::id*, std::string> g_loaded_model_paths;

auto gse::model_loader::load_obj_file(const std::string& model_path, const std::string& model_name) -> id* {
	std::ifstream model_file(model_path);
	if (!model_file.is_open()) {
		std::cerr << "Failed to open model file: " << model_path << '\n';
		return nullptr;
	}

	for (const auto& [id, path] : g_loaded_model_paths) {
		if (path == model_path) {
			return id; // Already loaded
		}
	}

	model model(model_name);

	auto split = [](const std::string& str, const char delimiter = ' ') -> std::vector<std::string> {
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
	std::vector<vertex> final_vertices;

	while (std::getline(model_file, file_line)) {
		std::vector<std::string> split_line = split(file_line, ' ');
		if (file_line.substr(0, 2) == "v ") {
			if (push_back_mesh) {
				std::vector<std::uint32_t> final_indices(final_vertices.size());
				for (size_t i = 0; i < final_vertices.size(); i++) final_indices[i] = i;
				model.meshes.emplace_back(final_vertices, final_indices);

				// Clear data to load next mesh
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
						pre_load_normals[static_cast<size_t>(std::stoi(vertex_map[2])) - 1],
						pre_load_texcoords[static_cast<size_t>(std::stoi(vertex_map[1])) - 1]);
				}
				else {
					if (!pre_load_normals.empty()) {
						final_vertices.emplace_back(pre_load_vertices[std::stoi(vertex_map[0]) - 1],
							pre_load_normals[static_cast<size_t>(std::stoi(vertex_map[1]) - 1)],
							glm::vec2(0.f));
					}
					else if (!pre_load_texcoords.empty()) {
						final_vertices.emplace_back(pre_load_vertices[std::stoi(vertex_map[0]) - 1],
							glm::vec3(0.0f),
							pre_load_texcoords[static_cast<size_t>(std::stoi(vertex_map[1])) - 1]);
					}
				}
			}
		}
	}

	pre_load_vertices.clear();
	pre_load_texcoords.clear();
	pre_load_normals.clear();

	model.initialize();

	id* id_pointer = model.get_id();
	g_loaded_model_paths.insert({ id_pointer, model_path });
	g_models.insert({ id_pointer, std::move(model) });

	return id_pointer;
}

auto gse::model_loader::add_model(std::vector<mesh>&& meshes, const std::string& model_name) -> id* {
	model model(model_name);
	model.meshes = std::move(meshes);
	model.initialize();
	id* id_pointer = model.get_id();
	g_models.insert({ id_pointer, std::move(model) });
	return id_pointer;
}

auto gse::model_loader::get_model_by_name(const std::string& model_name) -> const model& {
	return std::ranges::find_if(g_models, [&model_name](const auto& model) { return model.first->tag() == model_name; })->second;
}

auto gse::model_loader::get_model_by_id(id* model_id) -> const model& {
	return g_models.at(model_id);
}

auto gse::model_loader::get_models() -> const std::unordered_map<id*, model>& {
	return g_models;
}
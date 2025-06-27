export module gse.graphics:model_loader;

import std;

import :model;
import :mesh;
import :texture;
import :texture_loader;
import :material;

import gse.physics.math;
import gse.utility;
import gse.platform;

export namespace gse::model_loader {
	auto load_obj_file(const std::filesystem::path& model_path, const std::string& model_name) -> model_handle;
	auto add_model(const std::vector<mesh_data>& mesh_data, const std::string& model_name) -> model_handle;
	auto model(const model_handle& handle) -> const model&;
	auto model(const id& id) -> const class model&;

	auto load_queued_models(const vulkan::config& config) -> void;
}

export template<>
struct std::hash<gse::model_handle> {
	auto operator()(const gse::model_handle& handle) const noexcept -> std::size_t {
		return std::hash<gse::uuid>{}(handle.model_id().number());
	}
};

std::unordered_map<gse::model_handle, std::unique_ptr<gse::model>> g_models;
std::unordered_map<gse::model_handle, std::filesystem::path> g_loaded_model_paths;
std::vector<gse::model_handle> g_models_to_initialize;

auto gse::model_loader::load_obj_file(const std::filesystem::path& model_path, const std::string& model_name) -> model_handle {
	std::ifstream model_file(model_path);
	assert(model_file.is_open(), "Failed to open model file.");

	for (const auto& [handle, path] : g_loaded_model_paths) {
		if (path == model_path) {
			return handle;
		}
	}

	auto model_ptr = std::make_unique<class model>(model_name);

	auto split = [](const std::string& str, const char delimiter = ' ') -> std::vector<std::string> {
		std::vector<std::string> tokens;
		const size_t length = str.length();
		size_t i = 0;
		while (i < length) {
			while (i < length && str[i] == delimiter) {
				++i;
			}
			const size_t start = i;
			while (i < length && str[i] != delimiter) {
				++i;
			}
			if (start < i) {
				tokens.emplace_back(str.substr(start, i - start));
			}
		}
		return tokens;
		};

	std::string file_line;
	bool push_back_mesh = false;

	std::vector<raw3f> pre_load_vertices;
	std::vector<raw2f> pre_load_texcoords;
	std::vector<raw3f> pre_load_normals;
	std::vector<vertex> final_vertices;
	std::vector<std::uint32_t> loaded_texture_ids;
	std::string current_material = "";

	while (std::getline(model_file, file_line)) {
		std::vector<std::string> split_line = split(file_line, ' ');
		if (file_line.substr(0, 2) == "v ") {
			if (push_back_mesh) {
				std::vector<std::uint32_t> final_indices(final_vertices.size());
				for (size_t i = 0; i < final_vertices.size(); i++) final_indices[i] = i;
				if (current_material.empty()) {
					model_ptr->meshes.emplace_back(final_vertices, final_indices);
				}
				else {
					model_ptr->meshes.emplace_back(final_vertices, final_indices, get_material(current_material));
				}

				push_back_mesh = false;
				final_vertices.clear();
			}

			pre_load_vertices.push_back({ std::stof(split_line[1]), std::stof(split_line[2]), std::stof(split_line[3]) });
		}
		else if (file_line.substr(0, 3) == "vt ") {
			pre_load_texcoords.push_back({ std::stof(split_line[1]), std::stof(split_line[2]) });
		}
		else if (file_line.substr(0, 3) == "vn ") {
			pre_load_normals.push_back({ std::stof(split_line[1]), std::stof(split_line[2]), std::stof(split_line[3]) });
		}
		else if (file_line.substr(0, 2) == "f ") {
			push_back_mesh = true;
			if (split_line.size() - 1 == 3) {
				for (int i = 1; i <= 3; i++) {
					if (std::vector<std::string> vertex_map = split(split_line[i], '/'); vertex_map.size() == 3) {
						final_vertices.emplace_back(pre_load_vertices[std::stoi(vertex_map[0]) - 1],
							pre_load_normals[static_cast<size_t>(std::stoi(vertex_map[2])) - 1],
							pre_load_texcoords[static_cast<size_t>(std::stoi(vertex_map[1])) - 1]);
					}
					else {
						if (!pre_load_normals.empty()) {
							final_vertices.emplace_back(pre_load_vertices[std::stoi(vertex_map[0]) - 1],
								pre_load_normals[static_cast<size_t>(std::stoi(vertex_map[1])) - 1],
								raw2f());
						}
						else if (!pre_load_texcoords.empty()) {
							final_vertices.emplace_back(pre_load_vertices[std::stoi(vertex_map[0]) - 1],
								raw3f(),
								pre_load_texcoords[static_cast<size_t>(std::stoi(vertex_map[1])) - 1]);
						}
					}
				}
			}
			else if (split_line.size() - 1 == 4) {
				std::vector triangle_1 = { split_line[1], split_line[2], split_line[3] };
				std::vector triangle_2 = { split_line[1], split_line[3], split_line[4] };
				for (auto& triangle : { triangle_1, triangle_2 }) {
					for (int i = 0; i < 3; i++) {
						if (std::vector<std::string> vertex_map = split(triangle[i], '/'); vertex_map.size() == 3) {
							final_vertices.emplace_back(pre_load_vertices[std::stoi(vertex_map[0]) - 1],
								pre_load_normals[static_cast<size_t>(std::stoi(vertex_map[2])) - 1],
								pre_load_texcoords[static_cast<size_t>(std::stoi(vertex_map[1])) - 1]);
						}
						else {
							if (!pre_load_normals.empty()) {
								final_vertices.emplace_back(
									pre_load_vertices[std::stoi(vertex_map[0]) - 1],
									pre_load_normals[static_cast<size_t>(std::stoi(vertex_map[1])) - 1],
									raw2f()
								);
							}
							else if (!pre_load_texcoords.empty()) {
								final_vertices.emplace_back(
									pre_load_vertices[std::stoi(vertex_map[0]) - 1],
									raw3f(),
									pre_load_texcoords[static_cast<size_t>(std::stoi(vertex_map[1])) - 1]
								);
							}
						}
					}
				}

			}
		}
		else if (file_line.substr(0, 7) == "mtllib ") {
			std::string mtl_filename = split_line[1];
			std::string mtl_path = model_path.string().substr(0, model_path.string().find_last_of('/')) + "/" + mtl_filename;
			std::string directory_path = mtl_path.substr(0, mtl_path.find_last_of('/')) + "/";
			std::ifstream mtl_file(mtl_path);
			if (!mtl_file.is_open()) {
				std::cerr << "Failed to open material file: " << mtl_path << '\n';
			}

			std::string line;
			while (std::getline(mtl_file, line)) {
				auto tokens = split(line, ' ');
				if (tokens.empty()) continue;

				if (tokens[0] == "newmtl") {
					current_material = tokens[1];
					generate_material({}, {}, {}, current_material);
				}
				else if (!current_material.empty()) {
					auto& [ambient, diffuse, specular, emission, shininess, optical_density, transparency, illumination_model, diffuse_texture, normal_texture, specular_texture] = *get_material(current_material).data();

					if (tokens[0] == "Ns") shininess = std::stof(tokens[1]);
					else if (tokens[0] == "Ka") ambient = unitless::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
					else if (tokens[0] == "Kd") diffuse = unitless::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
					else if (tokens[0] == "Ks") specular = unitless::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
					else if (tokens[0] == "Ke") emission = unitless::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
					else if (tokens[0] == "Ni") optical_density = std::stof(tokens[1]);
					else if (tokens[0] == "d") transparency = std::stof(tokens[1]);
					else if (tokens[0] == "illum") illumination_model = std::stoi(tokens[1]);
					else if (tokens[0] == "map_Kd") diffuse_texture = texture_loader::get_texture(directory_path + tokens[1]).id();
					else if (tokens[0] == "map_Bump") normal_texture = texture_loader::get_texture(directory_path + tokens[1]).id();
					else if (tokens[0] == "map_Ks") specular_texture = texture_loader::get_texture(directory_path + tokens[1]).id();
				}
			}
		}
		else if (file_line.substr(0, 7) == "usemtl ") {
			current_material = split_line[1];

			if (!does_material_exist(current_material)) {
				std::cerr << "Warning: Material '" << current_material << "' not found in g_materials.\n";
			}
		}
	}

	if (push_back_mesh) {
		std::vector<std::uint32_t> final_indices(final_vertices.size());
		for (size_t i = 0; i < final_vertices.size(); i++) final_indices[i] = i;
		if (current_material.empty()) {
			model_ptr->meshes.emplace_back(final_vertices, final_indices, nullptr);
		}
		else {
			model_ptr->meshes.emplace_back(final_vertices, final_indices, get_material(current_material));
		}
	}

	model_handle handle(*model_ptr);
	g_models_to_initialize.push_back(handle);
	g_loaded_model_paths.emplace(handle, model_path);
	g_models.emplace(handle, std::move(model_ptr));

	return handle;
}

auto gse::model_loader::add_model(const std::vector<mesh_data>& mesh_data, const std::string& model_name) -> model_handle {
	for (const auto& [handle, path] : g_loaded_model_paths) {
		if (path == model_name) {
			return handle;
		}
	}

	auto model_ptr = std::make_unique<class model>(model_name);

	for (const auto& d : mesh_data) {
		model_ptr->meshes.emplace_back(d);
	}

	model_handle handle(*model_ptr);
	g_models_to_initialize.push_back(handle);
	g_loaded_model_paths.emplace(handle, model_name);
	g_models.emplace(handle, std::move(model_ptr));

	return handle;
}

auto gse::model_loader::model(const model_handle& handle) -> const class model& {
	const auto it = g_models.find(handle);
	assert(it != g_models.end(), "Model not found.");
	return *it->second;
}

auto gse::model_loader::model(const id& id) -> const class model& {
	const auto it = std::ranges::find_if(g_models, [&id](const auto& pair) {
		return pair.first.model_id() == id;
		});

	assert(it != g_models.end(), "Model not found.");
	return *it->second;
}

auto gse::model_loader::load_queued_models(const vulkan::config& config) -> void {
	for (const auto& handle : g_models_to_initialize) {
		if (auto it = g_models.find(handle); it != g_models.end()) {
			it->second->initialize(config);
		}
	}
	g_models_to_initialize.clear();
}
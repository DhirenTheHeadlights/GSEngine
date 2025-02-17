export module gse.graphics.model_loader;

import std;

import gse.core.id;
import gse.graphics.model;
import gse.graphics.mesh;
import gse.graphics.texture_loader;
import gse.graphics.material;

export namespace gse::model_loader {
	auto load_obj_file(const std::filesystem::path& model_path, const std::string& model_name) -> id*;
	auto add_model(std::vector<mesh>&& meshes, const std::string& model_name) -> id*;
	auto get_model_by_name(const std::string& model_name) -> const model&;
	auto get_model_by_id(id* model_id) -> const model&;
	auto get_models() -> const std::unordered_map<id*, model>&;
}

std::unordered_map<gse::id*, gse::model> g_models;
std::unordered_map<gse::id*, std::filesystem::path> g_loaded_model_paths;
std::unordered_map<std::string, gse::mtl_material> g_materials;

auto gse::model_loader::load_obj_file(const std::filesystem::path& model_path, const std::string& model_name) -> id* {
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
		size_t length = str.length();
		size_t i = 0;
		while (i < length) {
			// Skip multiple delimiters
			while (i < length && str[i] == delimiter) {
				++i;
			}
			// Start of a token
			size_t start = i;
			while (i < length && str[i] != delimiter) {
				++i;
			}
			// Add non-empty tokens to the list
			if (start < i) {
				tokens.emplace_back(str.substr(start, i - start));
			}
		}
		return tokens;
		};

	std::string file_line;
	bool push_back_mesh = false;

	std::vector<vec::raw3f> pre_load_vertices;
	std::vector<vec::raw2f> pre_load_texcoords;
	std::vector<vec::raw3f> pre_load_normals;
	std::vector<vertex> final_vertices;
	std::vector<std::uint32_t> loaded_texture_ids;
	std::string current_material = "";

	while (std::getline(model_file, file_line)) {
		std::vector<std::string> split_line = split(file_line, ' ');
		// Detect vertices for each mesh. Load and output mesh if push_back_mesh is true.
		if (file_line.substr(0, 2) == "v ") {
			if (push_back_mesh) {
				std::vector<std::uint32_t> final_indices(final_vertices.size());
				for (size_t i = 0; i < final_vertices.size(); i++) final_indices[i] = i;
				if (current_material.empty()) {
					model.meshes.emplace_back(final_vertices, final_indices);
				}
				else {
					model.meshes.emplace_back(final_vertices, final_indices, &g_materials[current_material]);
				}

				// Clear data to load next mesh
				push_back_mesh = false;
				final_vertices.clear();
			}

			pre_load_vertices.push_back({ std::stof(split_line[1]), std::stof(split_line[2]), std::stof(split_line[3]) });
		}
		// Detect texcoords for each vertex
		else if (file_line.substr(0, 3) == "vt ") {
			pre_load_texcoords.push_back({ std::stof(split_line[1]), std::stof(split_line[2]) });
		}
		// Detect normals for each vertex
		else if (file_line.substr(0, 3) == "vn ") {
			pre_load_normals.push_back({ std::stof(split_line[1]), std::stof(split_line[2]), std::stof(split_line[3]) });
		}
		// Detect maps for each mesh to corresponding vertices, texcoords, material, etc. This marks the end of a mesh, so set push_back_mesh to be true.
		else if (file_line.substr(0, 2) == "f ") {
			push_back_mesh = true;
			// Handle meshes that are provided as triangles
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
								pre_load_normals[static_cast<size_t>(std::stoi(vertex_map[1]) - 1)],
								vec::raw2f());
						}
						else if (!pre_load_texcoords.empty()) {
							final_vertices.emplace_back(pre_load_vertices[std::stoi(vertex_map[0]) - 1],
								vec::raw3f(),
								pre_load_texcoords[static_cast<size_t>(std::stoi(vertex_map[1])) - 1]);
						}
					}
				}
			}
			// Handle meshes that are provided as quads. This requires each to be converted to two triangles
			else if (split_line.size() - 1 == 4) {
				std::vector<std::string> triangle_1 = { split_line[1], split_line[2], split_line[3] };
				std::vector<std::string> triangle_2 = { split_line[1], split_line[3], split_line[4] };
				for (auto& triangle : { triangle_1, triangle_2 }) {
					for (int i = 0; i < 3; i++) {
						if (std::vector<std::string> vertex_map = split(triangle[i], '/'); vertex_map.size() == 3) {
							final_vertices.emplace_back(pre_load_vertices[std::stoi(vertex_map[0]) - 1],
								pre_load_normals[static_cast<size_t>(std::stoi(vertex_map[2])) - 1],
								pre_load_texcoords[static_cast<size_t>(std::stoi(vertex_map[1])) - 1]);
						}
						else {
							if (!pre_load_normals.empty()) {
								final_vertices.emplace_back(pre_load_vertices[std::stoi(vertex_map[0]) - 1],
									pre_load_normals[static_cast<size_t>(std::stoi(vertex_map[1]) - 1)],
									vec::raw2f());
							}
							else if (!pre_load_texcoords.empty()) {
								final_vertices.emplace_back(pre_load_vertices[std::stoi(vertex_map[0]) - 1],
									vec::raw3f(),
									pre_load_texcoords[static_cast<size_t>(std::stoi(vertex_map[1])) - 1]);
							}
						}
					}
				}

			}
		}
		// Detect MTL file and load material data
		else if (file_line.substr(0, 7) == "mtllib ") {
			std::string mtl_filename = split_line[1];
			std::string mtl_path = model_path.string().substr(0, model_path.string().find_last_of("/")) + "/" + mtl_filename;
			std::string directory_path = mtl_path.substr(0, mtl_path.find_last_of("/")) + "/";
			std::ifstream mtl_file(mtl_path);
			if (!mtl_file.is_open()) {
				std::cerr << "Failed to open material file: " << mtl_path << '\n';
			}

			std::string line;
			// Load mtl data to g_materials
			while (std::getline(mtl_file, line)) {
				auto tokens = split(line, ' ');
				if (tokens.empty()) continue;

				if (tokens[0] == "newmtl") {
					current_material = tokens[1];
					g_materials[current_material] = mtl_material();
				}
				else if (!current_material.empty()) {
					auto& [ambient, diffuse, specular, emission, shininess, optical_density, transparency, illumination_model, diffuse_texture, normal_texture, specular_texture] = g_materials[current_material];

					if (tokens[0] == "Ns") shininess = std::stof(tokens[1]);
					else if (tokens[0] == "Ka") ambient = unitless::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
					else if (tokens[0] == "Kd") diffuse = unitless::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
					else if (tokens[0] == "Ks") specular = unitless::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
					else if (tokens[0] == "Ke") emission = unitless::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
					else if (tokens[0] == "Ni") optical_density = std::stof(tokens[1]);
					else if (tokens[0] == "d") transparency = std::stof(tokens[1]);
					else if (tokens[0] == "illum") illumination_model = std::stoi(tokens[1]);
					else if (tokens[0] == "map_Kd") diffuse_texture = texture_loader::load_texture(directory_path + tokens[1], false);
					else if (tokens[0] == "map_Bump") normal_texture = texture_loader::load_texture(directory_path + tokens[1], false);
					else if (tokens[0] == "map_Ks") specular_texture = texture_loader::load_texture(directory_path + tokens[1], false);
				}
			}
		}
		// Detect material for each mesh
		else if (file_line.substr(0, 7) == "usemtl ") {
			current_material = split_line[1];

			// Check if the material exists in the map
			if (g_materials.find(current_material) == g_materials.end()) {
				std::cerr << "Warning: Material '" << current_material << "' not found in g_materials.\n";
			}
		}
	}
	// Load final mesh or in case model is single mesh
	if (push_back_mesh) {
		std::vector<std::uint32_t> final_indices(final_vertices.size());
		for (size_t i = 0; i < final_vertices.size(); i++) final_indices[i] = i;
		if (current_material.empty()) {
			model.meshes.emplace_back(final_vertices, final_indices);
		}
		else {
			model.meshes.emplace_back(final_vertices, final_indices, &g_materials[current_material]);
		}

		// Clear data to load next mesh
		push_back_mesh = false;
		final_vertices.clear();
	}

	model.initialize();

	pre_load_vertices.clear();
	pre_load_texcoords.clear();
	pre_load_normals.clear();

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
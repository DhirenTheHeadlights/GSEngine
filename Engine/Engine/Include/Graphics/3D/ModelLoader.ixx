export module gse.graphics.model_loader;

import std;

import gse.core.id;
import gse.graphics.model;
import gse.graphics.mesh;
import gse.graphics.texture;
import gse.graphics.texture_loader;
import gse.graphics.material;
import gse.platform;

export namespace gse::model_loader {
	auto load_obj_file(const std::filesystem::path& model_path, const std::string& model_name) -> model_handle;
	auto add_model(const std::vector<mesh_data>& mesh_data, const std::string& model_name) -> model_handle;
	auto get_model(const model_handle& handle) -> const model&;
	auto get_model(const id& id) -> const model&;

	auto load_queued_models(const vulkan::config& config) -> void;
}

export template<>
struct std::hash<gse::model_handle> {
	auto operator()(const gse::model_handle& handle) const noexcept -> std::size_t {
		return std::hash<gse::uuid>{}(handle.get_model_id().number());
	}
};

std::unordered_map<gse::model_handle, gse::model> g_models;
std::unordered_map<gse::model_handle, std::filesystem::path> g_loaded_model_paths;
std::vector<std::pair<gse::model, std::filesystem::path>> g_queued_models;

auto gse::model_loader::load_obj_file(const std::filesystem::path& model_path, const std::string& model_name) ->
	model_handle {
	std::ifstream model_file(model_path);
	assert(model_file.is_open(), "Failed to open model file.");

	for (const auto& [handle, path] : g_loaded_model_paths) {
		if (path == model_path) {
			return handle; // Already loaded
		}
	}

	model model(model_name);

	auto split = [](const std::string& str, const char delimiter = ' ') -> std::vector<std::string> {
		std::vector<std::string> tokens;
		const size_t length = str.length();
		size_t i = 0;
		while (i < length) {
			// Skip multiple delimiters
			while (i < length && str[i] == delimiter) {
				++i;
			}
			// Start of a token
			const size_t start = i;
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
					model.meshes.emplace_back(final_vertices, final_indices, get_material(current_material));
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
					generate_material({}, {}, {}, current_material);
				}
				else if (!current_material.empty()) {
					auto& [ambient, diffuse, specular, emission, shininess, optical_density, transparency, illumination_model, diffuse_texture, normal_texture, specular_texture] = *get_material(current_material).get_data();

					if (tokens[0] == "Ns") shininess = std::stof(tokens[1]);
					else if (tokens[0] == "Ka") ambient = unitless::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
					else if (tokens[0] == "Kd") diffuse = unitless::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
					else if (tokens[0] == "Ks") specular = unitless::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
					else if (tokens[0] == "Ke") emission = unitless::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
					else if (tokens[0] == "Ni") optical_density = std::stof(tokens[1]);
					else if (tokens[0] == "d") transparency = std::stof(tokens[1]);
					else if (tokens[0] == "illum") illumination_model = std::stoi(tokens[1]);
					else if (tokens[0] == "map_Kd") diffuse_texture = texture_loader::get_texture(directory_path + tokens[1]).get_id();
					else if (tokens[0] == "map_Bump") normal_texture = texture_loader::get_texture(directory_path + tokens[1]).get_id();
					else if (tokens[0] == "map_Ks") specular_texture = texture_loader::get_texture(directory_path + tokens[1]).get_id();
				}
			}
		}

		// Detect material for each mesh
		else if (file_line.substr(0, 7) == "usemtl ") {
			current_material = split_line[1];

			// Check if the material exists in the map
			if (!does_material_exist(current_material)) {
				std::cerr << "Warning: Material '" << current_material << "' not found in g_materials.\n";
			}
		}
	}

	// Load final mesh or in case model is single mesh
	if (push_back_mesh) {
		std::vector<std::uint32_t> final_indices(final_vertices.size());
		for (size_t i = 0; i < final_vertices.size(); i++) final_indices[i] = i;
		if (current_material.empty()) {
			model.meshes.emplace_back(final_vertices, final_indices, nullptr);
		}
		else {
			model.meshes.emplace_back(final_vertices, final_indices, get_material(current_material));
		}

		// Clear data to load next mesh
		push_back_mesh = false;
		final_vertices.clear();
	}

	pre_load_vertices.clear();
	pre_load_texcoords.clear();
	pre_load_normals.clear();

	model_handle handle(model);
	g_loaded_model_paths.insert(std::make_pair(handle, model_path));
	g_queued_models.emplace_back(std::move(model), model_path);

	return handle;
}

auto gse::model_loader::add_model(const std::vector<mesh_data>& mesh_data, const std::string& model_name) -> model_handle {
	for (const auto& [id, path] : g_loaded_model_paths) {
		if (path == model_name) {
			return id; // Already loaded
		}
	}

	model model(model_name);

	for (const auto& data : mesh_data) {
		model.meshes.emplace_back(data);
	}

	model_handle handle(model);
	g_loaded_model_paths.insert(std::make_pair(handle, model_name));
	g_queued_models.emplace_back(std::move(model), model_name);

	return handle;
}

auto gse::model_loader::get_model(const model_handle& handle) -> const model& {
	if (const auto it = g_models.find(handle); it != g_models.end()) {
		return it->second;
	}

	const auto it2 = std::ranges::find_if(g_queued_models, [&handle](const auto& pair) { return pair.first.get_id() == handle.get_model_id(); });
	assert(it2 != g_queued_models.end(), "Model not found.");

	return it2->first;
}

auto gse::model_loader::get_model(const id& id) -> const model& {
	if (const auto it = std::ranges::find_if(g_models, [&id](const auto& pair) { return pair.first.get_model_id() == id; }); it != g_models.end()) {
		return it->second;
	}

	const auto it2 = std::ranges::find_if(g_queued_models, [&id](const auto& pair) { return pair.first.get_id() == id; });

	assert(it2 != g_queued_models.end(), "Model not found.");
	return it2->first;
}

auto gse::model_loader::load_queued_models(const vulkan::config& config) -> void {
	for (auto& [model, path] : g_queued_models) {
		model.initialize(config);
		auto handle = model_handle(model);
		g_models.insert(std::make_pair(handle, std::move(model)));
		g_loaded_model_paths[handle] = std::move(path);
	}
	g_queued_models.clear();
}


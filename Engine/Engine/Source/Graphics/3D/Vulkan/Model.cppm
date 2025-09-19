export module gse.graphics:model;

import std;

import :mesh;
import :rendering_context;
import :material;

import gse.utility;
import gse.platform;
import gse.physics;
import gse.physics.math;
import gse.assert;

export namespace gse {
	class model;

	struct render_queue_entry {
		const resource::handle<model> model;
		std::size_t index;
		mat4 model_matrix;
		mat4 normal_matrix;
		unitless::vec3 color;
	};

	class model_instance {
	public:
		model_instance(const resource::handle<model>& model_handle) : m_model_handle(model_handle) {}

		auto update(const physics::motion_component& mc, const physics::collision_component& cc) -> void;

		auto render_queue_entries() -> std::span<render_queue_entry>;
		auto handle() const -> const resource::handle<model>&;
	private:
		std::vector<render_queue_entry> m_render_queue_entries;
		resource::handle<model> m_model_handle;

		vec3<length> m_position;
		quat m_rotation;
		unitless::vec3 m_scale = { 1.f, 1.f, 1.f };
		bool m_is_dirty = true;
	};

	class model : public identifiable {
	public:
		explicit model(const std::filesystem::path& path) : identifiable(path), m_baked_model_path(path) {}
		explicit model(std::string_view name, std::vector<mesh_data> meshes);

		static auto compile() -> std::set<std::filesystem::path>;

		auto load(const renderer::context& context) -> void;
		auto unload() -> void;

		auto meshes() const -> std::span<const mesh>;
		auto center_of_mass() const -> vec3<length>;
	private:
		friend class model_instance;

		std::vector<mesh> m_meshes;
		std::filesystem::path m_baked_model_path;
		vec3<length> m_center_of_mass;
	};
}

gse::model::model(const std::string_view name, std::vector<mesh_data> meshes) : identifiable(name) {
	m_meshes.reserve(meshes.size());
	for (auto& mesh_data : meshes) {
		m_meshes.emplace_back(std::move(mesh_data));
	}
}

auto gse::model::compile() -> std::set<std::filesystem::path> {
	const auto source_root = config::resource_path / "Models";
	const auto baked_root = config::baked_resource_path / "Models";

	if (!exists(source_root)) return {};

	if (!exists(baked_root)) {
		create_directories(baked_root);
	}

	std::set<std::filesystem::path> resources;

	for (const auto& model_dir_entry : std::filesystem::directory_iterator(source_root)) {
		if (!model_dir_entry.is_directory()) continue;

		const auto model_name = model_dir_entry.path().filename().string();
		std::filesystem::path source_obj_path;

		for (const auto& file_entry : std::filesystem::directory_iterator(model_dir_entry.path())) {
			if (file_entry.path().extension() == ".obj") {
				source_obj_path = file_entry.path();
				break;
			}
		}

		if (source_obj_path.empty()) {
			continue;
		}

		const auto baked_model_path = baked_root / (model_name + ".gmdl");
		resources.insert(baked_model_path);

		if (exists(baked_model_path)) {
			auto src_time = last_write_time(source_obj_path);
			if (auto dst_time = last_write_time(baked_model_path); src_time <= dst_time) {
				continue;
			}
		}

		std::ifstream model_file(source_obj_path);
		assert(model_file.is_open(), "Failed to open model file.");

		auto split = [](
			const std::string& str,
			const char delimiter = ' '
			) -> std::vector<std::string> {
				std::vector<std::string> tokens;
				const size_t length = str.length();
				size_t i = 0;
				while (i < length) {
					while (i < length && str[i] == delimiter) { ++i; }
					const size_t start = i;
					while (i < length && str[i] != delimiter) { ++i; }
					if (start < i) { tokens.emplace_back(str.substr(start, i - start)); }
				}
				return tokens;
			};

		struct mesh_build_data {
			std::string material_name;
			std::vector<vertex> vertices;
		};

		std::vector<mesh_build_data> built_meshes;
		std::string current_material_name;

		std::vector<vec3<length>> temp_positions;
		std::vector<unitless::vec3> temp_normals;
		std::vector<unitless::vec2> temp_tex_coords;

		std::string line;
		while (std::getline(model_file, line)) {
			const auto tokens = split(line, ' ');
			if (tokens.empty()) continue;

			if (const auto& type = tokens[0]; type == "mtllib") {

			}
			else if (type == "usemtl") {
				current_material_name = tokens[1];
				if (std::ranges::find_if(built_meshes, [&](const mesh_build_data& m) { return m.material_name == current_material_name; }) == built_meshes.end()) {
					built_meshes.emplace_back(mesh_build_data{ .material_name = current_material_name });
				}
			}
			else if (type == "v") {
				temp_positions.push_back({ std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]) });
			}
			else if (type == "vt") {
				temp_tex_coords.push_back({ std::stof(tokens[1]), std::stof(tokens[2]) });
			}
			else if (type == "vn") {
				temp_normals.push_back({ std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]) });
			}
			else if (type == "f") {
				auto mesh_it = std::ranges::find_if(
					built_meshes,
					[&](const auto& m) {
						return m.material_name == current_material_name;
					}
				);

				if (mesh_it == built_meshes.end()) {
					if (current_material_name.empty()) {
						current_material_name = "default";
					}
					built_meshes.emplace_back(mesh_build_data{ .material_name = current_material_name });
					mesh_it = std::prev(built_meshes.end());
				}

				auto& [material_name, vertices] = *mesh_it;

				auto process_face_vertex = [&](
					const std::string& token
					) {
						const auto indices = split(token, '/');
						vertex v;

						if (!indices.empty() && !indices[0].empty()) {
							if (const size_t idx = std::stoul(indices[0]) - 1; idx < temp_positions.size()) {
								v.position = temp_positions[idx];
							}
						}

						if (indices.size() > 1 && !indices[1].empty()) {
							if (const size_t idx = std::stoul(indices[1]) - 1; idx < temp_tex_coords.size()) {
								v.tex_coords = temp_tex_coords[idx];
							}
						}

						if (indices.size() > 2 && !indices[2].empty()) {
							if (const size_t idx = std::stoul(indices[2]) - 1; idx < temp_normals.size()) {
								v.normal = temp_normals[idx];
							}
						}
						return v;
					};

				if (tokens.size() == 4) {
					vertices.push_back(process_face_vertex(tokens[1]));
					vertices.push_back(process_face_vertex(tokens[2]));
					vertices.push_back(process_face_vertex(tokens[3]));
				}
				else if (tokens.size() == 5) {
					vertices.push_back(process_face_vertex(tokens[1]));
					vertices.push_back(process_face_vertex(tokens[2]));
					vertices.push_back(process_face_vertex(tokens[3]));
					vertices.push_back(process_face_vertex(tokens[1]));
					vertices.push_back(process_face_vertex(tokens[3]));
					vertices.push_back(process_face_vertex(tokens[4]));
				}
			}
		}

		std::ofstream out_file(baked_model_path, std::ios::binary);
		assert(out_file.is_open(), "Failed to open baked model file for writing.");

		constexpr uint32_t magic = 0x474D444C;
		constexpr uint32_t version = 1;
		out_file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
		out_file.write(reinterpret_cast<const char*>(&version), sizeof(version));

		uint64_t mesh_count = built_meshes.size();
		out_file.write(reinterpret_cast<const char*>(&mesh_count), sizeof(mesh_count));

		for (const auto& [material_name, vertices] : built_meshes) {
			uint64_t mat_name_len = material_name.length();
			out_file.write(reinterpret_cast<const char*>(&mat_name_len), sizeof(mat_name_len));
			out_file.write(material_name.c_str(), mat_name_len);

			uint64_t vertex_count = vertices.size();
			out_file.write(reinterpret_cast<const char*>(&vertex_count), sizeof(vertex_count));
			out_file.write(reinterpret_cast<const char*>(vertices.data()), vertex_count * sizeof(vertex));
		}

		out_file.close();
	}

	return resources;
}

auto gse::model::load(const renderer::context& context) -> void {
	if (!m_baked_model_path.empty()) {
		m_meshes.clear();

		std::ifstream in_file(m_baked_model_path, std::ios::binary);
		assert(in_file.is_open(), "Failed to open baked model file for reading.");

		std::uint32_t magic, version;
		in_file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
		in_file.read(reinterpret_cast<char*>(&version), sizeof(version));

		std::uint64_t mesh_count;
		in_file.read(reinterpret_cast<char*>(&mesh_count), sizeof(mesh_count));
		m_meshes.reserve(mesh_count);

		for (std::uint64_t i = 0; i < mesh_count; ++i) {
			std::uint64_t mat_name_len;
			in_file.read(reinterpret_cast<char*>(&mat_name_len), sizeof(mat_name_len));
			std::string material_name(mat_name_len, '\0');
			in_file.read(&material_name[0], mat_name_len);

			resource::handle<material> material_handle = context.get<material>(material_name);

			std::uint64_t vertex_count;
			in_file.read(reinterpret_cast<char*>(&vertex_count), sizeof(vertex_count));
			std::vector<vertex> vertices(vertex_count);
			in_file.read(reinterpret_cast<char*>(vertices.data()), vertex_count * sizeof(vertex));

			std::vector<std::uint32_t> indices(vertex_count);
			std::iota(indices.begin(), indices.end(), 0);

			m_meshes.emplace_back(std::move(vertices), std::move(indices), material_handle);
		}
	}

	context.queue_gpu_command<model>(
		this, 
		[](renderer::context& ctx, model& self) {
			for (auto& mesh : self.m_meshes) {
				mesh.initialize(ctx.config());
			}
		}
	);

	vec3<length> sum;
	for (const auto& mesh : m_meshes) {
		sum += mesh.center_of_mass();
	}
	m_center_of_mass = m_meshes.empty() ? vec3<length>{} : sum / static_cast<float>(m_meshes.size());
}

auto gse::model::unload() -> void {
	m_meshes.clear();
}

auto gse::model::meshes() const -> std::span<const mesh> {
	return m_meshes;
}

auto gse::model::center_of_mass() const -> vec3<length> {
	return m_center_of_mass;
}

auto gse::model_instance::update(const physics::motion_component& mc, const physics::collision_component& cc) -> void {
	m_position = mc.current_position;
	m_rotation = cc.bounding_box.obb().orientation;
	m_scale = cc.bounding_box.size().as<meters>();
	m_is_dirty = true;
}

auto gse::model_instance::render_queue_entries() -> std::span<render_queue_entry> {
	if (m_render_queue_entries.empty() && m_model_handle.valid()) {
		const auto* resolved_model = m_model_handle.resolve();
		m_render_queue_entries.reserve(resolved_model->meshes().size());
		for (std::size_t i = 0; i < resolved_model->meshes().size(); ++i) {
			m_render_queue_entries.emplace_back(m_model_handle, i, mat4(1.0f), mat4(1.0f), unitless::vec3(1.0f));
		}
	}

	if (m_is_dirty) {
		const mat4 scale_mat = scale(mat4(1.0f), m_scale);
		const mat4 rot_mat = m_rotation;
		const mat4 trans_mat = translate(mat4(1.0f), m_position);
		const vec3 center_of_mass = m_model_handle.resolve()->center_of_mass(); 
		const mat4 pivot_correction_mat = translate(mat4(1.0f), -center_of_mass);
		const mat4 final_model_matrix = trans_mat * rot_mat * scale_mat * pivot_correction_mat;
		const mat4 normal_matrix = final_model_matrix.inverse().transpose();

		for (auto& entry : m_render_queue_entries) {
			entry.model_matrix = final_model_matrix;
			entry.normal_matrix = normal_matrix;
		}

		m_is_dirty = false;
	}

	return m_render_queue_entries;
}

auto gse::model_instance::handle() const -> const resource::handle<model>& {
	return m_model_handle;
}
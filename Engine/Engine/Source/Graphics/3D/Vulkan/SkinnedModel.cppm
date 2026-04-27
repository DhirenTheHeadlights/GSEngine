export module gse.graphics:skinned_model;

import std;

import :skinned_mesh;

import gse.config;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.physics;
import gse.math;
import gse.assert;

export namespace gse {
	class skinned_model;

	struct skinned_render_queue_entry {
		resource::handle<skinned_model> model;
		std::size_t index;
		mat4f model_matrix;
		mat4f normal_matrix;
		vec3f color;
		std::uint32_t skin_offset;
		std::uint32_t joint_count;
	};

	class skinned_model_instance {
	public:
		explicit skinned_model_instance(const resource::handle<skinned_model>& model_handle) : m_model_handle(model_handle) {}

		auto update(const physics::motion_component& mc, const physics::collision_component& cc, std::uint32_t skin_offset, std::uint32_t joint_count) -> void;

		auto render_queue_entries() const -> std::span<const skinned_render_queue_entry>;
		auto handle() const -> const resource::handle<skinned_model>&;
	private:
		std::vector<skinned_render_queue_entry> m_render_queue_entries;
		resource::handle<skinned_model> m_model_handle;

		vec3<position> m_position;
		quat m_rotation;
		vec3<displacement> m_scale = { meters(1.f), meters(1.f), meters(1.f) };
		bool m_is_dirty = true;
		std::size_t m_cached_mesh_count = 0;
	};

	class skinned_model : public identifiable {
	public:
		explicit skinned_model(const std::filesystem::path& path) : identifiable(path, config::baked_resource_path), m_baked_model_path(path) {}
		explicit skinned_model(std::string_view name, std::vector<skinned_mesh_data> meshes);

		auto load(gpu::context& context) -> void;
		auto unload() -> void;

		auto meshes() const -> std::span<const skinned_mesh>;
		auto center_of_mass() const -> vec3<length>;
	private:
		friend class skinned_model_instance;

		std::vector<skinned_mesh> m_meshes;
		std::filesystem::path m_baked_model_path;
		vec3<length> m_center_of_mass;
	};
}

gse::skinned_model::skinned_model(const std::string_view name, std::vector<skinned_mesh_data> meshes) : identifiable(name) {
	m_meshes.reserve(meshes.size());
	for (auto& mesh_data : meshes) {
		m_meshes.emplace_back(std::move(mesh_data));
	}
}

auto read_string(std::ifstream& file) -> std::string {
	std::uint32_t len;
	file.read(reinterpret_cast<char*>(&len), sizeof(len));
	std::string s(len, '\0');
	file.read(s.data(), len);
	return s;
}

template <typename T>
auto read_value(std::ifstream& file) -> T {
	T v;
	file.read(reinterpret_cast<char*>(&v), sizeof(T));
	return v;
}

auto gse::skinned_model::load(gpu::context& context) -> void {
	if (!m_baked_model_path.empty() && exists(m_baked_model_path)) {
		m_meshes.clear();

		const auto model_relative = m_baked_model_path.lexically_relative(config::baked_resource_path);
		auto texture_dir = model_relative.parent_path().string();
		std::ranges::replace(texture_dir, '\\', '/');
		if (texture_dir.starts_with("SkinnedModels")) {
			texture_dir = "Textures/" + texture_dir;
		}

		if (std::ifstream file(m_baked_model_path, std::ios::binary); file) {
			char magic[4];
			file.read(magic, 4);

			if (std::memcmp(magic, "GSMD", 4) != 0) return;

			const auto version = read_value<std::uint32_t>(file);
			const auto mesh_count = read_value<std::uint32_t>(file);
			m_meshes.reserve(mesh_count);

			for (std::uint32_t m = 0; m < mesh_count; ++m) {
				gse::material mat;

				if (version >= 2) {
					mat.base_color = {
						read_value<float>(file),
						read_value<float>(file),
						read_value<float>(file)
					};
					mat.roughness = read_value<float>(file);
					mat.metallic = read_value<float>(file);

					auto albedo_file = read_string(file);
					auto normal_file = read_string(file);
					auto rm_file = read_string(file);

					if (!albedo_file.empty()) {
						auto stem = std::filesystem::path(albedo_file).stem().string();
						mat.diffuse_texture = context.assets().get<texture>(texture_dir + "/" + stem);
					}
					if (!normal_file.empty()) {
						auto stem = std::filesystem::path(normal_file).stem().string();
						mat.normal_texture = context.assets().get<texture>(texture_dir + "/" + stem);
					}
					if (!rm_file.empty()) {
						auto stem = std::filesystem::path(rm_file).stem().string();
						mat.specular_texture = context.assets().get<texture>(texture_dir + "/" + stem);
					}
				}
				else {
					read_string(file);
				}

				const auto vertex_count = read_value<std::uint32_t>(file);

				std::vector<skinned_vertex> vertices(vertex_count);
				for (std::uint32_t v = 0; v < vertex_count; ++v) {
					float px = read_value<float>(file), py = read_value<float>(file), pz = read_value<float>(file);
					vertices[v].position = vec3<displacement>{ meters(px), meters(py), meters(pz) };

					float nx = read_value<float>(file), ny = read_value<float>(file), nz = read_value<float>(file);
					vertices[v].normal = vec3f{ nx, ny, nz };

					float u = read_value<float>(file), vt = read_value<float>(file);
					vertices[v].tex_coords = vec2f{ u, vt };

					file.read(reinterpret_cast<char*>(vertices[v].bone_indices.data()), 4 * sizeof(std::uint32_t));

					float w0 = read_value<float>(file), w1 = read_value<float>(file);
					float w2 = read_value<float>(file), w3 = read_value<float>(file);
					vertices[v].bone_weights = vec4f{ w0, w1, w2, w3 };
				}

				const auto index_count = read_value<std::uint32_t>(file);
				std::vector<std::uint32_t> indices(index_count);
				file.read(reinterpret_cast<char*>(indices.data()), index_count * sizeof(std::uint32_t));

				m_meshes.emplace_back(skinned_mesh_data{
					.vertices = std::move(vertices),
					.indices = std::move(indices),
					.material = std::move(mat)
				});
			}
		}
	}

	context.queue_gpu_command<skinned_model>(
		this,
		[](gpu::context& ctx, skinned_model& self) {
			for (auto& mesh : self.m_meshes) {
				mesh.initialize(ctx);
			}
		}
	);

	vec3<length> sum;
	for (const auto& mesh : m_meshes) {
		sum += mesh.center_of_mass();
	}
	m_center_of_mass = m_meshes.empty() ? vec3<length>{} : sum / static_cast<float>(m_meshes.size());
}

auto gse::skinned_model::unload() -> void {
	m_meshes.clear();
}

auto gse::skinned_model::meshes() const -> std::span<const skinned_mesh> {
	return m_meshes;
}

auto gse::skinned_model::center_of_mass() const -> vec3<length> {
	return m_center_of_mass;
}

auto gse::skinned_model_instance::update(const physics::motion_component& mc, const physics::collision_component& cc, const std::uint32_t skin_offset, const std::uint32_t joint_count) -> void {
	m_position = mc.current_position;
	m_rotation = mc.orientation;
	m_scale = cc.bounding_box.size();
	m_is_dirty = true;

	if (!m_model_handle.valid()) {
		m_render_queue_entries.clear();
		m_cached_mesh_count = 0;
	}
	else {
		const auto* resolved = m_model_handle.resolve();
		const std::size_t mesh_count = resolved ? resolved->meshes().size() : 0;
		
		if (mesh_count == 0) {
			m_render_queue_entries.clear();
			m_cached_mesh_count = 0;
		}
		else {
			if (m_render_queue_entries.size() != mesh_count || m_cached_mesh_count != mesh_count) {
				m_render_queue_entries.clear();
				m_render_queue_entries.reserve(mesh_count);

				for (std::size_t i = 0; i < mesh_count; ++i) {
					m_render_queue_entries.emplace_back(
						skinned_render_queue_entry{
							.model = m_model_handle,
							.index = i,
							.model_matrix = mat4f(1.0f),
							.normal_matrix = mat4f(1.0f),
							.color = vec3f(1.0f),
							.skin_offset = skin_offset,
							.joint_count = joint_count
						}
					);
				}

				m_cached_mesh_count = mesh_count;
				m_is_dirty = true;
			}
		}
	}

	if (!m_is_dirty || m_render_queue_entries.empty() || !m_model_handle.valid()) {
		return;
	}

	const mat4f scale_mat = scale(mat4f(1.0f), m_scale);
	const mat4f rot_mat = m_rotation;
	const mat4f trans_mat = translate(mat4f(1.0f), m_position);
	const mat4f final_model_matrix = trans_mat * rot_mat * scale_mat;
	const mat4f normal_matrix = final_model_matrix.inverse().transpose();

	for (auto& entry : m_render_queue_entries) {
		entry.model_matrix = final_model_matrix;
		entry.normal_matrix = normal_matrix;
		entry.skin_offset = skin_offset;
		entry.joint_count = joint_count;
	}

	m_is_dirty = false;
}

auto gse::skinned_model_instance::render_queue_entries() const -> std::span<const skinned_render_queue_entry> {
	return m_render_queue_entries;
}

auto gse::skinned_model_instance::handle() const -> const resource::handle<skinned_model>& {
	return m_model_handle;
}

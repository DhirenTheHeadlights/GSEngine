export module gse.graphics:skinned_model;

import std;

import :skinned_mesh;
import :material;

import gse.utility;
import gse.platform;
import gse.physics;
import gse.math;
import gse.assert;

export namespace gse {
	class skinned_model;

	struct skinned_render_queue_entry {
		resource::handle<skinned_model> model;
		std::size_t index;
		mat4 model_matrix;
		mat4 normal_matrix;
		unitless::vec3 color;
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

		vec3<length> m_position;
		quat m_rotation;
		unitless::vec3 m_scale = { 1.f, 1.f, 1.f };
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

auto gse::skinned_model::load(gpu::context& context) -> void {
	if (!m_baked_model_path.empty() && exists(m_baked_model_path)) {
		m_meshes.clear();

		const auto model_relative = m_baked_model_path.lexically_relative(config::baked_resource_path);
		const auto material_dir = config::baked_resource_path / "Materials" / model_relative.parent_path();

		if (std::ifstream file(m_baked_model_path, std::ios::binary); file) {
			char magic[4];
			file.read(magic, 4);

			if (std::memcmp(magic, "GSMD", 4) == 0) {
				std::uint32_t version;
				file.read(reinterpret_cast<char*>(&version), sizeof(version));

				std::uint32_t mesh_count;
				file.read(reinterpret_cast<char*>(&mesh_count), sizeof(mesh_count));

				m_meshes.reserve(mesh_count);

				for (std::uint32_t m = 0; m < mesh_count; ++m) {
					std::uint32_t material_name_len;
					file.read(reinterpret_cast<char*>(&material_name_len), sizeof(material_name_len));
					std::string material_name(material_name_len, '\0');
					file.read(material_name.data(), material_name_len);

					const auto material_path = material_dir / (material_name + ".gmat");
					resource::handle<material> material_handle;
					if (std::filesystem::exists(material_path)) {
						material_handle = context.queue<material>(material_path.string());
					}

					std::uint32_t vertex_count;
					file.read(reinterpret_cast<char*>(&vertex_count), sizeof(vertex_count));

					std::vector<skinned_vertex> vertices(vertex_count);
					for (std::uint32_t v = 0; v < vertex_count; ++v) {
						float px, py, pz;
						file.read(reinterpret_cast<char*>(&px), sizeof(float));
						file.read(reinterpret_cast<char*>(&py), sizeof(float));
						file.read(reinterpret_cast<char*>(&pz), sizeof(float));
						vertices[v].position = vec3<length>{ px, py, pz };

						float nx, ny, nz;
						file.read(reinterpret_cast<char*>(&nx), sizeof(float));
						file.read(reinterpret_cast<char*>(&ny), sizeof(float));
						file.read(reinterpret_cast<char*>(&nz), sizeof(float));
						vertices[v].normal = unitless::vec3{ nx, ny, nz };

						float u, vt;
						file.read(reinterpret_cast<char*>(&u), sizeof(float));
						file.read(reinterpret_cast<char*>(&vt), sizeof(float));
						vertices[v].tex_coords = unitless::vec2{ u, vt };

						file.read(reinterpret_cast<char*>(vertices[v].bone_indices.data()), 4 * sizeof(std::uint32_t));

						float w0, w1, w2, w3;
						file.read(reinterpret_cast<char*>(&w0), sizeof(float));
						file.read(reinterpret_cast<char*>(&w1), sizeof(float));
						file.read(reinterpret_cast<char*>(&w2), sizeof(float));
						file.read(reinterpret_cast<char*>(&w3), sizeof(float));
						vertices[v].bone_weights = unitless::vec4{ w0, w1, w2, w3 };
					}

					std::uint32_t index_count;
					file.read(reinterpret_cast<char*>(&index_count), sizeof(index_count));

					std::vector<std::uint32_t> indices(index_count);
					file.read(reinterpret_cast<char*>(indices.data()), index_count * sizeof(std::uint32_t));

					m_meshes.emplace_back(skinned_mesh_data{
						.vertices = std::move(vertices),
						.indices = std::move(indices),
						.material = material_handle
					});
				}
			}
		}
	}

	context.queue_gpu_command<skinned_model>(
		this,
		[](gpu::context& ctx, skinned_model& self) {
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
	m_position = mc.render_position;
	m_rotation = mc.render_orientation;
	m_scale = cc.bounding_box.size().as<meters>();
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
							.model_matrix = mat4(1.0f),
							.normal_matrix = mat4(1.0f),
							.color = unitless::vec3(1.0f),
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

	const mat4 rot_mat = m_rotation;
	const mat4 trans_mat = translate(mat4(1.0f), m_position);
	const mat4 final_model_matrix = trans_mat * rot_mat;
	const mat4 normal_matrix = final_model_matrix.inverse().transpose();

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

export module gse.graphics:model;

import std;

import :mesh;
import :material;

import gse.utility;
import gse.platform;
import gse.physics;
import gse.math;
import gse.assert;
import gse.log;

export namespace gse {
	class model;

	struct render_queue_entry {
		resource::handle<model> model;
		std::size_t index;
		mat4f model_matrix;
		mat4f normal_matrix;
		vec3f color;
	};

	class model_instance {
	public:
		explicit model_instance(const resource::handle<model>& model_handle) : m_model_handle(model_handle) {}

		auto update(const physics::motion_component& mc, const physics::collision_component& cc) -> void;

		auto render_queue_entries() const -> std::span<const render_queue_entry>;
		auto handle() const -> const resource::handle<model>&;
	private:
		std::vector<render_queue_entry> m_render_queue_entries;
		resource::handle<model> m_model_handle;

		vec3<position> m_position;
		quat m_rotation;
		vec3<displacement> m_scale = { meters(1.f), meters(1.f), meters(1.f) };
		bool m_is_dirty = true;
		std::size_t m_cached_mesh_count = 0;
	};

	class model : public identifiable {
	public:
		explicit model(const std::filesystem::path& path) : identifiable(path, config::baked_resource_path), m_baked_model_path(path) {}
		explicit model(std::string_view name, std::vector<mesh_data> meshes);

		auto load(gpu::context& context) -> void;
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

auto gse::model::load(gpu::context& context) -> void {
	if (!m_baked_model_path.empty()) {
		m_meshes.clear();

		std::ifstream in_file(m_baked_model_path, std::ios::binary);
		assert(in_file.is_open(), std::source_location::current(), "Failed to open baked model file for reading.");

		auto read_val = [&]<typename T>(T& val) {
			in_file.read(reinterpret_cast<char*>(&val), sizeof(T));
		};

		std::uint32_t magic, version;
		read_val(magic);
		read_val(version);
		assert(magic == 0x474D444C && version == 2, std::source_location::current(),
			"Invalid or outdated .gmdl file: {}", m_baked_model_path.string());

		std::uint64_t mesh_count;
		read_val(mesh_count);
		m_meshes.reserve(mesh_count);

		const auto model_relative = m_baked_model_path.lexically_relative(config::baked_resource_path);
		const auto material_dir = config::baked_resource_path / "Materials" / model_relative.parent_path();

		for (std::uint64_t i = 0; i < mesh_count; ++i) {
			std::uint64_t mat_name_len;
			read_val(mat_name_len);
			std::string material_name(mat_name_len, '\0');
			in_file.read(material_name.data(), mat_name_len);

			const auto material_path = material_dir / (material_name + ".gmat");
			resource::handle<material> material_handle;
			if (std::filesystem::exists(material_path)) {
				material_handle = context.queue<material>(material_path.string());
			}

			std::uint64_t vertex_count;
			read_val(vertex_count);
			std::vector<vertex> vertices(vertex_count);
			in_file.read(reinterpret_cast<char*>(vertices.data()), vertex_count * sizeof(vertex));

			std::uint64_t index_count;
			read_val(index_count);
			std::vector<std::uint32_t> indices(index_count);
			in_file.read(reinterpret_cast<char*>(indices.data()), index_count * sizeof(std::uint32_t));

			meshlet_data ml;

			std::uint64_t meshlet_count;
			read_val(meshlet_count);
			if (meshlet_count == 0) {
				log::println(
					log::level::warning,
					log::category::render,
					"Model '{}' mesh {} has no meshlets; mesh shader path will skip it",
					m_baked_model_path.string(),
					i
				);
			}

			ml.descriptors.resize(meshlet_count);
			in_file.read(reinterpret_cast<char*>(ml.descriptors.data()), meshlet_count * sizeof(meshlet_descriptor));

			std::uint64_t meshlet_vertex_count;
			read_val(meshlet_vertex_count);
			ml.vertex_indices.resize(meshlet_vertex_count);
			in_file.read(reinterpret_cast<char*>(ml.vertex_indices.data()), meshlet_vertex_count * sizeof(std::uint32_t));

			std::uint64_t meshlet_triangle_count;
			read_val(meshlet_triangle_count);
			ml.triangles.resize(meshlet_triangle_count);
			in_file.read(reinterpret_cast<char*>(ml.triangles.data()), meshlet_triangle_count);

			ml.bounds.resize(meshlet_count);
			in_file.read(reinterpret_cast<char*>(ml.bounds.data()), meshlet_count * sizeof(meshlet_bounds));

			m_meshes.emplace_back(mesh_data{
				.vertices = std::move(vertices),
				.indices = std::move(indices),
				.material = material_handle,
				.meshlets = std::move(ml)
			});
		}
	}

	context.queue_gpu_command<model>(
		this, 
		[](gpu::context& ctx, model& self) {
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
	m_position = mc.render_position;
	m_rotation = mc.render_orientation;
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
						render_queue_entry{
							.model = m_model_handle,
							.index = i,
							.model_matrix = mat4f(1.0f),
							.normal_matrix = mat4f(1.0f),
							.color = vec3f(1.0f)
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

	const auto* mdl = m_model_handle.resolve();
	const vec3 center_of_mass = mdl->center_of_mass();

	const mat4f scale_mat             = scale(mat4f(1.0f), m_scale);
	const mat4f rot_mat               = m_rotation;
	const mat4f trans_mat             = translate(mat4f(1.0f), m_position);
	const mat4f pivot_correction_mat  = translate(mat4f(1.0f), -center_of_mass);
	const mat4f final_model_matrix    = trans_mat * rot_mat * scale_mat * pivot_correction_mat;
	const mat4f normal_matrix         = final_model_matrix.inverse().transpose();

	for (auto& entry : m_render_queue_entries) {
		entry.model_matrix  = final_model_matrix;
		entry.normal_matrix = normal_matrix;
	}

	m_is_dirty = false;
}

auto gse::model_instance::render_queue_entries() const -> std::span<const render_queue_entry> {
	return m_render_queue_entries;
}

auto gse::model_instance::handle() const -> const resource::handle<model>& {
	return m_model_handle;
}

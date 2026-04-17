export module gse.graphics:model;

import std;

import :mesh;

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

		auto sync_structure() -> void;
		auto sync_transform(const physics::motion_component& mc, const physics::collision_component& cc) -> void;

		auto render_queue_entries() const -> std::span<const render_queue_entry>;
		auto handle() const -> const resource::handle<model>&;
	private:
		std::vector<render_queue_entry> m_render_queue_entries;
		resource::handle<model> m_model_handle;
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
		if (!in_file.is_open()) return;

		binary_reader ar(in_file, 0x474D444C, 4, m_baked_model_path.string());
		if (!ar.valid()) return;

		std::uint32_t mesh_count = 0;
		ar & mesh_count;
		m_meshes.reserve(mesh_count);

		const auto model_relative = m_baked_model_path.lexically_relative(config::baked_resource_path);
		auto texture_dir = model_relative.parent_path().string();
		std::ranges::replace(texture_dir, '\\', '/');
		if (texture_dir.starts_with("Models/")) {
			texture_dir = "Textures/" + texture_dir.substr(7);
		}

		for (std::uint32_t i = 0; i < mesh_count; ++i) {
			gse::material mat;
			ar & mat.base_color & mat.roughness & mat.metallic;

			std::string albedo_file, normal_file, rm_file;
			ar & albedo_file & normal_file & rm_file;

			if (!albedo_file.empty()) {
				auto stem = std::filesystem::path(albedo_file).stem().string();
				mat.diffuse_texture = context.get<texture>(texture_dir + "/" + stem);
			}
			if (!normal_file.empty()) {
				auto stem = std::filesystem::path(normal_file).stem().string();
				mat.normal_texture = context.get<texture>(texture_dir + "/" + stem);
			}
			if (!rm_file.empty()) {
				auto stem = std::filesystem::path(rm_file).stem().string();
				mat.specular_texture = context.get<texture>(texture_dir + "/" + stem);
			}

			std::vector<vertex> vertices;
			std::vector<std::uint32_t> indices;
			ar & raw_blob(vertices);
			ar & raw_blob(indices);

			m_meshes.emplace_back(mesh_data{
				.vertices = std::move(vertices),
				.indices = std::move(indices),
				.material = std::move(mat),
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

auto gse::model_instance::sync_structure() -> void {
	if (!m_model_handle.valid()) {
		m_render_queue_entries.clear();
		m_cached_mesh_count = 0;
		return;
	}

	const auto* resolved = m_model_handle.resolve();
	const std::size_t mesh_count = resolved ? resolved->meshes().size() : 0;

	if (mesh_count == 0) {
		m_render_queue_entries.clear();
		m_cached_mesh_count = 0;
		return;
	}

	if (m_cached_mesh_count == mesh_count && m_render_queue_entries.size() == mesh_count) {
		return;
	}

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
}

auto gse::model_instance::sync_transform(const physics::motion_component& mc, const physics::collision_component& cc) -> void {
	if (m_render_queue_entries.empty() || !m_model_handle.valid()) {
		return;
	}

	const auto* mdl = m_model_handle.resolve();
	const vec3 center_of_mass = mdl->center_of_mass();

	const mat4f scale_mat             = scale(mat4f(1.0f), cc.bounding_box.size());
	const mat4f rot_mat               = mc.orientation;
	const mat4f trans_mat             = translate(mat4f(1.0f), mc.current_position);
	const mat4f pivot_correction_mat  = translate(mat4f(1.0f), -center_of_mass);
	const mat4f final_model_matrix    = trans_mat * rot_mat * scale_mat * pivot_correction_mat;
	const mat4f normal_matrix         = final_model_matrix.inverse().transpose();

	for (auto& entry : m_render_queue_entries) {
		entry.model_matrix  = final_model_matrix;
		entry.normal_matrix = normal_matrix;
	}
}

auto gse::model_instance::render_queue_entries() const -> std::span<const render_queue_entry> {
	return m_render_queue_entries;
}

auto gse::model_instance::handle() const -> const resource::handle<model>& {
	return m_model_handle;
}

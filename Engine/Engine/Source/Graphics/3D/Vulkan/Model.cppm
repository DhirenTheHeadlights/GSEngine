export module gse.graphics:model;

import std;

import :mesh;

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
		struct material_baked {
			vec3f base_color = vec3f(1.0f);
			float roughness = 0.5f;
			float metallic = 0.0f;
			std::string albedo_file;
			std::string normal_file;
			std::string rm_file;
		};

		struct mesh_baked {
			material_baked material;
			raw_blob_owned<vertex> vertices;
			raw_blob_owned<std::uint32_t> indices;
		};

		struct [[
			= asset_format::baked_ext<".gmdl">{},
			= asset_format::baked_dir<"Models">{},
			= asset_format::magic<0x474D444C>{},
			= asset_format::version<4>{}
		]] baked {
			std::vector<mesh_baked> meshes;
		};

		explicit model(const std::filesystem::path& path) : identifiable(path, config::baked_resource_path), m_baked_model_path(path) {}
		explicit model(std::string_view name, std::vector<mesh_data> meshes);

		auto load(asset::load_ctx& ctx) -> void;
		auto unload() -> void;

		auto meshes() const -> std::span<const mesh>;
		auto center_of_mass() const -> vec3<length>;

		auto uploads_ready(
		) const -> bool;
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

auto gse::model::load(asset::load_ctx& ctx) -> void {
	if (!m_baked_model_path.empty()) {
		m_meshes.clear();

		model::baked baked{};
		if (!load_baked(m_baked_model_path, baked)) {
			return;
		}

		const auto model_relative = m_baked_model_path.lexically_relative(config::baked_resource_path);
		auto texture_dir = model_relative.parent_path().string();
		std::ranges::replace(texture_dir, '\\', '/');
		if (texture_dir.starts_with("Models/")) {
			texture_dir = "Textures/" + texture_dir.substr(7);
		}

		m_meshes.reserve(baked.meshes.size());
		for (auto& mb : baked.meshes) {
			gse::material mat;
			mat.base_color = mb.material.base_color;
			mat.roughness = mb.material.roughness;
			mat.metallic = mb.material.metallic;

			if (!mb.material.albedo_file.empty()) {
				auto stem = std::filesystem::path(mb.material.albedo_file).stem().string();
				mat.diffuse_texture = asset::get<texture>(ctx.assets, texture_dir + "/" + stem);
			}
			if (!mb.material.normal_file.empty()) {
				auto stem = std::filesystem::path(mb.material.normal_file).stem().string();
				mat.normal_texture = asset::get<texture>(ctx.assets, texture_dir + "/" + stem);
			}
			if (!mb.material.rm_file.empty()) {
				auto stem = std::filesystem::path(mb.material.rm_file).stem().string();
				mat.specular_texture = asset::get<texture>(ctx.assets, texture_dir + "/" + stem);
			}

			m_meshes.emplace_back(mesh_data{
				.vertices = std::move(mb.vertices.storage),
				.indices = std::move(mb.indices.storage),
				.material = std::move(mat),
			});
		}
	}

	gpu::queue_gpu_command(
		ctx,
		this,
		[](gpu::context::state& gpu_s, model& self) {
			for (auto& mesh : self.m_meshes) {
				mesh.initialize(gpu_s);
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

auto gse::model::uploads_ready() const -> bool {
	return std::ranges::all_of(m_meshes, [](const mesh& m) {
		return m.upload_token().ready() && m.material().textures_ready();
	});
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

	const auto box_size = cc.bounding_box.size();
	const mat4f scale_mat = scale(mat4f(1.0f), box_size);
	const mat4f rot_mat = mc.orientation;
	const mat4f trans_mat = translate(mat4f(1.0f), mc.current_position);
	const mat4f pivot_correction_mat = translate(mat4f(1.0f), -center_of_mass);
	const mat4f final_model_matrix = trans_mat * rot_mat * scale_mat * pivot_correction_mat;
	const vec3f inv_scale{
		1.0f / std::max(std::abs(box_size.x().as<meters>()), 1e-6f),
		1.0f / std::max(std::abs(box_size.y().as<meters>()), 1e-6f),
		1.0f / std::max(std::abs(box_size.z().as<meters>()), 1e-6f)
	};
	const mat4f normal_matrix = scale(rot_mat, inv_scale);

	for (auto& entry : m_render_queue_entries) {
		entry.model_matrix = final_model_matrix;
		entry.normal_matrix = normal_matrix;
	}
}

auto gse::model_instance::render_queue_entries() const -> std::span<const render_queue_entry> {
	return m_render_queue_entries;
}

auto gse::model_instance::handle() const -> const resource::handle<model>& {
	return m_model_handle;
}

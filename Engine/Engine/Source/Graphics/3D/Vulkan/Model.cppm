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
import gse.math;
import gse.assert;
import gse.log;

export namespace gse {
	class model;

	template <typename ModelType>
	struct render_queue_entry_t {
		resource::handle<ModelType> model;
		std::size_t index;
		mat4f model_matrix;
		mat4f normal_matrix;
		vec3f color;
		std::uint32_t skin_offset = 0;
		std::uint32_t joint_count = 0;
	};

	using render_queue_entry = render_queue_entry_t<model>;

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

		struct[[
			= asset_format::baked_ext<".gmdl">{},
			= asset_format::baked_dir<"Models">{},
			= asset_format::magic<0x474D444C>{},
			= asset_format::version<4>{}
		]] baked {
			std::vector<mesh_baked> meshes;
		};

		explicit model(const std::filesystem::path& path) : identifiable(path, config::baked_resource_path), m_baked_model_path(path) {
		}
		explicit model(std::string_view name, std::vector<mesh_data> meshes);

		auto load(asset::load_ctx& ctx) -> async::task<>;
		auto unload() -> void;

		auto meshes() const -> std::span<const mesh>;
		auto center_of_mass() const -> vec3<length>;

		auto uploads_ready() const -> bool;

	private:
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

auto gse::model::load(asset::load_ctx& ctx) -> async::task<> {
	if (!m_baked_model_path.empty()) {
		m_meshes.clear();

		model::baked baked{};
		if (!load_baked(m_baked_model_path, baked)) {
			co_return;
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

	auto& gpu_s = co_await gpu::on_gpu(ctx.channels);
	for (auto& mesh : m_meshes) {
		mesh.initialize(gpu_s);
	}

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

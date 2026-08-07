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

	struct render_queue_entry {
		resource::handle<model> model;
		std::size_t index;
		spatial_matrix model_matrix;
		spatial_matrix normal_matrix;
		spatial_matrix prev_model_matrix;
		vec4f color;
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
			= asset_format::version<5>{}
		]] baked {
			std::vector<mesh_baked> meshes;
		};

		explicit model(const std::filesystem::path& path)
			: identifiable(config::asset_tag(path)), m_baked_model_path(path) {
		}
		
		explicit model(
			std::string_view name,
			std::vector<mesh_data> meshes
		);

		auto load(
			asset::load_ctx& ctx
		) -> async::task<asset_result>;

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

auto gse::model::load(asset::load_ctx& ctx) -> async::task<asset_result> {
	if (!m_baked_model_path.empty()) {
		m_meshes.clear();

		auto baked = load_baked<model::baked>(m_baked_model_path);
		if (!baked) {
			co_return std::unexpected(std::move(baked.error()));
		}

		m_meshes.reserve(baked->meshes.size());
		for (auto& mb : baked->meshes) {
			gse::material mat{
				.base_color = mb.material.base_color,
				.roughness = mb.material.roughness,
				.metallic = mb.material.metallic,
				.diffuse_texture = asset::try_get<texture>(ctx.assets, mb.material.albedo_file),
				.normal_texture = asset::try_get<texture>(ctx.assets, mb.material.normal_file),
				.specular_texture = asset::try_get<texture>(ctx.assets, mb.material.rm_file),
			};

			m_meshes.emplace_back(
				mesh_data{
					.vertices = std::move(mb.vertices.storage),
					.indices = std::move(mb.indices.storage),
					.material = std::move(mat),
				}
			);
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
	co_return asset_result{};
}

auto gse::model::meshes() const -> std::span<const mesh> {
	return m_meshes;
}

auto gse::model::center_of_mass() const -> vec3<length> {
	return m_center_of_mass;
}

auto gse::model::uploads_ready() const -> bool {
	return std::ranges::all_of(
		m_meshes,
		[](const mesh& m) {
			return m.upload_token().ready() && m.material().textures_ready();
		}
	);
}

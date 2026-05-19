export module gse.graphics:skinned_model;

import std;

import :skinned_mesh;
import :model;

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

export namespace gse {
	class skinned_model;

	using skinned_render_queue_entry = render_queue_entry_t<skinned_model>;

	class skinned_model : public identifiable {
	public:
		struct[[
			= asset_format::baked_ext<".gsmdl">{},
			= asset_format::baked_dir<"SkinnedModels">{},
			= asset_format::source_dir<"SkinnedModels">{},
			= asset_format::source_exts<".gsmdl">{},
			= asset_format::magic<0x47534D44>{},
			= asset_format::version<1>{}
		]] baked {
			raw_blob_owned<std::byte> bytes;
		};

		explicit skinned_model(const std::filesystem::path& path)
			: identifiable(path, config::baked_resource_path),
			  m_baked_model_path(path) {
		}
		explicit skinned_model(std::string_view name, std::vector<skinned_mesh_data> meshes);

		auto load(asset::load_ctx& ctx) -> async::task<>;
		auto unload() -> void;

		auto meshes() const -> std::span<const skinned_mesh>;
		auto center_of_mass() const -> vec3<length>;

		auto uploads_ready() const -> bool;

	private:
		std::vector<skinned_mesh> m_meshes;
		std::filesystem::path m_baked_model_path;
		vec3<length> m_center_of_mass;
	};

	auto bake(const std::filesystem::path& src, skinned_model::baked& out) -> bool;
}

gse::skinned_model::skinned_model(const std::string_view name, std::vector<skinned_mesh_data> meshes)
	: identifiable(name) {
	m_meshes.reserve(meshes.size());
	for (auto& mesh_data : meshes) {
		m_meshes.emplace_back(std::move(mesh_data));
	}
}

auto gse::bake(const std::filesystem::path& src, skinned_model::baked& out) -> bool {
	std::ifstream file(src, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		return false;
	}
	const auto size = file.tellg();
	file.seekg(0);
	out.bytes.storage.resize(size);
	file.read(reinterpret_cast<char*>(out.bytes.storage.data()), size);
	return true;
}

auto gse::skinned_model::load(asset::load_ctx& ctx) -> async::task<> {
	if (!m_baked_model_path.empty()) {
		m_meshes.clear();

		const auto model_relative = m_baked_model_path.lexically_relative(config::baked_resource_path);
		auto texture_dir = model_relative.parent_path().string();
		std::ranges::replace(texture_dir, '\\', '/');
		if (texture_dir.starts_with("SkinnedModels")) {
			texture_dir = "Textures/" + texture_dir;
		}

		baked b{};
		if (load_baked(m_baked_model_path, b)) {
			const auto& bytes = b.bytes.storage;
			std::size_t pos = 0;
			auto read_into = [&](void* dst, const std::size_t n) {
				std::memcpy(dst, bytes.data() + pos, n);
				pos += n;
			};
			auto read_val = [&]<typename T>() -> T {
				T v;
				read_into(&v, sizeof(T));
				return v;
			};
			auto read_str = [&]() -> std::string {
				const auto len = read_val.template operator()<std::uint32_t>();
				std::string s(len, '\0');
				read_into(s.data(), len);
				return s;
			};

			char magic[4];
			read_into(magic, 4);

			if (std::memcmp(magic, "GSMD", 4) != 0) {
				co_return;
			}

			const auto version = read_val.template operator()<std::uint32_t>();
			const auto mesh_count = read_val.template operator()<std::uint32_t>();
			m_meshes.reserve(mesh_count);

			for (std::uint32_t m = 0; m < mesh_count; ++m) {
				gse::material mat;

				if (version >= 2) {
					mat.base_color = { read_val.template operator()<float>(),
									   read_val.template operator()<float>(),
									   read_val.template operator()<float>() };
					mat.roughness = read_val.template operator()<float>();
					mat.metallic = read_val.template operator()<float>();

					auto albedo_file = read_str();
					auto normal_file = read_str();
					auto rm_file = read_str();

					if (!albedo_file.empty()) {
						auto stem = std::filesystem::path(albedo_file).stem().string();
						mat.diffuse_texture = asset::get<texture>(ctx.assets, texture_dir + "/" + stem);
					}
					if (!normal_file.empty()) {
						auto stem = std::filesystem::path(normal_file).stem().string();
						mat.normal_texture = asset::get<texture>(ctx.assets, texture_dir + "/" + stem);
					}
					if (!rm_file.empty()) {
						auto stem = std::filesystem::path(rm_file).stem().string();
						mat.specular_texture = asset::get<texture>(ctx.assets, texture_dir + "/" + stem);
					}
				}
				else {
					read_str();
				}

				const auto vertex_count = read_val.template operator()<std::uint32_t>();

				std::vector<skinned_vertex> vertices(vertex_count);
				for (std::uint32_t v = 0; v < vertex_count; ++v) {
					float px = read_val.template operator()<float>(), py = read_val.template operator()<float>(),
						  pz = read_val.template operator()<float>();
					vertices[v].position = vec3<displacement>{ meters(px), meters(py), meters(pz) };

					float nx = read_val.template operator()<float>(), ny = read_val.template operator()<float>(),
						  nz = read_val.template operator()<float>();
					vertices[v].normal = vec3f{ nx, ny, nz };

					float u = read_val.template operator()<float>(), vt = read_val.template operator()<float>();
					vertices[v].tex_coords = vec2f{ u, vt };

					read_into(vertices[v].bone_indices.data(), 4 * sizeof(std::uint32_t));

					float w0 = read_val.template operator()<float>(), w1 = read_val.template operator()<float>();
					float w2 = read_val.template operator()<float>(), w3 = read_val.template operator()<float>();
					vertices[v].bone_weights = vec4f{ w0, w1, w2, w3 };
				}

				const auto index_count = read_val.template operator()<std::uint32_t>();
				std::vector<std::uint32_t> indices(index_count);
				read_into(indices.data(), index_count * sizeof(std::uint32_t));

				m_meshes.emplace_back(
					skinned_mesh_data{ .vertices = std::move(vertices),
									   .indices = std::move(indices),
									   .material = std::move(mat) }
				);
			}
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

auto gse::skinned_model::unload() -> void {
	m_meshes.clear();
}

auto gse::skinned_model::meshes() const -> std::span<const skinned_mesh> {
	return m_meshes;
}

auto gse::skinned_model::center_of_mass() const -> vec3<length> {
	return m_center_of_mass;
}

auto gse::skinned_model::uploads_ready() const -> bool {
	return std::ranges::all_of(m_meshes, [](const skinned_mesh& m) {
		return m.upload_token().ready() && m.material().textures_ready();
	});
}

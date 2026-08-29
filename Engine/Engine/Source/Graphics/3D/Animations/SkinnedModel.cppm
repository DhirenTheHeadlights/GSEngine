export module gse.graphics:skinned_model;

import std;

import :material;
import :mesh;
import :model;
import :source_reader;

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
import gse.physics;

export namespace gse {
	struct skinned_vertex {
		vec3<displacement> position;
		vec3f normal;
		vec2f tex_coords;
		std::array<std::uint8_t, 4> bone_slots{};
		vec4f bone_weights;
	};

	struct rig_bone {
		std::string name;
		std::uint16_t source_index = 0;
		std::uint16_t parent = std::numeric_limits<std::uint16_t>::max();
		vec3<displacement> joint_local_offset;
		quat joint_local_rotation = quat(1.f, 0.f, 0.f, 0.f);
		vec3<displacement> body_offset;
		quat body_rotation = quat(1.f, 0.f, 0.f, 0.f);
		mat4f inverse_bind;
		physics::collision_shape shape;
		mass mass;
	};

	struct rig_proxy {
		length radius;
		length half_height;
		vec3<displacement> center;
	};

	struct skinned_mesh_baked {
		model::material_baked material;
		raw_blob_owned<skinned_vertex> vertices;
		raw_blob_owned<std::uint32_t> indices;
	};

	struct [[= shaders::shader_struct]] skin_influence {
		vec4u bone_slots;
		vec4f bone_weights;
	};

	class skinned_model : public identifiable {
	public:
		static constexpr std::uint16_t no_parent = std::numeric_limits<std::uint16_t>::max();

		struct [[
			= asset_format::baked_ext<".gsmdl">{},
			= asset_format::baked_dir<"SkinnedModels">{},
			= asset_format::source_dir<"SkinnedModels">{},
			= asset_format::source_exts<".gsmdl">{},
			= asset_format::magic<0x47534D44>{},
			= asset_format::version<4>{}
		]] baked {
			std::vector<rig_bone> bones;
			rig_proxy proxy;
			std::vector<skinned_mesh_baked> meshes;
		};

		explicit skinned_model(const std::filesystem::path& path)
			: identifiable(config::asset_tag(path)), m_baked_path(path) {
		}

		auto load(
			asset::load_ctx& ctx
		) -> async::task<asset_result>;

		auto bones() const -> std::span<const rig_bone>;
		auto proxy() const -> const rig_proxy&;
		auto meshes() const -> std::span<const mesh>;
		auto influence_buffers() const -> std::span<const gpu::buffer>;
		auto uploads_ready() const -> bool;

	private:
		std::vector<rig_bone> m_bones;
		std::vector<mesh> m_meshes;
		std::vector<gpu::buffer> m_influence_buffers;
		rig_proxy m_proxy;
		std::filesystem::path m_baked_path;
	};

	auto bake(
		const std::filesystem::path& src,
		skinned_model::baked& out
	) -> bool;
}

auto gse::bake(const std::filesystem::path& src, skinned_model::baked& out) -> bool {
	auto reader = animation::source_reader::open(src, "GSMD");
	if (!reader) {
		return false;
	}

	if (reader->u32() < 3) {
		return false;
	}

	const auto bone_count = reader->u32();
	for (std::uint32_t i = 0; i < bone_count && !reader->overran(); ++i) {
		auto& bone = out.bones.emplace_back();
		bone.name = reader->string();
		bone.source_index = reader->u16();
		bone.parent = reader->u16();
		bone.joint_local_offset = reader->translation();
		bone.joint_local_rotation = reader->rotation();
		bone.body_offset = reader->translation();
		bone.body_rotation = reader->rotation();
		bone.inverse_bind = reader->matrix();
		bone.shape = reader->shape();
		bone.mass = kilograms(reader->f32());
	}

	out.proxy = {
		.radius = meters(reader->f32()),
		.half_height = meters(reader->f32()),
		.center = reader->translation(),
	};

	const auto mesh_count = reader->u32();
	for (std::uint32_t m = 0; m < mesh_count && !reader->overran(); ++m) {
		auto& mesh = out.meshes.emplace_back();
		mesh.material = {
			.base_color = reader->vec3(),
			.roughness = reader->f32(),
			.metallic = reader->f32(),
			.albedo_file = reader->string(),
			.normal_file = reader->string(),
			.rm_file = reader->string(),
		};

		const auto vertex_count = reader->u32();
		for (std::uint32_t v = 0; v < vertex_count && !reader->overran(); ++v) {
			auto& vertex = mesh.vertices.storage.emplace_back();
			vertex.position = reader->translation();
			vertex.normal = reader->vec3();
			vertex.tex_coords = reader->vec2();
			reader->bytes(vertex.bone_slots.data(), vertex.bone_slots.size());
			vertex.bone_weights = reader->vec4();
		}

		const auto index_count = reader->u32();
		if (index_count > reader->remaining() / sizeof(std::uint32_t)) {
			return false;
		}

		mesh.indices.storage.resize(index_count);
		reader->bytes(mesh.indices.storage.data(), mesh.indices.storage.size() * sizeof(std::uint32_t));
	}

	return !reader->overran();
}

auto gse::skinned_model::load(asset::load_ctx& ctx) -> async::task<asset_result> {
	if (m_baked_path.empty()) {
		co_return asset_result{};
	}

	auto loaded = load_baked<baked>(m_baked_path);
	if (!loaded) {
		co_return std::unexpected(std::move(loaded.error()));
	}

	m_bones = std::move(loaded->bones);
	m_proxy = loaded->proxy;

	m_meshes.clear();
	m_influence_buffers.clear();
	m_meshes.reserve(loaded->meshes.size());

	std::vector<std::vector<skin_influence>> influences;
	influences.reserve(loaded->meshes.size());

	for (auto& baked_mesh : loaded->meshes) {
		const auto& source = baked_mesh.vertices.storage;

		std::vector<vertex> vertices(source.size());
		std::vector<skin_influence> mesh_influences(source.size());

		for (std::size_t i = 0; i < source.size(); ++i) {
			vertices[i] = {
				.position = source[i].position,
				.normal = source[i].normal,
				.tex_coords = source[i].tex_coords,
			};
			mesh_influences[i] = {
				.bone_slots = vec4u(
					source[i].bone_slots[0],
					source[i].bone_slots[1],
					source[i].bone_slots[2],
					source[i].bone_slots[3]
				),
				.bone_weights = source[i].bone_weights,
			};
		}
		influences.push_back(std::move(mesh_influences));

		material mat{
			.base_color = baked_mesh.material.base_color,
			.roughness = baked_mesh.material.roughness,
			.metallic = baked_mesh.material.metallic,
		};
		mat.diffuse_texture = asset::try_get<texture>(ctx.assets, baked_mesh.material.albedo_file);
		mat.normal_texture = asset::try_get<texture>(ctx.assets, baked_mesh.material.normal_file);
		mat.specular_texture = asset::try_get<texture>(ctx.assets, baked_mesh.material.rm_file);

		m_meshes.emplace_back(std::move(vertices), std::move(baked_mesh.indices.storage), mat);
	}

	auto& gpu_s = co_await gpu::on_gpu(ctx.channels);
	for (auto& mesh : m_meshes) {
		mesh.initialize(gpu_s);
	}

	m_influence_buffers.reserve(influences.size());
	for (const auto& mesh_influences : influences) {
		m_influence_buffers.push_back(
			gpu_s.device->create_buffer(
				{
					.size = mesh_influences.size() * sizeof(skin_influence),
					.stride = sizeof(skin_influence),
					.usage = gpu::buffer_flag::storage,
					.data = mesh_influences.data(),
					.bindless = true,
				},
				"skinned_model.influences"
			)
		);
	}
	co_return asset_result{};
}

auto gse::skinned_model::bones() const -> std::span<const rig_bone> {
	return m_bones;
}

auto gse::skinned_model::proxy() const -> const rig_proxy& {
	return m_proxy;
}

auto gse::skinned_model::meshes() const -> std::span<const mesh> {
	return m_meshes;
}

auto gse::skinned_model::influence_buffers() const -> std::span<const gpu::buffer> {
	return m_influence_buffers;
}

auto gse::skinned_model::uploads_ready() const -> bool {
	return std::ranges::all_of(
		m_meshes,
		[](const mesh& m) {
			return m.upload_token().ready() && m.material().textures_ready();
		}
	);
}
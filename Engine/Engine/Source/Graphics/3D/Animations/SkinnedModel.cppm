export module gse.graphics:skinned_model;

import std;

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
		std::string material;
		raw_blob_owned<skinned_vertex> vertices;
		raw_blob_owned<std::uint32_t> indices;
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
			= asset_format::version<3>{}
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
		) -> async::task<>;

		auto unload() -> void;

		auto bones() const -> std::span<const rig_bone>;
		auto proxy() const -> const rig_proxy&;
		auto meshes() const -> std::span<const skinned_mesh_baked>;

		auto slot_of_source(
			std::uint16_t source_index
		) const -> std::uint16_t;

	private:
		std::vector<rig_bone> m_bones;
		std::vector<skinned_mesh_baked> m_meshes;
		std::flat_map<std::uint16_t, std::uint16_t> m_slot_by_source;
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

	out.bones.resize(reader->u32());
	for (auto& bone : out.bones) {
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

	out.meshes.resize(reader->u32());
	for (auto& mesh : out.meshes) {
		mesh.material = reader->string();

		mesh.vertices.storage.resize(reader->u32());
		for (auto& vertex : mesh.vertices.storage) {
			vertex.position = reader->translation();
			vertex.normal = vec3f(reader->f32(), reader->f32(), reader->f32());
			vertex.tex_coords = vec2f(reader->f32(), reader->f32());
			reader->bytes(vertex.bone_slots.data(), vertex.bone_slots.size());
			vertex.bone_weights = vec4f(reader->f32(), reader->f32(), reader->f32(), reader->f32());
		}

		mesh.indices.storage.resize(reader->u32());
		reader->bytes(mesh.indices.storage.data(), mesh.indices.storage.size() * sizeof(std::uint32_t));
	}

	return !reader->overran();
}

auto gse::skinned_model::load(asset::load_ctx& ctx) -> async::task<> {
	(void)ctx;

	if (m_baked_path.empty()) {
		co_return;
	}

	baked b{};
	if (!load_baked(m_baked_path, b)) {
		co_return;
	}

	m_bones = std::move(b.bones);
	m_meshes = std::move(b.meshes);
	m_proxy = b.proxy;

	m_slot_by_source.clear();
	for (std::size_t i = 0; i < m_bones.size(); ++i) {
		m_slot_by_source.insert_or_assign(m_bones[i].source_index, static_cast<std::uint16_t>(i));
	}
}

auto gse::skinned_model::unload() -> void {
	m_bones.clear();
	m_meshes.clear();
	m_slot_by_source.clear();
}

auto gse::skinned_model::bones() const -> std::span<const rig_bone> {
	return m_bones;
}

auto gse::skinned_model::proxy() const -> const rig_proxy& {
	return m_proxy;
}

auto gse::skinned_model::meshes() const -> std::span<const skinned_mesh_baked> {
	return m_meshes;
}

auto gse::skinned_model::slot_of_source(const std::uint16_t source_index) const -> std::uint16_t {
	const auto it = m_slot_by_source.find(source_index);
	if (it == m_slot_by_source.end()) {
		return no_parent;
	}
	return it->second;
}

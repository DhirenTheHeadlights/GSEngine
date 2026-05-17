export module gse.graphics:skeleton;

import std;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.config;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;

import :joint;

export namespace gse {
	class skeleton : public identifiable {
	public:
		struct params {
			std::string name;
			std::vector<joint> joints;
		};

		struct[[
			= asset_format::baked_ext<".gskel">{},
			= asset_format::baked_dir<"Skeletons">{},
			= asset_format::source_dir<"Skeletons">{},
			= asset_format::source_exts<".gskel">{},
			= asset_format::magic<0x47534B4C>{},
			= asset_format::version<1>{}
		]] baked {
			raw_blob_owned<std::byte> bytes;
		};

		explicit skeleton(
			const std::filesystem::path& path
		);

		explicit skeleton(
			const params& p
		);

		auto load(
			asset::load_ctx& ctx
		) -> async::task<>;

		auto unload() -> void;

		auto joint_count() const -> std::uint16_t;

		auto joints() const -> std::span<const joint>;

		auto joint(
			std::uint16_t index
		) const -> const joint&;

	private:
		std::vector<gse::joint> m_joints;
		std::filesystem::path m_baked_path;
	};

	auto bake(
		const std::filesystem::path& src,
		skeleton::baked& out
	) -> bool;
}

gse::skeleton::skeleton(const std::filesystem::path& path)
	: identifiable(path, config::baked_resource_path), m_baked_path(path) {
}

gse::skeleton::skeleton(const params& p)
	: identifiable(p.name), m_joints(p.joints) {
}

auto gse::bake(const std::filesystem::path& src, skeleton::baked& out) -> bool {
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

auto gse::skeleton::load(asset::load_ctx& ctx) -> async::task<> {
	(void)ctx;

	if (m_baked_path.empty()) {
		co_return;
	}

	baked b{};
	if (!load_baked(m_baked_path, b)) {
		co_return;
	}

	const auto& bytes = b.bytes.storage;
	std::size_t pos = 0;
	auto read_into = [&](void* dst, const std::size_t n) {
		std::memcpy(dst, bytes.data() + pos, n);
		pos += n;
	};

	char magic[4];
	read_into(magic, 4);
	if (std::memcmp(magic, "GSKL", 4) != 0) {
		co_return;
	}

	std::uint32_t version;
	read_into(&version, sizeof(version));

	std::uint32_t joint_count;
	read_into(&joint_count, sizeof(joint_count));

	m_joints.clear();
	m_joints.reserve(joint_count);

	for (std::uint32_t i = 0; i < joint_count; ++i) {
		std::uint32_t name_len;
		read_into(&name_len, sizeof(name_len));

		std::string name(name_len, '\0');
		read_into(name.data(), name_len);

		std::uint16_t parent_index;
		read_into(&parent_index, sizeof(parent_index));

		mat4f local_bind;
		for (int row = 0; row < 4; ++row) {
			for (int col = 0; col < 4; ++col) {
				float val;
				read_into(&val, sizeof(val));
				local_bind[col][row] = val;
			}
		}

		mat4f inverse_bind;
		for (int row = 0; row < 4; ++row) {
			for (int col = 0; col < 4; ++col) {
				float val;
				read_into(&val, sizeof(val));
				inverse_bind[col][row] = val;
			}
		}

		m_joints.emplace_back(joint::params{ .name = std::move(name), .parent_index = parent_index, .local_bind = local_bind, .inverse_bind = inverse_bind });
	}
}

auto gse::skeleton::unload() -> void {
	m_joints.clear();
}

auto gse::skeleton::joint_count() const -> std::uint16_t {
	return static_cast<std::uint16_t>(m_joints.size());
}

auto gse::skeleton::joints() const -> std::span<const gse::joint> {
	return m_joints;
}

auto gse::skeleton::joint(const std::uint16_t index) const -> const gse::joint& {
	return m_joints[index];
}

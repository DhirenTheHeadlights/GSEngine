export module gse.graphics:clip;

import std;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import :skeleton;

export namespace gse {
	struct joint_keyframe {
		time time;
		mat4f local_transform;
	};

	struct joint_track {
		std::uint16_t joint_index;
		std::vector<joint_keyframe> keys;
	};

	class clip_asset : public identifiable {
	public:
		struct params {
			std::string name;
			time length;
			bool loop;
			std::vector<joint_track> tracks;
		};

		struct[[
			= asset_format::baked_ext<".gclip">{},
			= asset_format::baked_dir<"Clips">{},
			= asset_format::source_dir<"Clips">{},
			= asset_format::source_exts<".gclip">{},
			= asset_format::magic<0x47434C50>{},
			= asset_format::version<1>{}
		]] baked {
			raw_blob_owned<std::byte> bytes;
		};

		explicit clip_asset(
			const std::filesystem::path& path
		);

		explicit clip_asset(
			params p
		);

		auto load(
			asset::load_ctx& ctx
		) -> async::task<>;

		auto unload() -> void;

		auto length() const -> time;

		auto loop() const -> bool;

		auto tracks() const -> std::span<const joint_track>;

	private:
		time m_length{};
		bool m_loop = true;
		std::vector<joint_track> m_tracks;
		std::filesystem::path m_baked_path;
	};

	auto bake(
		const std::filesystem::path& src,
		clip_asset::baked& out
	) -> bool;
}

gse::clip_asset::clip_asset(const std::filesystem::path& path)
	: identifiable(path, config::baked_resource_path), m_baked_path(path) {
}

gse::clip_asset::clip_asset(params p)
	: identifiable(p.name),
	  m_length(p.length),
	  m_loop(p.loop),
	  m_tracks(std::move(p.tracks)) {
}

auto gse::bake(const std::filesystem::path& src, clip_asset::baked& out) -> bool {
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

auto gse::clip_asset::load(asset::load_ctx& ctx) -> async::task<> {
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
	if (std::memcmp(magic, "GCLP", 4) != 0) {
		co_return;
	}

	std::uint32_t version;
	read_into(&version, sizeof(version));

	std::uint32_t name_len;
	read_into(&name_len, sizeof(name_len));
	std::string name(name_len, '\0');
	read_into(name.data(), name_len);

	float length_seconds;
	read_into(&length_seconds, sizeof(length_seconds));
	m_length = seconds(length_seconds);

	std::uint8_t loop_byte;
	read_into(&loop_byte, sizeof(loop_byte));
	m_loop = loop_byte != 0;

	std::uint32_t track_count;
	read_into(&track_count, sizeof(track_count));

	m_tracks.clear();
	m_tracks.reserve(track_count);

	for (std::uint32_t t = 0; t < track_count; ++t) {
		joint_track track;

		read_into(&track.joint_index, sizeof(track.joint_index));

		std::uint32_t key_count;
		read_into(&key_count, sizeof(key_count));

		track.keys.reserve(key_count);

		for (std::uint32_t k = 0; k < key_count; ++k) {
			float key_time_seconds;
			read_into(&key_time_seconds, sizeof(key_time_seconds));

			mat4f local_transform;
			for (int row = 0; row < 4; ++row) {
				for (int col = 0; col < 4; ++col) {
					float val;
					read_into(&val, sizeof(val));
					local_transform[col][row] = val;
				}
			}

			track.keys.push_back(joint_keyframe{ .time = seconds(key_time_seconds), .local_transform = local_transform });
		}

		m_tracks.push_back(std::move(track));
	}
}

auto gse::clip_asset::unload() -> void {
	m_tracks.clear();
}

auto gse::clip_asset::length() const -> time {
	return m_length;
}

auto gse::clip_asset::loop() const -> bool {
	return m_loop;
}

auto gse::clip_asset::tracks() const -> std::span<const joint_track> {
	return m_tracks;
}

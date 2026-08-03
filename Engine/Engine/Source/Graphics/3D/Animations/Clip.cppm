export module gse.graphics:clip;

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

export namespace gse {
	struct joint_pose {
		vec3<displacement> translation;
		quat rotation = quat(1.f, 0.f, 0.f, 0.f);
	};

	struct joint_keyframe {
		time stamp;
		joint_pose pose;
	};

	struct joint_track {
		std::uint16_t joint_index = 0;
		raw_blob_owned<joint_keyframe> keys;
	};

	class clip_asset : public identifiable {
	public:
		struct [[
			= asset_format::baked_ext<".gclip">{},
			= asset_format::baked_dir<"Clips">{},
			= asset_format::source_dir<"Clips">{},
			= asset_format::source_exts<".gclip">{},
			= asset_format::magic<0x47434C50>{},
			= asset_format::version<2>{}
		]] baked {
			time length;
			bool loops = true;
			std::vector<joint_track> tracks;
		};

		explicit clip_asset(const std::filesystem::path& path)
			: identifiable(config::asset_tag(path)), m_baked_path(path) {
		}

		auto load(
			asset::load_ctx& ctx
		) -> async::task<>;

		auto unload() -> void;

		auto length() const -> time;
		auto loops() const -> bool;
		auto tracks() const -> std::span<const joint_track>;

		auto track_for(
			std::uint16_t joint_index
		) const -> const joint_track*;

	private:
		std::vector<joint_track> m_tracks;
		std::flat_map<std::uint16_t, std::uint32_t> m_track_by_joint;
		std::filesystem::path m_baked_path;
		time m_length;
		bool m_loops = true;
	};

	auto sample(
		const joint_track& track,
		time at
	) -> joint_pose;

	auto bake(
		const std::filesystem::path& src,
		clip_asset::baked& out
	) -> bool;
}

auto gse::bake(const std::filesystem::path& src, clip_asset::baked& out) -> bool {
	auto reader = animation::source_reader::open(src, "GCLP");
	if (!reader) {
		return false;
	}

	reader->u32();
	reader->string();
	out.length = seconds(reader->f32());
	out.loops = reader->u8() != 0;

	const auto track_count = reader->u32();
	out.tracks.resize(track_count);

	for (auto& track : out.tracks) {
		track.joint_index = reader->u16();
		track.keys.storage.resize(reader->u32());
		for (auto& key : track.keys.storage) {
			key.stamp = seconds(reader->f32());
			const auto local = reader->matrix();
			key.pose = {
				.translation = vec3<displacement>(meters(local[3][0]), meters(local[3][1]), meters(local[3][2])),
				.rotation = normalize(from_mat4(local)),
			};
		}
	}

	return !reader->overran();
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

	m_length = b.length;
	m_loops = b.loops;
	m_tracks = std::move(b.tracks);

	m_track_by_joint.clear();
	for (std::size_t i = 0; i < m_tracks.size(); ++i) {
		m_track_by_joint.insert_or_assign(m_tracks[i].joint_index, static_cast<std::uint32_t>(i));
	}
}

auto gse::clip_asset::unload() -> void {
	m_tracks.clear();
	m_track_by_joint.clear();
}

auto gse::clip_asset::length() const -> time {
	return m_length;
}

auto gse::clip_asset::loops() const -> bool {
	return m_loops;
}

auto gse::clip_asset::tracks() const -> std::span<const joint_track> {
	return m_tracks;
}

auto gse::clip_asset::track_for(const std::uint16_t joint_index) const -> const joint_track* {
	const auto it = m_track_by_joint.find(joint_index);
	if (it == m_track_by_joint.end()) {
		return nullptr;
	}
	return &m_tracks[it->second];
}

auto gse::sample(const joint_track& track, const time at) -> joint_pose {
	const auto keys = std::span(track.keys.storage);
	if (keys.empty()) {
		return {};
	}
	if (keys.size() == 1 || at <= keys.front().stamp) {
		return keys.front().pose;
	}
	if (at >= keys.back().stamp) {
		return keys.back().pose;
	}

	const auto upper = std::ranges::upper_bound(keys, at, {}, &joint_keyframe::stamp);
	const auto& before = *(upper - 1);
	const auto& after = *upper;

	const auto span = after.stamp - before.stamp;
	const float alpha = span > time{} ? (at - before.stamp) / span : 0.f;

	return {
		.translation = before.pose.translation + (after.pose.translation - before.pose.translation) * alpha,
		.rotation = normalize(slerp(before.pose.rotation, after.pose.rotation, alpha)),
	};
}

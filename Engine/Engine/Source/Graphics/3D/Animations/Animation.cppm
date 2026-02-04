export module gse.graphics:animation;

import std;

import gse.utility;
import gse.physics.math;

import :animation_component;
import :clip_component;
import :clip;
import :skeleton;
import :renderer;

namespace gse::animation {
	struct anim_job {
		animation_component* anim = nullptr;
		clip_component* clip = nullptr;
		const skeleton* skel = nullptr;
		const clip_asset* clip_asset = nullptr;
		float scale = 1.f;
		bool loop = true;
		time sample_t{};
	};

	struct pose_cache_key {
		const clip_asset* clip = nullptr;
		const skeleton* skel = nullptr;
		std::int64_t time_bucket = 0;

		auto operator==(const pose_cache_key&) const -> bool = default;
	};

	struct pose_cache_key_hash {
		auto operator()(const pose_cache_key& k) const -> std::size_t {
			return std::hash<const void*>{}(k.clip) ^
			       (std::hash<const void*>{}(k.skel) << 1) ^
			       (std::hash<std::int64_t>{}(k.time_bucket) << 2);
		}
	};

	auto wrap_time(time t, time length) -> time;
	auto lerp_mat4(const mat4& a, const mat4& b, float t) -> mat4;
	auto sample_track(const joint_track& track, time t, mat4& out) -> bool;
	auto ensure_pose_buffers(animation_component& anim, std::size_t joint_count) -> void;
	auto build_local_pose(animation_component& anim, const skeleton& skeleton, const clip_asset& clip, time t) -> void;
	auto build_global_and_skins(animation_component& anim, const skeleton& skeleton) -> void;
}

export namespace gse::animation {
	struct state {
		time last_tick{};
		mutable std::vector<anim_job> jobs;
		mutable std::unordered_map<pose_cache_key, std::size_t, pose_cache_key_hash> pose_cache;
	};

	struct system {
		static auto initialize(initialize_phase& phase, state& s) -> void;
		static auto update(update_phase& phase, state& s) -> void;
	};
}

auto gse::animation::system::initialize(initialize_phase&, state& s) -> void {
	s.last_tick = system_clock::now();
}

auto gse::animation::system::update(update_phase& phase, state& s) -> void {
	const time dt = system_clock::dt();

	auto animations = phase.registry.chunk<animation_component>();
	auto clips = phase.registry.chunk<clip_component>();

	const auto* renderer_state = phase.try_state_of<renderer::state>();
	if (!renderer_state) {
		return;
	}

	s.jobs.clear();
	s.pose_cache.clear();

	for (std::size_t i = 0; i < animations.size(); ++i) {
		auto& anim = animations[i];

		auto* clip_c = phase.registry.try_write<clip_component>(anim.owner_id());
		if (clip_c == nullptr) {
			continue;
		}

		if (const auto& [skeleton_id] = anim.networked_data(); !anim.skeleton && skeleton_id.exists()) {
			anim.skeleton = renderer_state->get<skeleton>(skeleton_id);
		}

		if (!anim.skeleton) {
			continue;
		}

		const auto& [clip_id, scale, loop] = clip_c->networked_data();
		if (!clip_c->clip && clip_id.exists()) {
			clip_c->clip = renderer_state->get<clip_asset>(clip_id);
		}

		if (!clip_c->clip) {
			continue;
		}

		const auto& skel = *anim.skeleton;
		const auto& clip = *clip_c->clip;

		const auto joint_count = static_cast<std::size_t>(skel.joint_count());
		ensure_pose_buffers(anim, joint_count);

		if (clip_c->playing) {
			clip_c->t += dt * scale;
		}

		const time length = clip.length();
		time sample_t = clip_c->t;
		const bool should_loop = (loop && clip.loop());

		if (should_loop) {
			sample_t = wrap_time(sample_t, length);
			clip_c->t = sample_t;
		} else if (length > 0 && sample_t >= length) {
			sample_t = length;
			clip_c->t = length;
			clip_c->playing = false;
		}

		s.jobs.push_back({
			.anim = std::addressof(anim),
			.clip = clip_c,
			.skel = std::addressof(skel),
			.clip_asset = std::addressof(clip),
			.scale = scale,
			.loop = should_loop,
			.sample_t = sample_t
		});
	}

	if (s.jobs.empty()) {
		return;
	}

	std::vector<std::size_t> job_cache_index(s.jobs.size());
	std::vector<std::size_t> unique_job_indices;

	for (std::size_t i = 0; i < s.jobs.size(); ++i) {
		constexpr float time_bucket_size = 16'666'666.f;
		const auto& job = s.jobs[i];
		const auto time_bucket = static_cast<std::int64_t>(job.sample_t / time{time_bucket_size});

		const pose_cache_key key{
			.clip = job.clip_asset,
			.skel = job.skel,
			.time_bucket = time_bucket
		};

		const auto [it, inserted] = s.pose_cache.try_emplace(key, i);
		job_cache_index[i] = it->second;

		if (inserted) {
			unique_job_indices.push_back(i);
		}
	}

	task::parallel_for(0uz, unique_job_indices.size(), [&](const std::size_t i) {
		const auto job_idx = unique_job_indices[i];
		const auto& job = s.jobs[job_idx];
		build_local_pose(*job.anim, *job.skel, *job.clip_asset, job.sample_t);
		build_global_and_skins(*job.anim, *job.skel);
	});

	task::parallel_for(0uz, s.jobs.size(), [&](const std::size_t i) {
		if (const auto source_idx = job_cache_index[i]; source_idx != i) {
			const auto& source_anim = *s.jobs[source_idx].anim;
			auto& dest_anim = *s.jobs[i].anim;

			std::ranges::copy(source_anim.local_pose, dest_anim.local_pose.begin());
			std::ranges::copy(source_anim.global_pose, dest_anim.global_pose.begin());
			std::ranges::copy(source_anim.skins, dest_anim.skins.begin());
		}
	});
}

auto gse::animation::wrap_time(const time t, const time length) -> time {
	if (length <= time{}) {
		return 0;
	}

	const float ratio = t / length;
	const float wrapped = ratio - std::floor(ratio);
	return length * wrapped;
}

auto gse::animation::lerp_mat4(const mat4& a, const mat4& b, const float t) -> mat4 {
	if constexpr (requires { a + (b - a) * t; }) {
		return a + (b - a) * t;
	}
	else {
		return t < 0.5f ? a : b;
	}
}

auto gse::animation::sample_track(const joint_track& track, const time t, mat4& out) -> bool {
	const auto key_count = track.keys.size();
	if (key_count == 0) {
		return false;
	}

	if (key_count == 1 || t <= track.keys.front().time) {
		out = track.keys.front().local_transform;
		return true;
	}

	if (t >= track.keys.back().time) {
		out = track.keys.back().local_transform;
		return true;
	}

	std::size_t lo = 0;
	std::size_t hi = key_count - 1;
	while (lo + 1 < hi) {
		if (const std::size_t mid = (lo + hi) / 2; track.keys[mid].time <= t) {
			lo = mid;
		} else {
			hi = mid;
		}
	}

	const auto& prev = track.keys[lo];
	const auto& next = track.keys[hi];

	const auto denom = next.time - prev.time;
	if (denom <= 0.f) {
		out = next.local_transform;
		return true;
	}

	const float alpha = (t - prev.time) / denom;
	const auto& a = prev.local_transform;
	const auto& b = next.local_transform;

	for (int col = 0; col < 4; ++col) {
		for (int row = 0; row < 4; ++row) {
			out[col][row] = a[col][row] + (b[col][row] - a[col][row]) * alpha;
		}
	}
	return true;
}

auto gse::animation::ensure_pose_buffers(animation_component& anim, const std::size_t joint_count) -> void {
	if (anim.local_pose.size() != joint_count) {
		anim.local_pose.resize(joint_count);
		anim.global_pose.resize(joint_count);
		anim.skins.resize(joint_count);
	}
}

auto gse::animation::build_local_pose(animation_component& anim, const skeleton& skeleton, const clip_asset& clip, const time t) -> void {
	const auto joint_count = static_cast<std::size_t>(skeleton.joint_count());
	const auto joints = skeleton.joints();

	for (std::size_t i = 0; i < joint_count; ++i) {
		anim.local_pose[i] = joints[i].local_bind();
	}

	mat4 sampled;
	for (const auto& track : clip.tracks()) {
		if (const auto idx = static_cast<std::size_t>(track.joint_index); idx < joint_count && sample_track(track, t, sampled)) {
			anim.local_pose[idx] = sampled;
		}
	}
}

auto gse::animation::build_global_and_skins(animation_component& anim, const skeleton& skeleton) -> void {
	const auto joint_count = static_cast<std::size_t>(skeleton.joint_count());
	const auto joints = skeleton.joints();
	constexpr auto invalid = std::numeric_limits<std::uint16_t>::max();

	for (std::size_t i = 0; i < joint_count; ++i) {
		const auto& jnt = joints[i];

		if (const auto parent = jnt.parent_index(); parent == invalid) {
			anim.global_pose[i] = anim.local_pose[i];
		}
		else {
			anim.global_pose[i] = anim.global_pose[static_cast<std::size_t>(parent)] * anim.local_pose[i];
		}

		anim.skins[i] = anim.global_pose[i] * jnt.inverse_bind();
	}
}

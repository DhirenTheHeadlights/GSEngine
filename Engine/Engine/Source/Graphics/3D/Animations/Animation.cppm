export module gse.graphics:animation;

import std;

import gse.utility;
import gse.physics.math;

import :animation_component;
import :clip_component;
import :clip;
import :skeleton;
import :renderer;

export namespace gse::animation {
	class system final : public gse::system {
	public:
		auto initialize(
		) -> void override;

		auto update(
		) -> void override;
	private:
		struct anim_job {
			animation_component* anim = nullptr;
			clip_component* clip = nullptr;
			const skeleton* skel = nullptr;
			const clip_asset* clip_asset = nullptr;
			float scale = 1.f;
			bool loop = true;
		};

		time m_last_tick{};

		auto tick_animations(
			const component_chunk<animation_component>& animations,
			const component_chunk<clip_component>& clips,
			time dt
		) const -> void;

		static auto wrap_time(
			time t,
			time length
		) -> time;

		static auto lerp_mat4(
			const mat4& a,
			const mat4& b,
			float t
		) -> mat4;

		static auto sample_track(
			const joint_track& track,
			time t
		) -> std::optional<mat4>;

		static auto ensure_pose_buffers(
			animation_component& anim,
			std::size_t joint_count
		) -> void;

		static auto build_local_pose(
			animation_component& anim,
			const skeleton& skeleton,
			const clip_asset& clip,
			time t
		) -> void;

		static auto build_global_and_skins(
			animation_component& anim,
			const skeleton& skeleton
		) -> void;
	};
}

auto gse::animation::system::initialize() -> void {
	m_last_tick = system_clock::now();
}

auto gse::animation::system::update() -> void {
	const time dt = system_clock::dt();

	write([this, dt](
		const component_chunk<animation_component>& anims,
		const component_chunk<clip_component>& clips
	) {
		tick_animations(anims, clips, dt);
	});
}

auto gse::animation::system::tick_animations(const component_chunk<animation_component>& animations, const component_chunk<clip_component>& clips, const time dt) const -> void {
	auto& renderer = const_cast<renderer::system&>(system_of<renderer::system>());

	std::vector<anim_job> jobs;
	jobs.reserve(animations.size());

	for (std::size_t i = 0; i < animations.size(); ++i) {
		auto& anim = animations[i];

		auto* clip_c = clips.write(anim.owner_id());
		if (clip_c == nullptr) {
			continue;
		}

		if (const auto& [skeleton_id] = anim.networked_data(); !anim.skeleton && skeleton_id.exists()) {
			anim.skeleton = renderer.get<skeleton>(skeleton_id);
		}

		if (!anim.skeleton) {
			continue;
		}

		const auto& [clip_id, scale, loop] = clip_c->networked_data();
		if (!clip_c->clip && clip_id.exists()) {
			clip_c->clip = renderer.get<clip_asset>(clip_id);
		}

		if (!clip_c->clip) {
			continue;
		}

		const auto& skeleton = *anim.skeleton;
		const auto& clip = *clip_c->clip;

		const auto joint_count = static_cast<std::size_t>(skeleton.joint_count());
		ensure_pose_buffers(anim, joint_count);

		jobs.push_back({
			.anim = std::addressof(anim),
			.clip = clip_c,
			.skel = std::addressof(skeleton),
			.clip_asset = std::addressof(clip),
			.scale = scale,
			.loop = loop && clip.loop()
		});
	}

	if (jobs.empty()) {
		return;
	}

	task::parallel_for(0uz, jobs.size(), [&](const std::size_t i) {
		auto& [anim, clip_c, skeleton, clip_asset, scale, loop] = jobs[i];

		if (clip_c->playing) {
			clip_c->t += dt * scale;
		}

		const time length = clip_asset->length();
		time sample_t = clip_c->t;

		if (loop) {
			sample_t = wrap_time(sample_t, length);
			clip_c->t = sample_t;
		} else if (length > time{} && sample_t >= length) {
			sample_t = length;
			clip_c->t = length;
			clip_c->playing = false;
		}

		build_local_pose(*anim, *skeleton, *clip_asset, sample_t);
		build_global_and_skins(*anim, *skeleton);
	});
}

auto gse::animation::system::wrap_time(time t, const time length) -> time {
	if (length <= time{}) {
		return time{};
	}

	while (t >= length) {
		t -= length;
	}

	while (t < time{}) {
		t += length;
	}

	return t;
}

auto gse::animation::system::lerp_mat4(const mat4& a, const mat4& b, const float t) -> mat4 {
	if constexpr (requires { a + (b - a) * t; }) {
		return a + (b - a) * t;
	}
	else {
		return t < 0.5f ? a : b;
	}
}

auto gse::animation::system::sample_track(const joint_track& track, const time t) -> std::optional<mat4> {
	if (track.keys.empty()) {
		return std::nullopt;
	}

	if (t <= track.keys.front().time) {
		return track.keys.front().local_transform;
	}

	if (t >= track.keys.back().time) {
		return track.keys.back().local_transform;
	}

	const auto it = std::lower_bound(
		track.keys.begin(),
		track.keys.end(),
		t,
		[](const joint_keyframe& k, const time v) {
		return k.time < v;
	}
	);

	if (it == track.keys.begin()) {
		return it->local_transform;
	}

	const auto& [time, local_transform] = *it;
	const auto& [prev_time, prev_local_transform] = *(it - 1);

	const auto denominator = time - prev_time;
	if (denominator <= 0) {
		return local_transform;
	}

	const float alpha = std::clamp(
		(t - prev_time) / denominator,
		0.f,
		1.f
	);

	return lerp_mat4(prev_local_transform, local_transform, alpha);
}

auto gse::animation::system::ensure_pose_buffers(animation_component& anim, const std::size_t joint_count) -> void {
	if (anim.local_pose.size() != joint_count) {
		anim.local_pose.resize(joint_count);
		anim.global_pose.resize(joint_count);
		anim.skins.resize(joint_count);
	}
}

auto gse::animation::system::build_local_pose(animation_component& anim, const skeleton& skeleton, const clip_asset& clip, const time t) -> void {
	const auto joint_count = static_cast<std::size_t>(skeleton.joint_count());

	for (std::size_t i = 0; i < joint_count; ++i) {
		anim.local_pose[i] = skeleton.joint(static_cast<std::uint16_t>(i)).local_bind();
	}

	for (const auto& track : clip.tracks()) {
		const auto idx = static_cast<std::size_t>(track.joint_index);
		if (idx >= joint_count) {
			continue;
		}

		if (const auto sampled = sample_track(track, t)) {
			anim.local_pose[idx] = *sampled;
		}
	}
}

auto gse::animation::system::build_global_and_skins(animation_component& anim, const skeleton& skeleton) -> void {
	const auto joint_count = static_cast<std::size_t>(skeleton.joint_count());
	constexpr auto invalid = std::numeric_limits<std::uint16_t>::max();

	for (std::size_t i = 0; i < joint_count; ++i) {
		const auto joint_index = static_cast<std::uint16_t>(i);

		if (const auto parent = skeleton.joint(joint_index).parent_index(); parent == invalid) {
			anim.global_pose[i] = anim.local_pose[i];
		}
		else {
			anim.global_pose[i] = anim.global_pose[static_cast<std::size_t>(parent)] * anim.local_pose[i];
		}

		anim.skins[i] = anim.global_pose[i] * skeleton.joint(joint_index).inverse_bind();
	}
}


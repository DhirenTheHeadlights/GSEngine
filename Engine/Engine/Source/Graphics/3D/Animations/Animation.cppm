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
	struct set_skeleton_request {
		id owner_id;
		id skeleton_id;
	};

	struct set_clip_request {
		id owner_id;
		id clip_id;
		float scale = 1.f;
		bool loop = true;
		bool play = true;
		bool reset_time = true;
	};

	struct play_request {
		id owner_id;
		bool playing = true;
	};

	struct stop_request {
		id owner_id;
	};

	struct seek_request {
		id owner_id;
		time t = 0.f;
		bool normalized = false;
	};

	class system final : public gse::system {
	public:
		system();

		auto initialize(
		) -> void override;

		auto update(
		) -> void override;
	private:
		struct frame_requests {
			std::vector<set_skeleton_request> set_skeleton;
			std::vector<set_clip_request> set_clip;
			std::vector<play_request> play;
			std::vector<stop_request> stop;
			std::vector<seek_request> seek;
		};

		struct anim_job {
			animation_component* anim = nullptr;
			clip_component* clip = nullptr;
			const skeleton* skel = nullptr;
			const clip_asset* clip_asset = nullptr;
			float scale = 1.f;
			bool loop = true;
		};

		time m_last_tick{};

		auto gather_requests(
		) const -> frame_requests;

		auto apply_requests(
			const component_chunk<animation_component>& anims,
			const component_chunk<clip_component>& clips,
			const frame_requests& requirements
		) -> void;

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

gse::animation::system::system() = default;

auto gse::animation::system::initialize() -> void {
	m_last_tick = system_clock::now();
}

auto gse::animation::system::gather_requests() const -> frame_requests {
	frame_requests out;

	for (const auto& r : channel_of<set_skeleton_request>()) {
		out.set_skeleton.push_back(r);
	}

	for (const auto& r : channel_of<set_clip_request>()) {
		out.set_clip.push_back(r);
	}

	for (const auto& r : channel_of<play_request>()) {
		out.play.push_back(r);
	}

	for (const auto& r : channel_of<stop_request>()) {
		out.stop.push_back(r);
	}

	for (const auto& r : channel_of<seek_request>()) {
		out.seek.push_back(r);
	}

	return out;
}

auto gse::animation::system::update() -> void {
	const time dt = system_clock::dt();
	frame_requests requirements = gather_requests();

	write([this, dt, requirements = std::move(requirements)](
		const component_chunk<animation_component>& anims,
		const component_chunk<clip_component>& clips
	) {
		apply_requests(anims, clips, requirements);
		tick_animations(anims, clips, dt);
	});
}

auto gse::animation::system::apply_requests(const component_chunk<animation_component>& anims, const component_chunk<clip_component>& clips, const frame_requests& requirements) -> void {
	for (const auto& [owner_id, req_skeleton_id] : requirements.set_skeleton) {
		if (!owner_id.exists()) {
			continue;
		}

		auto* anim = anims.write(owner_id);
		if (anim == nullptr) {
			defer_add<animation_component>(owner_id, animation_component_data{
				.skeleton_id = req_skeleton_id
			});
			continue;
		}

		if (auto& [skeleton_id] = anim->networked_data(); skeleton_id != req_skeleton_id) {
			skeleton_id = req_skeleton_id;
			anim->skeleton = {};
			anim->local_pose.clear();
			anim->global_pose.clear();
			anim->skins.clear();
		}
	}

	for (const auto& [owner_id, clip_id, scale, loop, play, reset_time] : requirements.set_clip) {
		if (!owner_id.exists()) {
			continue;
		}

		auto* clip_c = clips.write(owner_id);
		if (clip_c == nullptr) {
			defer_add<clip_component>(owner_id, clip_component_data{
				.clip_id = clip_id,
				.scale = scale,
				.loop = loop
			});
			continue;
		}

		auto& [net_clip_id, net_scale, net_loop] = clip_c->networked_data();

		if (net_clip_id != clip_id) {
			net_clip_id = clip_id;
			clip_c->clip = {};
		}

		net_scale = scale;
		net_loop = loop;

		if (reset_time) {
			clip_c->t = 0.f;
		}

		clip_c->playing = play;
	}

	for (const auto& [owner_id, playing] : requirements.play) {
		if (!owner_id.exists()) {
			continue;
		}

		if (auto* clip_c = clips.write(owner_id)) {
			clip_c->playing = playing;
		}
	}

	for (const auto& [owner_id] : requirements.stop) {
		if (!owner_id.exists()) {
			continue;
		}

		if (auto* clip_c = clips.write(owner_id)) {
			clip_c->t = 0.f;
			clip_c->playing = false;
		}
	}

	for (const auto& [owner_id, t, normalized] : requirements.seek) {
		if (!owner_id.exists()) {
			continue;
		}

		auto* clip_c = clips.write(owner_id);
		if (clip_c == nullptr) {
			continue;
		}

		time dst = t;

		if (normalized) {
			if (const auto& data = clip_c->networked_data(); data.clip_id.exists()) {
				if (!clip_c->clip) {
					auto& renderer = const_cast<renderer::system&>(system_of<renderer::system>());
					clip_c->clip = renderer.get<clip_asset>(data.clip_id);
				}

				if (clip_c->clip) {
					if (const auto len = clip_c->clip->length(); len > 0) {
						dst = seconds(t.as<seconds>() * len.as<seconds>());
					}
				}
			}
		}

		clip_c->t = dst;
	}
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


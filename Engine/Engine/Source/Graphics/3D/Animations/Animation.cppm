export module gse.graphics:animation;

import std;

import gse.utility;
import gse.physics.math;

import :animation_component;
import :clip_component;
import :controller_component;
import :animation_graph;
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

	struct controller_job {
		animation_component* anim = nullptr;
		controller_component* ctrl = nullptr;
		const skeleton* skel = nullptr;
		const animation_graph* graph = nullptr;
	};

	struct pose_cache_key {
		const clip_asset* clip = nullptr;
		const skeleton* skel = nullptr;
		std::int64_t time_bucket = 0;

		auto operator==(
			const pose_cache_key&
		) const -> bool = default;
	};

	struct pose_cache_key_hash {
		auto operator()(const pose_cache_key& k) const -> std::size_t {
			return std::hash<const void*>{}(k.clip) ^
			       (std::hash<const void*>{}(k.skel) << 1) ^
			       (std::hash<std::int64_t>{}(k.time_bucket) << 2);
		}
	};

	auto wrap_time(
		time t, 
		time length
	) -> time;

	auto lerp_mat4(
		const mat4& a, 
		const mat4& b,
		float t
	) -> mat4;

	auto sample_track(
		const joint_track& track, 
		time t, 
		mat4& out
	) -> bool;

	auto ensure_pose_buffers(
		animation_component& anim, 
		std::size_t joint_count
	) -> void;

	auto build_local_pose(
		animation_component& anim, 
		const skeleton& skeleton, 
		const clip_asset& clip,
		time t
	) -> void;

	auto build_global_and_skins(
		animation_component& anim, 
		const skeleton& skeleton
	) -> void;

	auto sample_clip_to_pose(
		std::vector<mat4>& pose,
		const skeleton& skel,
		const clip_asset& clip,
		time t
	) -> void;

	auto blend_poses(
		std::vector<mat4>& out,
		const std::vector<mat4>& from,
		const std::vector<mat4>& to,
		float alpha
	) -> void;

	auto evaluate_condition(
		const transition_condition& condition,
		const std::unordered_map<std::string, animation_parameter>& params
	) -> bool;

	auto evaluate_transition(
		const animation_transition& transition,
		const std::unordered_map<std::string,
		animation_parameter>& params, 
		time state_time,
		time clip_length
	) -> bool;

	auto clear_triggers(
		std::unordered_map<std::string, animation_parameter>& params
	) -> void;

	auto process_controller_job(
		const controller_job& job,
		const renderer::state& renderer_state,
		time dt
	) -> void;
}

export namespace gse::animation {
	struct state {
		time last_tick{};
		std::vector<anim_job> jobs;
		std::vector<controller_job> controller_jobs;
		std::unordered_map<pose_cache_key, std::size_t, pose_cache_key_hash> pose_cache;
		std::unordered_map<id, animation_graph> graphs;
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
	const auto* renderer_state = phase.try_state_of<renderer::state>();
	if (!renderer_state) {
		return;
	}

	const time dt = system_clock::dt();

	phase.schedule([&s, renderer_state, dt](
		chunk<animation_component> animations,
		chunk<controller_component> controllers,
		chunk<clip_component> clips
	) {
		s.jobs.clear();
		s.controller_jobs.clear();
		s.pose_cache.clear();

		for (auto& anim : animations) {
			if (const auto& [skeleton_id] = anim.networked_data(); !anim.skeleton && skeleton_id.exists()) {
				anim.skeleton = renderer_state->get<skeleton>(skeleton_id);
			}

			if (!anim.skeleton) {
				continue;
			}

			const auto& skel = *anim.skeleton;
			const auto joint_count = static_cast<std::size_t>(skel.joint_count());
			ensure_pose_buffers(anim, joint_count);

			if (auto* ctrl_c = controllers.find(anim.owner_id())) {
				const auto& [graph_id] = ctrl_c->networked_data();
				if (const auto graph_it = s.graphs.find(graph_id); graph_it != s.graphs.end()) {
					s.controller_jobs.push_back({
						.anim = std::addressof(anim),
						.ctrl = ctrl_c,
						.skel = std::addressof(skel),
						.graph = &graph_it->second
					});
					continue;
				}
			}

			auto* clip_c = clips.find(anim.owner_id());
			if (clip_c == nullptr) {
				continue;
			}
			const auto& [clip_id, scale, loop] = clip_c->networked_data();
			if (!clip_c->clip && clip_id.exists()) {
				clip_c->clip = renderer_state->get<clip_asset>(clip_id);
			}

			if (!clip_c->clip) {
				continue;
			}

			const auto& clip = *clip_c->clip;

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

		if (!s.jobs.empty()) {
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

		if (!s.controller_jobs.empty()) {
			task::parallel_for(0uz, s.controller_jobs.size(), [&](const std::size_t i) {
				process_controller_job(s.controller_jobs[i], *renderer_state, dt);
			});
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
	const auto& a_mat = prev.local_transform;
	const auto& b_mat = next.local_transform;

	for (int col = 0; col < 4; ++col) {
		for (int row = 0; row < 4; ++row) {
			out[col][row] = a_mat[col][row] + (b_mat[col][row] - a_mat[col][row]) * alpha;
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

auto gse::animation::sample_clip_to_pose(std::vector<mat4>& pose, const skeleton& skel, const clip_asset& clip, const time t) -> void {
	const auto joint_count = static_cast<std::size_t>(skel.joint_count());
	const auto joints = skel.joints();

	if (pose.size() != joint_count) {
		pose.resize(joint_count);
	}

	for (std::size_t i = 0; i < joint_count; ++i) {
		pose[i] = joints[i].local_bind();
	}

	mat4 sampled;
	for (const auto& track : clip.tracks()) {
		if (const auto idx = static_cast<std::size_t>(track.joint_index); idx < joint_count && sample_track(track, t, sampled)) {
			pose[idx] = sampled;
		}
	}
}

auto gse::animation::blend_poses(std::vector<mat4>& out, const std::vector<mat4>& from, const std::vector<mat4>& to, const float alpha) -> void {
	const auto count = std::min(from.size(), to.size());
	if (out.size() != count) {
		out.resize(count);
	}

	for (std::size_t i = 0; i < count; ++i) {
		out[i] = lerp_mat4(from[i], to[i], alpha);
	}
}

auto gse::animation::evaluate_condition(const transition_condition& condition, const std::unordered_map<std::string, animation_parameter>& params) -> bool {
	const auto it = params.find(condition.parameter_name);
	if (it == params.end()) {
		return false;
	}

	const auto& [value, is_trigger] = it->second;

	switch (condition.type) {
	case transition_condition_type::bool_equals:
		if (const auto* val = std::get_if<bool>(&value)) {
			return *val == condition.bool_value;
		}
		return false;

	case transition_condition_type::float_greater:
		if (const auto* val = std::get_if<float>(&value)) {
			return *val > condition.threshold;
		}
		return false;

	case transition_condition_type::float_less:
		if (const auto* val = std::get_if<float>(&value)) {
			return *val < condition.threshold;
		}
		return false;

	case transition_condition_type::trigger:
		if (const auto* val = std::get_if<bool>(&value)) {
			return *val && is_trigger;
		}
		return false;
	}

	return false;
}

auto gse::animation::evaluate_transition(const animation_transition& transition, const std::unordered_map<std::string, animation_parameter>& params, const time state_time, const time clip_length) -> bool {
	if (transition.has_exit_time && clip_length > time{}) {
		if (const float normalized = state_time / clip_length; normalized < transition.exit_time_normalized) {
			return false;
		}
	}

	for (const auto& condition : transition.conditions) {
		if (!evaluate_condition(condition, params)) {
			return false;
		}
	}

	return true;
}

auto gse::animation::clear_triggers(std::unordered_map<std::string, animation_parameter>& params) -> void {
	for (auto& [value, is_trigger] : params | std::views::values) {
		if (is_trigger) {
			value = false;
		}
	}
}

auto gse::animation::process_controller_job(const controller_job& job, const renderer::state& renderer_state, const time dt) -> void {
	auto& anim = *job.anim;
	auto& ctrl = *job.ctrl;
	const auto& skel = *job.skel;
	const auto& graph = *job.graph;

	if (ctrl.current_state.empty()) {
		ctrl.current_state = graph.default_state;
		ctrl.state_time = time{};
	}

	const auto* current_state = graph.find_state(ctrl.current_state);
	if (!current_state) {
		return;
	}

	const auto current_clip_handle = renderer_state.get<clip_asset>(current_state->clip_id);
	if (!current_clip_handle) {
		return;
	}

	const auto& current_clip = *current_clip_handle;

	if (ctrl.state_playing) {
		ctrl.state_time += dt * current_state->speed;
	}

	const time clip_length = current_clip.length();
	if (current_state->loop && clip_length > time{}) {
		ctrl.state_time = wrap_time(ctrl.state_time, clip_length);
	}

	if (ctrl.blend.active) {
		ctrl.blend.blend_elapsed += dt;
		const float alpha = std::clamp(ctrl.blend.blend_elapsed / ctrl.blend.blend_duration, 0.f, 1.f);

		const auto* from_state = graph.find_state(ctrl.blend.from_state);
		const auto* to_state = graph.find_state(ctrl.blend.to_state);

		if (from_state && to_state) {
			const auto from_clip_handle = renderer_state.get<clip_asset>(from_state->clip_id);
			const auto to_clip_handle = renderer_state.get<clip_asset>(to_state->clip_id);

			if (from_clip_handle && to_clip_handle) {
				ctrl.blend.from_time += dt * from_state->speed;
				ctrl.blend.to_time += dt * to_state->speed;

				time from_sample = ctrl.blend.from_time;
				time to_sample = ctrl.blend.to_time;

				if (from_state->loop && from_clip_handle->length() > time{}) {
					from_sample = wrap_time(from_sample, from_clip_handle->length());
				}
				if (to_state->loop && to_clip_handle->length() > time{}) {
					to_sample = wrap_time(to_sample, to_clip_handle->length());
				}

				sample_clip_to_pose(ctrl.blend_from_pose, skel, *from_clip_handle, from_sample);
				sample_clip_to_pose(ctrl.blend_to_pose, skel, *to_clip_handle, to_sample);
				blend_poses(anim.local_pose, ctrl.blend_from_pose, ctrl.blend_to_pose, alpha);
			}
		}

		if (alpha >= 1.f) {
			ctrl.current_state = ctrl.blend.to_state;
			ctrl.state_time = ctrl.blend.to_time;
			ctrl.blend.active = false;
		}
	}
	else {
		for (const auto* transition : graph.find_transitions_from(ctrl.current_state)) {
			if (evaluate_transition(*transition, ctrl.parameters, ctrl.state_time, clip_length)) {
				ctrl.blend.active = true;
				ctrl.blend.from_state = ctrl.current_state;
				ctrl.blend.to_state = transition->to_state;
				ctrl.blend.blend_duration = transition->blend_duration;
				ctrl.blend.blend_elapsed = 0;
				ctrl.blend.from_time = ctrl.state_time;
				ctrl.blend.to_time = 0;
				break;
			}
		}

		if (!ctrl.blend.active) {
			sample_clip_to_pose(anim.local_pose, skel, current_clip, ctrl.state_time);
		}
	}

	clear_triggers(ctrl.parameters);
	build_global_and_skins(anim, skel);
}

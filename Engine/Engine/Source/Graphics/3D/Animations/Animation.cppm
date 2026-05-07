export module gse.graphics:animation;

import std;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.assets;

import :animation_component;
import :clip_component;
import :controller_component;
import :animation_graph;
import :clip;
import :skeleton;
import :renderer;

export namespace gse::animation {
	struct anim_job {
		animation_component* anim = nullptr;
		clip_component* clip = nullptr;
		const skeleton* skel = nullptr;
		const clip_asset* asset = nullptr;
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

		auto operator==(const pose_cache_key& other) const -> bool {
			return clip == other.clip && skel == other.skel && time_bucket == other.time_bucket;
		}
	};

	struct pose_cache_key_hash {
		auto operator()(const pose_cache_key& k) const -> std::size_t {
			return std::hash<const void*>{}(k.clip) ^
			       (std::hash<const void*>{}(k.skel) << 1) ^
			       (std::hash<std::int64_t>{}(k.time_bucket) << 2);
		}
	};

	struct system {
		struct state {
			time last_tick{};
			std::vector<anim_job> jobs;
			std::vector<controller_job> controller_jobs;
			std::unordered_map<pose_cache_key, std::size_t, pose_cache_key_hash> pose_cache;
			std::unordered_map<id, animation_graph> graphs;
		};

		static auto run(
			run_context& ctx,
			const asset::state& assets_s,
			state& s
		) -> async::task<>;

	private:
		static auto wrap_time(
			time t,
			time length
		) -> time;

		static auto lerp_mat4(
			const mat4f& a,
			const mat4f& b,
			float t
		) -> mat4f;

		static auto sample_track(
			const joint_track& track,
			time t,
			mat4f& out
		) -> bool;

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

		static auto sample_clip_to_pose(
			std::vector<mat4f>& pose,
			const skeleton& skel,
			const clip_asset& clip,
			time t
		) -> void;

		static auto blend_poses(
			std::vector<mat4f>& out,
			const std::vector<mat4f>& from,
			const std::vector<mat4f>& to,
			float alpha
		) -> void;

		static auto evaluate_condition(
			const transition_condition& condition,
			const std::unordered_map<std::string, animation_parameter>& params
		) -> bool;

		static auto evaluate_transition(
			const animation_transition& transition,
			const std::unordered_map<std::string, animation_parameter>& params,
			time state_time,
			time clip_length
		) -> bool;

		static auto clear_triggers(
			std::unordered_map<std::string, animation_parameter>& params
		) -> void;

		static auto process_controller_job(
			const controller_job& job,
			const asset::state& assets_s,
			time dt
		) -> void;
	};
}

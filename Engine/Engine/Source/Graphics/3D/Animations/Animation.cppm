export module gse.graphics:animation;

import std;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;

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

	struct state {
		time last_tick{};
		std::vector<anim_job> jobs;
		std::vector<controller_job> controller_jobs;
		std::unordered_map<pose_cache_key, std::size_t, pose_cache_key_hash> pose_cache;
		std::unordered_map<id, animation_graph> graphs;
	};

	struct system {
		static auto initialize(
			init_context& phase,
			state& s
		) -> void;

		static auto update(
			update_context& ctx,
			state& s
		) -> async::task<>;
	};
}

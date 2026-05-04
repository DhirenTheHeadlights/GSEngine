export module gse.graphics:renderer;

import std;

import :clip;
import :render_component;
import :font;
import :model;
import :skinned_model;
import :skeleton;
import :texture;

import :texture_compiler;
import :font_compiler;
import :skeleton_compiler;
import :clip_compiler;
import :skinned_model_compiler;
import :camera_system;

import :capture_renderer;
import :cull_compute_renderer;
import :depth_prepass_renderer;
import :forward_renderer;
import :geometry_collector;
import :light_culling_renderer;
import :physics_debug_renderer;
import :physics_transform_renderer;
import :rt_shadow_renderer;
import :skin_compute_renderer;
import :ui_renderer;

import gse.log;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.audio;
import gse.math;
import gse.save;
import gse.meta;

export namespace gse::renderer {
	struct settings {
		[[=gse::settings::describe{}]]
		bool hot_reload_enabled = false;

		[[=gse::settings::describe{}]]
		bool gpu_timestamps_enabled = true;

		[[=gse::settings::describe{}]]
		bool gpu_pipeline_stats_enabled = false;

		[[=gse::settings::describe{}]]
		bool profile_aggregator_enabled = true;
	};

	struct system {
		struct state {
			renderer::settings settings;
			actions::handle dump_profile_action;
			vec2f last_viewport{ 1920.f, 1080.f };
		};

		struct resources {
			gpu::context* ctx = nullptr;
			asset::registry* assets = nullptr;
		};

		static auto initialize(const init_context& phase, resources& r, state& s) -> void;
		static auto update(const update_context& ctx, state& s) -> async::task<>;
		static auto shutdown(shutdown_context& phase) -> void;
	};
}

export module gse.graphics:renderer;

import std;

import :render_component;
import :font;
import :model;
import :texture;
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
	struct system {
		struct [[= gse::settings::category<"Graphics">{}]] data {
			[[
				= gse::settings::describe<"Watch shader sources on disk and reload pipelines when files change.">{}
			]]
			bool hot_reload_enabled = false;

			[[
				= gse::settings::describe<"Record GPU timestamp queries around each render pass for the profiler.">{}
			]]
			bool gpu_timestamps_enabled = true;

			[[
				= gse::settings::describe<"Collect pipeline statistics (invocations, primitives) per pass. Has measurable overhead.">{}
			]]
			bool gpu_pipeline_stats_enabled = false;

			[[
				= gse::settings::describe<"Aggregate per-frame profiler samples into rolling averages for the HUD.">{}
			]]
			bool profile_aggregator_enabled = true;

			actions::handle dump_profile_action;
			vec2f last_viewport{ 1920.f, 1080.f };
			bool last_hot_reload_enabled = false;
		};

		static auto init(
			context& ctx,
			data& d
		) -> async::task<>;

		static auto run(
			context& ctx,
			shared_view<gpu::context> gpu_s,
			shared_view<window> window_s,
			data& d,
			shared_view<actions::system> sys
		) -> async::task<>;
	};
}

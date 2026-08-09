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
import gse.physics;
import gse.math;
import gse.save;
import gse.meta;

export namespace gse::renderer {
	struct [[= gse::system_state<"Renderer">{}, = gse::settings::category<"Graphics">{}]] data {
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

		[[
			= gse::settings::describe<"Retain a rolling ring of recent frame traces so a profile dump can emit the "
									  "worst frames instead of the current one. Costs a per-frame copy of the trace.">{}
		]]
		bool profile_frame_recording = false;

		actions::handle dump_profile_action;
		vec2f last_viewport{ 1920.f, 1080.f };
		bool last_hot_reload_enabled = false;
		bool last_profile_aggregator_enabled = true;
		bool last_profile_frame_recording = false;
		bool render_world = true;
	};

	[[= gse::system_init{}]]
	auto init(
		context& ctx,
		data& d,
		channel_write<actions::add_action_request> actions_out
	) -> async::task<>;

	[[= gse::system_run<>{}]]
	auto run(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		shared_view<window::data> window_s,
		data& d,
		channel_write<asset::hot_reload_request, camera::viewport_update> requests_out,
		shared_view<actions::data> sys
	) -> async::task<>;
}

module gse.graphics;

import std;

import :renderer;
import :camera_system;
import :clip;
import :font;
import :model;
import :skinned_model;
import :skeleton;
import :texture;
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

auto gse::renderer::system::resources::set_ui_focus(const bool focus) const -> void {
	ctx->set_ui_focus(focus);
}

auto gse::renderer::system::resources::context_ref() -> gpu::context& {
	return *ctx;
}

auto gse::renderer::system::resources::context_ref() const -> const gpu::context& {
	return *ctx;
}

auto gse::renderer::system::initialize(const init_context& phase, resources& r, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();
	r.ctx = &ctx;

	ctx.add_loader<texture>();
	ctx.add_loader<model>();
	ctx.add_loader<skinned_model>();
	ctx.add_loader<font>();
	ctx.add_loader<skeleton>();
	ctx.add_loader<clip_asset>();
	ctx.add_loader<audio_clip>();

	ctx.compile();

	phase.sched.add_system<rt_shadow::system, rt_shadow::state>(phase.reg);
	phase.sched.add_system<geometry_collector::system, geometry_collector::state>(phase.reg);
	phase.sched.add_system<skin_compute::system, skin_compute::state>(phase.reg);
	phase.sched.add_system<physics_transform::system, physics_transform::state>(phase.reg);
	phase.sched.add_system<cull_compute::system, cull_compute::state>(phase.reg);
	phase.sched.add_system<depth_prepass::system, depth_prepass::state>(phase.reg);
	phase.sched.add_system<light_culling::system, light_culling::state>(phase.reg);
	phase.sched.add_system<forward::system, forward::state>(phase.reg);
	phase.sched.add_system<physics_debug::system, physics_debug::state>(phase.reg);
	phase.sched.add_system<ui::system, ui::state>(phase.reg);
	phase.sched.add_system<capture::system, capture::state>(phase.reg);

	phase.channels.push<save::register_property>({
		.category = "Graphics",
		.name = "Hot Reload",
		.description = "Automatically reload assets when source files change",
		.ref = &s.hot_reload_enabled,
		.type = typeid(bool)
	});

	phase.channels.push<save::register_property>({
		.category = "Profiling",
		.name = "GPU Timestamps",
		.description = "Measure per-pass GPU time via timestamp queries",
		.ref = &s.gpu_timestamps_enabled,
		.type = typeid(bool)
	});

	phase.channels.push<save::register_property>({
		.category = "Profiling",
		.name = "GPU Pipeline Stats",
		.description = "Record draw/primitive/invocation counts per graphics pass",
		.ref = &s.gpu_pipeline_stats_enabled,
		.type = typeid(bool)
	});

	phase.channels.push<save::register_property>({
		.category = "Profiling",
		.name = "Rolling Averages",
		.description = "Aggregate CPU and GPU samples into rolling EMAs",
		.ref = &s.profile_aggregator_enabled,
		.type = typeid(bool)
	});

	const id dump_profile_id = generate_id("Dump Profile");
	phase.channels.push<actions::add_action_request>({
		.name = "Dump Profile",
		.default_key = key::f11,
		.action_id = dump_profile_id
	});
	s.dump_profile_action = actions::handle(dump_profile_id);
}

auto gse::renderer::system::update(const update_context& ctx, state& s) -> async::task<> {
	auto& gpu = ctx.get<gpu::context>();

	if (s.hot_reload_enabled != gpu.hot_reload_enabled()) {
		if (s.hot_reload_enabled) {
			gpu.enable_hot_reload();
		} else {
			gpu.disable_hot_reload();
		}
	}

	gpu.graph().set_gpu_timestamps_enabled(s.gpu_timestamps_enabled);
	gpu.graph().set_gpu_pipeline_stats_enabled(s.gpu_pipeline_stats_enabled);
	profile::set_enabled(s.profile_aggregator_enabled);

	if (const auto* sys = ctx.try_state_of<actions::system_state>(); sys && s.dump_profile_action.pressed(sys->current_state(), *sys)) {
		profile::dump();
		log::println(log::category::render, "Profile dumped");
	}

	gpu.poll_assets();
	gpu.process_resource_queue();
	gpu.window().update(gpu.ui_focus());

	ctx.channels.push<camera::ui_focus_update>({ .focus = gpu.ui_focus() });

	const auto window_size = gpu.window().viewport();
	const auto new_viewport = vec2f(
		static_cast<float>(window_size.x()),
		static_cast<float>(window_size.y())
	);

	if (new_viewport.x() != s.last_viewport.x() || new_viewport.y() != s.last_viewport.y()) {
		ctx.channels.push<camera::viewport_update>({ .size = new_viewport });
		s.last_viewport = new_viewport;
	}

	co_return;
}

auto gse::renderer::system::shutdown(const shutdown_context& phase) -> void {
	auto& ctx = phase.get<gpu::context>();

	ctx.shutdown();
}

module gse.graphics:renderer_impl;

import std;

import :renderer;
import :camera_system;
import :font;
import :model;
import :texture;
import :primitive_resolver;
import :atmosphere_renderer;
import :bloom_renderer;
import :capture_renderer;
import :cloud_renderer;
import :cull_compute_renderer;
import :depth_prepass_renderer;
import :forward_renderer;
import :geometry_collector;
import :gi_probe_renderer;
import :light_culling_renderer;
import :physics_debug_renderer;
import :physics_transform_renderer;
import :rt_shadow_renderer;
import :scene_snapshot_renderer;
import :sdf_grid_renderer;
import :ui_renderer;
import :taa_renderer;
import :tonemap_renderer;
import :world_text_renderer;
import :settings;


import gse.log;
import gse.core;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.system_manifest;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.audio;
import gse.physics;
import gse.math;
import gse.save;

auto gse::renderer::init(context& ctx, data& d) -> async::task<> {
	system_manifest<
		^^renderer::scene_snapshot::data, ^^renderer::scene_snapshot::init, ^^renderer::scene_snapshot::run, ^^renderer::scene_snapshot::frame,
		^^renderer::ui::data, ^^renderer::ui::init, ^^renderer::ui::run, ^^renderer::ui::frame
	>{}.register_with(ctx);

	const id dump_profile_id = generate_id("Dump Profile");
	ctx.channels.push<actions::add_action_request>({
		.name = "Dump Profile",
		.default_key = key::f11,
		.action_id = dump_profile_id
	});
	d.dump_profile_action = actions::handle(dump_profile_id);
	return {};
}

auto gse::renderer::run(context& ctx, const shared_view<gpu::context::data> gpu_s, const shared_view<window::data> window_s, data& d, const shared_view<actions::data> sys, const std::optional<shared_view<camera::data>> camera_s, const std::optional<shared_view<physics::data>> physics_s) -> async::task<> {
	if (d.render_world && !d.world_systems_registered && camera_s && physics_s) {
		system_manifest<
			^^primitive_resolver::data, ^^primitive_resolver::ensure_renders, ^^primitive_resolver::populate,
			^^renderer::geometry_collector::data, ^^renderer::geometry_collector::init, ^^renderer::geometry_collector::run, ^^renderer::geometry_collector::frame,
			^^renderer::cull_compute::data, ^^renderer::cull_compute::init, ^^renderer::cull_compute::frame,
			^^renderer::physics_transform::data, ^^renderer::physics_transform::init, ^^renderer::physics_transform::frame,
			^^renderer::depth_prepass::data, ^^renderer::depth_prepass::init, ^^renderer::depth_prepass::frame,
			^^renderer::rt_shadow::data, ^^renderer::rt_shadow::init, ^^renderer::rt_shadow::frame,
			^^renderer::gi_probe::data, ^^renderer::gi_probe::init, ^^renderer::gi_probe::frame,
			^^renderer::light_culling::data, ^^renderer::light_culling::init, ^^renderer::light_culling::frame,
			^^renderer::forward::data, ^^renderer::forward::init, ^^renderer::forward::frame,
			^^renderer::sdf_grid::data, ^^renderer::sdf_grid::init, ^^renderer::sdf_grid::frame,
			^^renderer::world_text::data, ^^renderer::world_text::init, ^^renderer::world_text::frame,
			^^renderer::physics_debug::data, ^^renderer::physics_debug::init, ^^renderer::physics_debug::prepare, ^^renderer::physics_debug::build, ^^renderer::physics_debug::frame,
			^^renderer::atmosphere::data, ^^renderer::atmosphere::init, ^^renderer::atmosphere::frame,
			^^renderer::cloud::data, ^^renderer::cloud::init, ^^renderer::cloud::frame,
			^^renderer::taa::data, ^^renderer::taa::init, ^^renderer::taa::frame,
			^^renderer::bloom::data, ^^renderer::bloom::init, ^^renderer::bloom::frame,
			^^renderer::tonemap::data, ^^renderer::tonemap::init, ^^renderer::tonemap::frame,
			^^renderer::capture::data, ^^renderer::capture::init, ^^renderer::capture::run, ^^renderer::capture::frame
		>{}.register_with(ctx);
		d.world_systems_registered = true;
	}

	if (d.hot_reload_enabled != d.last_hot_reload_enabled) {
		ctx.channels.push<asset::hot_reload_request>({
			.enabled = d.hot_reload_enabled
		});
		d.last_hot_reload_enabled = d.hot_reload_enabled;
	}

	gpu_s.render_graph->set_gpu_timestamps_enabled(d.gpu_timestamps_enabled);
	gpu_s.render_graph->set_gpu_pipeline_stats_enabled(d.gpu_pipeline_stats_enabled);
	profile::set_enabled(d.profile_aggregator_enabled);

	if (actions::pressed(actions::current_state(sys), sys, d.dump_profile_action)) {
		profile::dump();
		profile::dump_chrome_trace();
		log::println(log::category::render, "Profile dumped");
	}

	const auto window_size = window::viewport(window_s);
	const auto new_viewport = vec2f(static_cast<float>(window_size.x()), static_cast<float>(window_size.y()));

	if (new_viewport.x() > 0.f && new_viewport.y() > 0.f && (new_viewport.x() != d.last_viewport.x() || new_viewport.y() != d.last_viewport.y())) {
		ctx.channels.push<camera::viewport_update>({
			.size = new_viewport
		});
		d.last_viewport = new_viewport;
	}

	return {};
}

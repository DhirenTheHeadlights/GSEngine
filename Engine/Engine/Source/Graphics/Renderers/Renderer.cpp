module gse.graphics:renderer_impl;

import std;

import :renderer;
import :camera_system;
import :font;
import :model;
import :texture;
import :settings;


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
import gse.physics;
import gse.math;
import gse.save;

auto gse::renderer::init(context& ctx, data& d, const channel_write<actions::add_action_request> actions_out) -> async::task<> {
	const id dump_profile_id = generate_id("Dump Profile");
	actions_out.push<actions::add_action_request>({
		.name = "Dump Profile",
		.default_combo = { .k = key::f11 },
		.action_id = dump_profile_id
	});
	d.dump_profile_action = actions::handle(dump_profile_id);
	return {};
}

auto gse::renderer::run(context& ctx, const shared_view<gpu::context::data> gpu_s, const shared_view<window::data> window_s, data& d, const channel_write<asset::hot_reload_request, camera::viewport_update> requests_out, const shared_view<actions::data> sys) -> async::task<> {
	if (d.hot_reload_enabled != d.last_hot_reload_enabled) {
		requests_out.push<asset::hot_reload_request>({
			.enabled = d.hot_reload_enabled
		});
		d.last_hot_reload_enabled = d.hot_reload_enabled;
	}

	gpu_s.render_graph->set_gpu_timestamps_enabled(d.gpu_timestamps_enabled);
	gpu_s.render_graph->set_gpu_pipeline_stats_enabled(d.gpu_pipeline_stats_enabled);
	if (d.profile_aggregator_enabled != d.last_profile_aggregator_enabled) {
		profile::set_enabled(d.profile_aggregator_enabled);
		d.last_profile_aggregator_enabled = d.profile_aggregator_enabled;
	}
	if (d.profile_frame_recording != d.last_profile_frame_recording) {
		profile::set_frame_recording(d.profile_frame_recording);
		d.last_profile_frame_recording = d.profile_frame_recording;
	}
	if (d.profile_warmup_frames != d.last_profile_warmup_frames) {
		profile::reset();
		profile::set_warmup_frames(static_cast<std::uint64_t>(std::max(d.profile_warmup_frames, 0)));
		d.last_profile_warmup_frames = d.profile_warmup_frames;
	}

	if (actions::pressed(actions::current_state(sys), sys, d.dump_profile_action)) {
		profile::dump();
		profile::dump_chrome_trace();
		log::println(log::category::render, "Profile dumped");
	}

	const auto window_size = window::viewport(window_s);
	const auto new_viewport = vec2f(static_cast<float>(window_size.x()), static_cast<float>(window_size.y()));

	if (new_viewport.x() > 0.f && new_viewport.y() > 0.f && (new_viewport.x() != d.last_viewport.x() || new_viewport.y() != d.last_viewport.y())) {
		requests_out.push<camera::viewport_update>({
			.size = new_viewport
		});
		d.last_viewport = new_viewport;
	}

	return {};
}

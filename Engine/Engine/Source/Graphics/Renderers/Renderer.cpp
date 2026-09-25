module gse.graphics:renderer_impl;

import gse.assets;
import gse.audio;
import gse.concurrency;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.gpu;
import gse.log;
import gse.math;
import gse.os;
import gse.physics;
import gse.save;
import gse.time;
import std;

import :camera_system;
import :model;
import :renderer;
import :texture;

auto gse::renderer::run(context& ctx, const shared_view<gpu::context::data> gpu_s, const shared_view<window::data> window_s, data& d, const channel_write<asset::hot_reload_request, camera::viewport_update> requests_out, const shared_view<actions::data> sys) -> async::task<> {
	if (d.hot_reload_enabled != d.last_hot_reload_enabled) {
		requests_out.push<asset::hot_reload_request>({
			.enabled = d.hot_reload_enabled
		});
		d.last_hot_reload_enabled = d.hot_reload_enabled;
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
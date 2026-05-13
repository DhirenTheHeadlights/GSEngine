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
import gse.math;
import gse.save;
import gse.shader;

auto gse::renderer::system::run(run_context& ctx, const gpu::context::state& gpu_s, const window::state& window_s, settings& cfg, state& s, const actions::system::state& sys) -> async::task<> {
	shaders::initialize_layouts(*gpu_s.shader_registry);

	const id dump_profile_id = generate_id("Dump Profile");
	ctx.channels.push<actions::add_action_request>({
		.name = "Dump Profile",
		.default_key = key::f11,
		.action_id = dump_profile_id
	});
	s.dump_profile_action = actions::handle(dump_profile_id);

	while (true) {
		if (cfg.hot_reload_enabled != s.last_hot_reload_enabled) {
			ctx.channels.push<asset::hot_reload_request>({ .enabled = cfg.hot_reload_enabled });
			s.last_hot_reload_enabled = cfg.hot_reload_enabled;
		}

		gpu_s.render_graph->set_gpu_timestamps_enabled(cfg.gpu_timestamps_enabled);
		gpu_s.render_graph->set_gpu_pipeline_stats_enabled(cfg.gpu_pipeline_stats_enabled);
		profile::set_enabled(cfg.profile_aggregator_enabled);

		if (actions::system::pressed(actions::system::current_state(sys), sys, s.dump_profile_action)) {
			profile::dump();
			log::println(log::category::render, "Profile dumped");
		}

		const auto window_size = window::viewport(window_s);
		const auto new_viewport = vec2f(
			static_cast<float>(window_size.x()),
			static_cast<float>(window_size.y())
		);

		if (new_viewport.x() != s.last_viewport.x() || new_viewport.y() != s.last_viewport.y()) {
			ctx.channels.push<camera::viewport_update>({ .size = new_viewport });
			s.last_viewport = new_viewport;
		}

		co_await ctx.next_tick();
	}
}

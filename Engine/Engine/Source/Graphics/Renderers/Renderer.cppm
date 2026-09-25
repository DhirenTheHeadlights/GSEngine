export module gse.graphics:renderer;

import gse.assets;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.gpu;
import gse.math;
import gse.meta;
import gse.os;
import gse.physics;

import :camera_system;
import :capture_renderer;
import :model;
import :physics_debug_renderer;

export namespace gse::renderer {
	struct [[= system_state<"Renderer">{}, = settings::category<"Graphics">{}]] data {
		[[
			= settings::describe<"Watch shader sources on disk and reload pipelines when files change.">{}
		]]
		bool hot_reload_enabled = false;

		[[= actions::bind<"Dump Profile", key::f11>{}]]
		[[= actions::hidden{}]]
		actions::handle dump_profile_action;

		vec2f last_viewport{ 1920.f, 1080.f };
		bool last_hot_reload_enabled = false;
	};

	[[= system_run<>{}]]
	auto run(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		shared_view<window::data> window_s,
		data& d,
		channel_write<asset::hot_reload_request, camera::viewport_update> requests_out,
		shared_view<actions::data> sys
	) -> async::task<>;
}
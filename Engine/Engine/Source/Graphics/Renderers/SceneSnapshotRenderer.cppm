export module gse.graphics:scene_snapshot_renderer;

import std;

import gse.gpu;
import gse.core;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.meta;

export namespace gse::renderer::scene_snapshot {
	struct system {
		struct data {
			[[= gse::shared]] per_frame_resource<gpu::image> snapshots;
			[[= gse::shared]] std::array<gpu::bindless_texture_slot, per_frame_resource<gpu::image>::frames_in_flight>
				slots;
			[[= gse::shared]] bool ready = false;

			gpu::sampler sampler;
			vec2u current_extent{ 0, 0 };
		};

		static auto run(run_context& ctx, const gpu::context::data& gpu_s, data& d) -> async::task<>;

		static auto frame(const frame_context& ctx, shared_view<gpu::context> gpu_s, data& d) -> async::task<>;
	};
}

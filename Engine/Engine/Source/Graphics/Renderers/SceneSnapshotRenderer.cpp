module gse.graphics;

import std;

import :scene_snapshot_renderer;
import :forward_renderer;
import :physics_debug_renderer;
import :sdf_grid_renderer;
import :world_text_renderer;
import :shared_shaders;

import gse.gpu;
import gse.core;
import gse.concurrency;
import gse.math;
import gse.meta;
import gse.log;
import gse.time;

namespace gse::renderer::scene_snapshot {
	template <typename GpuS>
	auto recreate_resources(GpuS& gpu_s, system::data& d, const vec2u extent) -> void {
		for (std::size_t i = 0; i < per_frame_resource<gpu::image>::frames_in_flight; ++i) {
			if (d.slots[i]) {
				gpu_s.bindless_textures->release(d.slots[i]);
				d.slots[i] = {};
			}
		}

		for (std::size_t i = 0; i < per_frame_resource<gpu::image>::frames_in_flight; ++i) {
			d.snapshots[i] = gpu::image::create(
				gpu_s.device->allocator(),
				{
					.size = extent,
					.format = gpu_s.swapchain->format(),
					.usage = gpu::image_flag::sampled | gpu::image_flag::transfer_dst,
				}
			);

			d.slots[i] = gpu_s.bindless_textures->allocate(d.snapshots[i].view(), d.sampler.native());
		}

		d.current_extent = extent;
		d.ready = true;
	}
}

auto gse::renderer::scene_snapshot::system::run(run_context& ctx, const gpu::context::data& gpu_s, data& d)
	-> async::task<> {
	d.sampler = gpu::sampler::create(
		gpu_s.device->allocator(),
		{
			.min = gpu::sampler_filter::linear,
			.mag = gpu::sampler_filter::linear,
			.address_u = gpu::sampler_address_mode::clamp_to_edge,
			.address_v = gpu::sampler_address_mode::clamp_to_edge,
			.address_w = gpu::sampler_address_mode::clamp_to_edge,
		}
	);

	const auto initial_extent = gpu_s.render_graph->extent();
	if (initial_extent.x() > 0 && initial_extent.y() > 0) {
		recreate_resources(gpu_s, d, initial_extent);
	}

	while (true) {
		co_await ctx.next_tick();
	}
}

auto gse::renderer::scene_snapshot::system::frame(const frame_context& ctx, shared_view<gpu::context> gpu_s, data& d)
	-> async::task<> {
	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto extent = gpu_s.render_graph->extent();
	if (extent.x() == 0 || extent.y() == 0) {
		co_return;
	}

	if (!d.ready || d.current_extent != extent) {
		recreate_resources(gpu_s, d, extent);
	}

	const auto frame_index = gpu_s.render_graph->current_frame();

	auto rec = co_await gpu::pass<system>(ctx)
				   .after<forward::system, physics_debug::system, sdf_grid::system, world_text::system>();

	rec.blit_swapchain_to_image(*gpu_s.swapchain, *gpu_s.frame, d.snapshots[frame_index], extent);
}

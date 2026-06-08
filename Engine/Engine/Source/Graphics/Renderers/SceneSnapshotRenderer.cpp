module gse.graphics:scene_snapshot_renderer_impl;

import std;

import :scene_snapshot_renderer;
import :forward_renderer;
import :physics_debug_renderer;
import :sdf_grid_renderer;
import :tonemap_renderer;
import :world_text_renderer;
import :shared_shaders;


import gse.gpu;
import gse.core;
import gse.concurrency;
import gse.math;
import gse.meta;

namespace gse::renderer::scene_snapshot {
	constexpr gpu::sampler_desc snapshot_sampler_desc{
		.min = gpu::sampler_filter::linear,
		.mag = gpu::sampler_filter::linear,
		.address_u = gpu::sampler_address_mode::clamp_to_edge,
		.address_v = gpu::sampler_address_mode::clamp_to_edge,
		.address_w = gpu::sampler_address_mode::clamp_to_edge,
	};

	template <typename GpuS>
	auto recreate_resources(GpuS& gpu_s, system::data& d, const vec2u extent) -> void {
		for (std::size_t i = 0; i < per_frame_resource<gpu::image>::frames_in_flight; ++i) {
			if (d.slots[i].valid()) {
				d.slots[i] = {};
			}
		}

		for (std::size_t i = 0; i < per_frame_resource<gpu::image>::frames_in_flight; ++i) {
			d.snapshots[i] = gpu_s.device->create_image(
				{
					.size = extent,
					.format = gpu_s.swapchain->format(),
					.usage = gpu::image_flag::sampled | gpu::image_flag::transfer_dst,
				}
			);

			d.slots[i] = gpu_s.device->register_texture(d.snapshots[i], snapshot_sampler_desc);
		}

		d.current_extent = extent;
		d.ready = true;
	}
}

auto gse::renderer::scene_snapshot::system::init(const gpu::context::data& gpu_s, data& d) -> async::task<> {
	const auto initial_extent = gpu_s.render_graph->extent();
	if (d.enabled && initial_extent.x() > 0 && initial_extent.y() > 0) {
		recreate_resources(gpu_s, d, initial_extent);
	}
	return {};
}

auto gse::renderer::scene_snapshot::system::run(run_context& ctx, const gpu::context::data& gpu_s, data& d) -> async::task<> {
	return {};
}

auto gse::renderer::scene_snapshot::system::frame(const frame_context& ctx, shared_view<gpu::context> gpu_s, data& d) -> async::task<> {
	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	if (!d.enabled) {
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

	auto rec =
		co_await gpu::pass<system>(ctx)
		.after<forward::system, physics_debug::system, sdf_grid::system, world_text::system, tonemap::system>();

	rec.blit_swapchain_to_image(*gpu_s.swapchain, *gpu_s.frame, d.snapshots[frame_index], extent);
}

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
import gse.gpu_record;
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

	auto recreate_resources(const shared_view<gpu::context::data> gpu_s, data& d, const gpu::image_ref& target) -> void {
		for (std::size_t i = 0; i < per_frame_resource<gpu::image>::frames_in_flight; ++i) {
			if (d.slots[i].valid()) {
				d.slots[i] = {};
			}
		}

		for (std::size_t i = 0; i < per_frame_resource<gpu::image>::frames_in_flight; ++i) {
			d.snapshots[i] = gpu_s.device->create_image(
				{
					.size = target.extent,
					.format = target.format,
					.usage = { gpu::image_flag::sampled, gpu::image_flag::transfer_dst },
				}
			);

			d.slots[i] = gpu_s.device->register_texture(d.snapshots[i], snapshot_sampler_desc);
		}

		d.current_extent = target.extent;
		d.ready = true;
	}
}

auto gse::renderer::scene_snapshot::init(const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	const auto target = gpu_s.render_graph->current_target();
	if (d.enabled && target.extent.x() > 0 && target.extent.y() > 0) {
		recreate_resources(gpu_s, d, target);
	}
	return {};
}

auto gse::renderer::scene_snapshot::run(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	return {};
}

auto gse::renderer::scene_snapshot::frame(const context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out) -> async::task<> {
	if (!gpu_s.render_graph->frame_in_progress() || !gpu_s.render_graph->target_live()) {
		co_return;
	}

	if (!d.enabled) {
		co_return;
	}

	const auto target = gpu_s.render_graph->current_target();
	if (target.extent.x() == 0 || target.extent.y() == 0) {
		co_return;
	}

	if (!d.ready || d.current_extent != target.extent) {
		recreate_resources(gpu_s, d, target);
	}

	const auto frame_index = gpu_s.render_graph->current_frame();

	auto rec =
		co_await gpu::pass<^^frame>(pass_out)
		.after<^^forward::frame, ^^physics_debug::frame, ^^sdf_grid::frame, ^^world_text::frame, ^^tonemap::frame>();

	rec.blit_target_to_image(target, d.snapshots[frame_index], target.extent);
}
module gse.graphics;

import std;

import :cull_compute_renderer;
import :geometry_collector;
import :skin_compute_renderer;
import :camera_system;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.math;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;

auto gse::renderer::cull_compute::system::run(run_context& ctx, const gpu::context::state& gpu_s, const asset::state& assets_s, const geometry_collector::system::resources& gc_r, resources& r, state& s) -> async::task<> {
	r.shader_handle = co_await asset::load<shader>(ctx, "Shaders/Compute/cull_instances");

	if (!r.shader_handle.valid() || !r.shader_handle->is_compute()) {
		s.enabled = false;
		co_return;
	}

	r.batch_layout = layout_of(r.shader_handle->uniform_block("batches"));

	r.pipeline = gpu::create_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, r.shader_handle, "push_constants");

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		constexpr std::size_t frustum_size = sizeof(std::array<vec4f, 6>);
		r.frustum_buffer[i] = gpu::buffer::create(gpu_s.device->allocator(), {
			.size = frustum_size,
			.usage = gpu::buffer_flag::uniform | gpu::buffer_flag::transfer_dst
		});

		const std::size_t batch_info_size = geometry_collector::render_data::max_batches * 2 * r.batch_layout.stride;
		r.batch_info_buffer[i] = gpu::buffer::create(gpu_s.device->allocator(), {
			.size = batch_info_size,
			.usage = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst
		});
	}

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		r.normal_descriptors[i] = gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), r.shader_handle);
		r.skinned_descriptors[i] = gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), r.shader_handle);
	}

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		auto write_shared = [&](gpu::descriptor_writer& w) -> gpu::descriptor_writer& {
			return w.buffer("FrustumUBO", r.frustum_buffer[i], 0, sizeof(std::array<vec4f, 6>))
				.buffer("instances", gc_r.instance_buffer[i])
				.buffer("batches", r.batch_info_buffer[i]);
		};

		gpu::descriptor_writer normal_writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), r.shader_handle, r.normal_descriptors[i]);
		write_shared(normal_writer)
			.buffer("indirectCommands", gc_r.normal_indirect_commands_buffer[i])
			.commit();

		gpu::descriptor_writer skinned_writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), r.shader_handle, r.skinned_descriptors[i]);
		write_shared(skinned_writer)
			.buffer("indirectCommands", gc_r.skinned_indirect_commands_buffer[i])
			.commit();
	}

	co_return;
}

auto gse::renderer::cull_compute::system::frame(frame_context& ctx, const resources& r) -> async::task<> {
	auto rec = co_await gpu::pass<state>(ctx);
	rec.bind(r.pipeline);
}

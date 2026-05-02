export module gse.graphics:physics_transform_renderer;

import std;

import :geometry_collector;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.physics;

export namespace gse::renderer::physics_transform {
	struct state {};

	struct system {
		struct resources {
			resource::handle<shader> shader_handle;
			gpu::pipeline pipeline;
			bool initialized = false;
		};

		struct frame_data {
			per_frame_resource<gpu::descriptor_region> descriptors;

			per_frame_resource<gpu::buffer> mapping_buffers;
			std::size_t mapping_buffer_size = 0;
			std::uint32_t cached_mapping_count = 0;
		};

		static auto initialize(
			const init_context& phase,
			resources& r,
			frame_data& fd
		) -> void;

		static auto frame(
			frame_context& ctx,
			const resources& r,
			frame_data& fd,
			const geometry_collector::state& gc_s
		) -> async::task<>;
	};
}

auto gse::renderer::physics_transform::system::initialize(const init_context& phase, resources& r, frame_data& fd) -> void {
	auto& ctx = phase.get<gpu::context>();
	auto& assets = phase.assets();

	r.shader_handle = assets.get<shader>("Shaders/Compute/physics_instance_transform");
	assets.instantly_load(r.shader_handle);

	assert(r.shader_handle->is_compute(), std::source_location::current(), "Physics instance transform shader is not loaded as a compute shader");

	r.pipeline = gpu::create_compute_pipeline(ctx.device(), ctx.shader_registry(), ctx.bindless_textures(), r.shader_handle, "push_constants");

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		fd.descriptors[i] = gpu::allocate_descriptors(ctx.shader_registry(), ctx.descriptor_heap(), r.shader_handle);
	}

	r.initialized = true;
}

auto gse::renderer::physics_transform::system::frame(frame_context& ctx, const resources& r, frame_data& fd, const geometry_collector::state& gc_s) -> async::task<> {
	auto& gpu = ctx.get<gpu::context>();

	const auto& solver_infos = ctx.read_channel<physics::gpu_solver_frame_info>();

	if (solver_infos.empty()) {
		co_return;
	}

	if (!solver_infos[0].snapshot || solver_infos[0].semaphore.value() == 0 || solver_infos[0].body_count == 0) {
		co_return;
	}

	const auto& info = solver_infos[0];
	const auto& snapshot = *info.snapshot;

	const auto* gc_r = ctx.try_resources_of<geometry_collector::system::resources>();
	if (!gc_r) {
		co_return;
	}

	const auto frame_index = gpu.graph().current_frame();

	const auto& render_items = ctx.read_channel<geometry_collector::render_data>();

	if (!render_items.empty() && render_items[0].physics_mapping_count > 0) {
		const auto& data = render_items[0];
		const auto required = data.physics_mapping_count * sizeof(geometry_collector::physics_mapping_entry);

		fd.cached_mapping_count = data.physics_mapping_count;

		if (fd.mapping_buffer_size < required) {
			for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
				fd.mapping_buffers[i] = gpu::buffer::create(gpu.allocator(),{
					.size = required,
					.usage = gpu::buffer_flag::storage,
					.data = data.physics_mappings.data()
				});
			}
			fd.mapping_buffer_size = required;
		} else {
			for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
				gse::memcpy(fd.mapping_buffers[i].mapped(), data.physics_mappings.data(), required);
			}
		}
	}

	if (fd.cached_mapping_count == 0) {
		co_return;
	}

	if (!info.semaphore.has_signaled()) {
		co_return;
	}

	gpu::descriptor_writer(gpu.shader_registry(), gpu.device_handle(), r.shader_handle, fd.descriptors[frame_index])
		.buffer("body_data", snapshot, 0, info.body_count * info.body_stride)
		.buffer("mapping_data", fd.mapping_buffers[frame_index], 0, fd.cached_mapping_count * sizeof(geometry_collector::physics_mapping_entry))
		.buffer("instance_data", gc_r->instance_buffer[frame_index], 0, gc_r->instance_buffer[frame_index].size())
		.commit();

	auto pc = gpu::cache_push_block(r.shader_handle, "push_constants");
	pc.set("mapping_count", fd.cached_mapping_count);
	pc.set("body_count", info.body_count);

	const std::uint32_t workgroups = (fd.cached_mapping_count + 63) / 64;

	auto pass = gpu.graph().add_pass<state>();
	pass.track(fd.mapping_buffers[frame_index]);
	pass.track(gc_r->instance_buffer[frame_index]);

	pass.reads(
			gpu::storage_read(snapshot, gpu::pipeline_stage::compute_shader),
			gpu::storage_read(fd.mapping_buffers[frame_index], gpu::pipeline_stage::compute_shader)
		)
		.writes(gpu::storage_write(gc_r->instance_buffer[frame_index], gpu::pipeline_stage::compute_shader))
		.after<geometry_collector::state>();

	auto& rec = co_await pass.record();
	rec.bind(r.pipeline);
	rec.bind_descriptors(r.pipeline, fd.descriptors[frame_index]);
	rec.push(r.pipeline, pc);
	rec.dispatch(workgroups, 1, 1);
}

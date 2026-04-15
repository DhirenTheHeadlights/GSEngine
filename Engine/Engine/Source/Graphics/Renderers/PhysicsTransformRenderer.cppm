export module gse.graphics:physics_transform_renderer;

import std;

import :geometry_collector;
import gse.platform;
import gse.utility;
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
			per_frame_resource<gpu::buffer> staging_buffers;
			std::size_t staging_size = 0;

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
			frame_data& fd
		) -> async::task<>;
	};
}

auto gse::renderer::physics_transform::system::initialize(const init_context& phase, resources& r, frame_data& fd) -> void {
	auto& ctx = phase.get<gpu::context>();

	r.shader_handle = ctx.get<shader>("Shaders/Compute/physics_instance_transform");
	ctx.instantly_load(r.shader_handle);

	assert(r.shader_handle->is_compute(), std::source_location::current(), "Physics instance transform shader is not loaded as a compute shader");

	r.pipeline = gpu::create_compute_pipeline(ctx.device_ref(), *r.shader_handle, "push_constants");

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		fd.descriptors[i] = gpu::allocate_descriptors(ctx.device_ref(), *r.shader_handle);
	}

	r.initialized = true;
}

auto gse::renderer::physics_transform::system::frame(frame_context& ctx, const resources& r, frame_data& fd) -> async::task<> {
	co_await ctx.after<geometry_collector::state>();

	const auto& gpu = ctx.get<gpu::context>();

	const auto& solver_infos = ctx.read_channel<physics::gpu_solver_frame_info>();
	if (solver_infos.empty() || !solver_infos[0].snapshot || solver_infos[0].semaphore.value == 0 || solver_infos[0].body_count == 0) {
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
				fd.mapping_buffers[i] = gpu::create_buffer(gpu.device_ref(), {
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

	gpu.frame().add_wait_semaphore(info.semaphore);

	const auto required_size = gc_r->instance_buffer[frame_index].size();
	if (fd.staging_size < required_size) {
		for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
			fd.staging_buffers[i] = gpu::create_buffer(gpu.device_ref(), {
				.size = required_size,
				.usage = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_src | gpu::buffer_flag::transfer_dst
			});
		}
		fd.staging_size = required_size;
	}

	auto& staging = fd.staging_buffers[frame_index];

	gpu::descriptor_writer(gpu.device_ref(), r.shader_handle, fd.descriptors[frame_index])
		.buffer("body_data", snapshot, 0, info.body_count * info.body_stride)
		.buffer("mapping_data", fd.mapping_buffers[frame_index], 0, fd.cached_mapping_count * sizeof(geometry_collector::physics_mapping_entry))
		.buffer("instance_data", staging, 0, staging.size())
		.commit();

	auto pc = r.shader_handle->cache_push_block("push_constants");
	pc.set("mapping_count", fd.cached_mapping_count);
	pc.set("body_count", info.body_count);

	const std::uint32_t workgroups = (fd.cached_mapping_count + 63) / 64;
	const auto copy_size = required_size;

	auto pass = gpu.graph().add_pass<state>();
	pass.track(fd.mapping_buffers[frame_index]);
	pass.track(gc_r->instance_buffer[frame_index]);

	pass.reads(
			gpu::storage_read(snapshot, gpu::pipeline_stage::compute_shader),
			gpu::storage_read(fd.mapping_buffers[frame_index], gpu::pipeline_stage::compute_shader),
			gpu::transfer_read(gc_r->instance_buffer[frame_index])
		)
		.writes(gpu::transfer_write(gc_r->instance_buffer[frame_index]))
		.after<geometry_collector::state>()
		.record([&r, &fd, &staging, gc_r, frame_index, workgroups, copy_size, pc = std::move(pc)](const gpu::recording_context& rec) {
			rec.copy_buffer(gc_r->instance_buffer[frame_index], staging, copy_size);
			rec.barrier(gpu::barrier_scope::transfer_to_compute);

			rec.bind(r.pipeline);
			rec.bind_descriptors(r.pipeline, fd.descriptors[frame_index]);
			rec.push(r.pipeline, pc);
			rec.dispatch(workgroups, 1, 1);
			rec.barrier(gpu::barrier_scope::compute_to_transfer);

			rec.copy_buffer(staging, gc_r->instance_buffer[frame_index], copy_size);
		});
}

module gse.graphics;

import std;

import :physics_transform_renderer;
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

namespace gse::renderer::physics_transform {
	struct [[= shaders::shader_struct]] physics_mapping {
		std::uint32_t body_index;
		std::uint32_t instance_index;
		vec3f center_of_mass;
	};

	struct [[= shaders::shader_struct]] push_constants {
		std::uint32_t mapping_count;
		std::uint32_t body_count;
	};

	struct [[= shaders::binding<0, 0>{}, = shaders::ssbo_readonly]] body_data {
		using element = vbd::gpu_body;
	};

	struct [[= shaders::binding<0, 1>{}, = shaders::ssbo_readonly]] mapping_data {
		using element = physics_mapping;
	};

	struct [[= shaders::binding<0, 2>{}, = shaders::ssbo_readwrite]] instance_data_buffer {
		using element = shaders::common::instance_data;
	};

	using shader_binding_types = type_pack<body_data, mapping_data, instance_data_buffer>;
	using shader_types = type_pack<physics_mapping>;

	using entry = gpu::compute_entry<
		gpu::body_path<"Compute/physics_instance_transform">,
		gpu::layout<"physics_instance_transform">,
		gpu::types<shaders::common::shader_types, vbd::shader_types, shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"VBDPhysics/vbd_shared">,
		gpu::threads<64>,
		gpu::push_constant<push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;
}

auto gse::renderer::physics_transform::system::run(run_context& ctx, const gpu::context::state& gpu_s, const asset::state& assets_s, resources& r, frame_data& fd) -> async::task<> {
	r.pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, entry::pod);

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		fd.descriptors[i] = gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), entry::pod);
	}

	r.initialized = true;

	co_return;
}

auto gse::renderer::physics_transform::system::frame(frame_context& ctx, const gpu::context::state& gpu_s, const resources& r, frame_data& fd, const geometry_collector::system::resources& gc_r) -> async::task<> {

	const auto& solver_infos = ctx.read_channel<physics::gpu_solver_frame_info>();

	if (solver_infos.empty()) {
		co_return;
	}

	if (!solver_infos[0].snapshot || solver_infos[0].semaphore.value() == 0 || solver_infos[0].body_count == 0) {
		co_return;
	}

	const auto& info = solver_infos[0];
	const auto& snapshot = *info.snapshot;

	const auto frame_index = gpu_s.render_graph->current_frame();

	const auto& render_items = ctx.read_channel<geometry_collector::render_data>();

	if (!render_items.empty() && render_items[0].physics_mapping_count > 0) {
		const auto& data = render_items[0];
		const auto required = data.physics_mapping_count * sizeof(geometry_collector::physics_mapping_entry);

		fd.cached_mapping_count = data.physics_mapping_count;

		if (fd.mapping_buffer_size < required) {
			for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
				fd.mapping_buffers[i] = gpu::buffer::create(gpu_s.device->allocator(), {
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

	gpu::descriptor_writer(gpu::context::device_handle(gpu_s), fd.descriptors[frame_index])
		.buffer<body_data>(snapshot, 0, info.body_count * info.body_stride)
		.buffer<mapping_data>(fd.mapping_buffers[frame_index], 0, fd.cached_mapping_count * sizeof(geometry_collector::physics_mapping_entry))
		.buffer<instance_data_buffer>(gc_r.instance_buffer[frame_index], 0, gc_r.instance_buffer[frame_index].size())
		.commit();

	const gpu::typed_push_constants<push_constants> pc{
		.data = {
			.mapping_count = fd.cached_mapping_count,
			.body_count = info.body_count,
		},
		.stages = gpu::stage_flag::compute,
	};

	const std::uint32_t workgroups = (fd.cached_mapping_count + 63) / 64;

	auto rec = co_await gpu::pass<system>(ctx)
		.after<geometry_collector::system>()
		.reads(
			gpu::storage_read(snapshot, gpu::pipeline_stage::compute_shader),
			gpu::storage_read(fd.mapping_buffers[frame_index], gpu::pipeline_stage::compute_shader)
		)
		.writes(gpu::storage_write(gc_r.instance_buffer[frame_index], gpu::pipeline_stage::compute_shader))
		.tracks(fd.mapping_buffers[frame_index], gc_r.instance_buffer[frame_index]);

	rec.bind(r.pipeline);
	rec.bind_descriptors(r.pipeline, fd.descriptors[frame_index]);
	rec.push(r.pipeline, pc);
	rec.dispatch(workgroups, 1, 1);
}

module gse.gpu:acceleration_structure_impl;

import std;

import :acceleration_structure;
import :device;
import :gpu_task;
import :render_graph;
import :pass_recorder;

import gse.vulkan;
import gse.concurrency;

auto gse::build_blas_async(gpu::device& dev, const gpu::acceleration_structure as_handle, gpu::acceleration_structure_geometry geometry, const std::uint32_t prim_count, const gpu::device_size scratch_size, const gpu::device_size scratch_alignment) -> async::task<> {
	auto scratch = dev.create_buffer(
		gpu::buffer_desc{
			.size = scratch_size + scratch_alignment,
			.usage = gpu::buffer_flag::acceleration_structure_scratch,
		}
	);

	const auto scratch_addr = scratch.device_address();
	const gpu::device_address aligned_scratch = (scratch_addr + scratch_alignment - 1) & ~(scratch_alignment - 1);

	auto cmd = co_await gpu::begin_transient(dev, gpu::queue_id::graphics, "transient.blas_build");

	const gpu::acceleration_structure_build_geometry_info build_info{
		.type = gpu::acceleration_structure_type::bottom_level,
		.flags = gpu::build_acceleration_structure_flag::prefer_fast_build,
		.mode = gpu::build_acceleration_structure_mode::build,
		.dst = as_handle,
		.geometries = std::span(&geometry, 1),
		.scratch_address = aligned_scratch,
	};

	const gpu::acceleration_structure_build_range_info range{
		.primitive_count = prim_count,
	};
	const gpu::acceleration_structure_build_range_info* range_ptr = &range;

	const auto cmd_handle = cmd.handle();

	const gpu::memory_barrier pre_barrier{
		.src_stages = gpu::pipeline_stage_flag::copy,
		.src_access = gpu::access_flag::transfer_write,
		.dst_stages = gpu::pipeline_stage_flag::acceleration_structure_build,
		.dst_access = gpu::access_flag::shader_read,
	};
	gpu::pass_recorder(cmd_handle).pipeline_barrier(gpu::dependency_info{
		.memory_barriers = std::span(&pre_barrier, 1)
	});

	gpu::pass_recorder(cmd_handle).build_acceleration_structures(build_info, std::span(&range_ptr, 1));

	const gpu::memory_barrier barrier{
		.src_stages = gpu::pipeline_stage_flag::acceleration_structure_build,
		.src_access = gpu::access_flag::acceleration_structure_write,
		.dst_stages = gpu::pipeline_stage_flag::acceleration_structure_build,
		.dst_access = gpu::access_flag::acceleration_structure_read,
	};
	gpu::pass_recorder(cmd_handle).pipeline_barrier(gpu::dependency_info{
		.memory_barriers = std::span(&barrier, 1)
	});

	co_await gpu::submit(dev, std::move(cmd), gpu::queue_id::graphics).retain(std::move(scratch));
}

auto gse::gpu::build_blas(gpu::device& device, const blas_geometry_desc& desc) -> blas {
	const auto vertex_addr = desc.vertex_buffer->device_address();
	const auto index_addr = desc.index_buffer->device_address();

	const acceleration_structure_geometry geometry{
		.type = acceleration_structure_geometry_type::triangles,
		.triangles = {
			.vertex_format = vertex_format::r32g32b32_sfloat,
			.vertex_data = vertex_addr,
			.vertex_stride = desc.vertex_stride,
			.max_vertex = desc.vertex_count - 1,
			.index_type = index_type::uint32,
			.index_data = index_addr,
		},
		.flags = geometry_flag::opaque,
	};

	const std::uint32_t prim_count = desc.index_count / 3;

	auto result = device.create_blas(geometry, prim_count);

	const auto sizes = device.query_blas_build_sizes(geometry, prim_count);
	const auto scratch_alignment = device.acceleration_structure_scratch_alignment();

	dispatch(
		device,
		gse::build_blas_async(
			device,
			result.handle(),
			geometry,
			prim_count,
			sizes.build_scratch_size,
			scratch_alignment
		)
	);

	return result;
}

auto gse::build_tlas_initial_empty_async(gpu::device& dev, const gpu::acceleration_structure as_handle, const gpu::device_address instance_addr, const gpu::device_address scratch_addr) -> async::task<> {
	auto cmd = co_await gpu::begin_transient(dev, gpu::queue_id::graphics, "transient.tlas_initial_build");

	const gpu::acceleration_structure_geometry geometry{
		.type = gpu::acceleration_structure_geometry_type::instances,
		.instances = {
			.array_of_pointers = false,
			.data = instance_addr,
		},
	};

	const gpu::acceleration_structure_build_geometry_info build_info{
		.type = gpu::acceleration_structure_type::top_level,
		.flags = gpu::build_acceleration_structure_flag::prefer_fast_build | gpu::build_acceleration_structure_flag::allow_update,
		.mode = gpu::build_acceleration_structure_mode::build,
		.dst = as_handle,
		.geometries = std::span(&geometry, 1),
		.scratch_address = scratch_addr,
	};

	const gpu::acceleration_structure_build_range_info range{
		.primitive_count = 0,
	};
	const gpu::acceleration_structure_build_range_info* range_ptr = &range;

	gpu::pass_recorder(cmd.handle()).build_acceleration_structures(build_info, std::span(&range_ptr, 1));

	const gpu::memory_barrier post_barrier{
		.src_stages = gpu::pipeline_stage_flag::acceleration_structure_build,
		.src_access = gpu::access_flag::acceleration_structure_write,
		.dst_stages = gpu::pipeline_stage_flag::acceleration_structure_build | gpu::pipeline_stage_flag::fragment_shader,
		.dst_access = gpu::access_flag::acceleration_structure_read,
	};
	gpu::pass_recorder(cmd.handle()).pipeline_barrier(gpu::dependency_info{
		.memory_barriers = std::span(&post_barrier, 1)
	});

	co_await gpu::submit(dev, std::move(cmd), gpu::queue_id::graphics);
}

auto gse::gpu::build_tlas(gpu::device& device, const std::uint32_t max_instances) -> tlas {
	auto t = device.create_tlas(max_instances);

	const auto instance_addr = t.instance_buffer().device_address();
	const auto scratch_alignment = device.acceleration_structure_scratch_alignment();
	const auto scratch_raw = t.scratch_buffer().device_address();
	const device_address scratch_addr = (scratch_raw + scratch_alignment - 1) & ~(scratch_alignment - 1);

	dispatch(device, build_tlas_initial_empty_async(device, t.handle(), instance_addr, scratch_addr));

	return t;
}

auto gse::gpu::rebuild_tlas(gpu::device& device, tlas& t, const std::span<const tlas_instance_desc> instances, recording_context& rec) -> void {
	std::vector<acceleration_structure_instance> packed_instances;
	packed_instances.reserve(instances.size());
	for (const auto& inst : instances) {
		packed_instances.push_back(pack_instance(inst));
	}

	if (!packed_instances.empty()) {
		t.instance_buffer().host_write(packed_instances.data(), packed_instances.size() * sizeof(acceleration_structure_instance));
	}

	const auto instance_addr = t.instance_buffer().device_address();
	const auto scratch_alignment = device.acceleration_structure_scratch_alignment();
	const auto scratch_raw = t.scratch_buffer().device_address();
	const device_address scratch_addr = (scratch_raw + scratch_alignment - 1) & ~(scratch_alignment - 1);

	const std::array pre_barriers{
		memory_barrier{
			.src_stages = pipeline_stage_flag::host,
			.src_access = access_flag::host_write,
			.dst_stages = pipeline_stage_flag::acceleration_structure_build,
			.dst_access = access_flag::shader_read,
		},
		memory_barrier{
			.src_stages = pipeline_stage_flag::acceleration_structure_build,
			.src_access = access_flag::acceleration_structure_write,
			.dst_stages = pipeline_stage_flag::acceleration_structure_build,
			.dst_access = access_flag::acceleration_structure_read,
		},
	};
	rec.pipeline_barrier(dependency_info{
		.memory_barriers = pre_barriers
	});
	t.instance_buffer().clear_host_dirty();

	const acceleration_structure_geometry geometry{
		.type = acceleration_structure_geometry_type::instances,
		.instances = {
			.array_of_pointers = false,
			.data = instance_addr,
		},
	};

	const acceleration_structure_build_geometry_info build_info{
		.type = acceleration_structure_type::top_level,
		.flags = build_acceleration_structure_flag::prefer_fast_build | build_acceleration_structure_flag::allow_update,
		.mode = build_acceleration_structure_mode::build,
		.dst = t.handle(),
		.geometries = std::span(&geometry, 1),
		.scratch_address = scratch_addr,
	};

	const acceleration_structure_build_range_info range{
		.primitive_count = static_cast<std::uint32_t>(packed_instances.size()),
	};
	const acceleration_structure_build_range_info* range_ptr = &range;

	rec.build_acceleration_structure(
		build_info,
		{ &range_ptr, 1 }
	);

	const memory_barrier barrier{
		.src_stages = pipeline_stage_flag::acceleration_structure_build,
		.src_access = access_flag::acceleration_structure_write,
		.dst_stages = pipeline_stage_flag::acceleration_structure_build | pipeline_stage_flag::fragment_shader,
		.dst_access = access_flag::acceleration_structure_read | access_flag::acceleration_structure_write,
	};
	rec.pipeline_barrier(dependency_info{
		.memory_barriers = std::span(&barrier, 1)
	});
}

auto gse::gpu::write_tlas_instances(tlas& t, const std::span<const tlas_instance_desc> instances) -> void {
	std::vector<acceleration_structure_instance> packed_instances;
	packed_instances.reserve(instances.size());
	for (const auto& inst : instances) {
		packed_instances.push_back(pack_instance(inst));
	}

	if (!packed_instances.empty()) {
		t.instance_buffer().host_write(packed_instances.data(), packed_instances.size() * sizeof(acceleration_structure_instance));
	}
}

auto gse::gpu::build_tlas_in_place(gpu::device& device, tlas& t, const std::uint32_t instance_count, recording_context& rec) -> void {
	const auto instance_addr = t.instance_buffer().device_address();
	const auto scratch_alignment = device.acceleration_structure_scratch_alignment();
	const auto scratch_raw = t.scratch_buffer().device_address();
	const device_address scratch_addr = (scratch_raw + scratch_alignment - 1) & ~(scratch_alignment - 1);

	const std::array pre_barriers{
		memory_barrier{
			.src_stages = pipeline_stage_flag::host,
			.src_access = access_flag::host_write,
			.dst_stages = pipeline_stage_flag::acceleration_structure_build,
			.dst_access = access_flag::shader_read,
		},
		memory_barrier{
			.src_stages = pipeline_stage_flag::compute_shader,
			.src_access = access_flag::shader_write,
			.dst_stages = pipeline_stage_flag::acceleration_structure_build,
			.dst_access = access_flag::shader_read,
		},
		memory_barrier{
			.src_stages = pipeline_stage_flag::acceleration_structure_build,
			.src_access = access_flag::acceleration_structure_write,
			.dst_stages = pipeline_stage_flag::acceleration_structure_build,
			.dst_access = access_flag::acceleration_structure_read,
		},
	};
	rec.pipeline_barrier(dependency_info{
		.memory_barriers = pre_barriers
	});
	t.instance_buffer().clear_host_dirty();

	const acceleration_structure_geometry geometry{
		.type = acceleration_structure_geometry_type::instances,
		.instances = {
			.array_of_pointers = false,
			.data = instance_addr,
		},
	};

	const acceleration_structure_build_geometry_info build_info{
		.type = acceleration_structure_type::top_level,
		.flags = build_acceleration_structure_flag::prefer_fast_build | build_acceleration_structure_flag::allow_update,
		.mode = build_acceleration_structure_mode::build,
		.dst = t.handle(),
		.geometries = std::span(&geometry, 1),
		.scratch_address = scratch_addr,
	};

	const acceleration_structure_build_range_info range{
		.primitive_count = instance_count,
	};
	const acceleration_structure_build_range_info* range_ptr = &range;

	rec.build_acceleration_structure(
		build_info,
		{ &range_ptr, 1 }
	);

	const memory_barrier post_barrier{
		.src_stages = pipeline_stage_flag::acceleration_structure_build,
		.src_access = access_flag::acceleration_structure_write,
		.dst_stages = pipeline_stage_flag::acceleration_structure_build | pipeline_stage_flag::fragment_shader,
		.dst_access = access_flag::acceleration_structure_read | access_flag::acceleration_structure_write,
	};
	rec.pipeline_barrier(dependency_info{
		.memory_barriers = std::span(&post_barrier, 1)
	});
}

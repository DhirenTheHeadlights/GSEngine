module gse.gpu_record:acceleration_structure_impl;

import std;

import :acceleration_structure;
import :recording_context;

import gse.gpu;
import gse.concurrency;

auto gse::gpu::make_blas_geometry(const blas_geometry_desc& desc) -> acceleration_structure_geometry {
	return acceleration_structure_geometry{
		.type = acceleration_structure_geometry_type::triangles,
		.triangles = {
			.vertex_format = vertex_format::r32g32b32_sfloat,
			.vertex_data = desc.vertex_buffer->device_address(),
			.vertex_stride = desc.vertex_stride,
			.max_vertex = desc.vertex_count - 1,
			.index_type = index_type::uint32,
			.index_data = desc.index_buffer->device_address(),
		},
		.flags = geometry_flag::opaque,
	};
}

auto gse::gpu::build_blas_in_place(gpu::device& device, const acceleration_structure dst, const acceleration_structure_geometry& geometry, const std::uint32_t prim_count, const buffer& scratch, recording_context& rec) -> void {
	const auto scratch_alignment = device.acceleration_structure_scratch_alignment();
	const auto scratch_raw = scratch.device_address();
	const device_address scratch_addr = (scratch_raw + scratch_alignment - 1) & ~(scratch_alignment - 1);

	const acceleration_structure_build_geometry_info build_info{
		.type = acceleration_structure_type::bottom_level,
		.flags = build_acceleration_structure_flag::prefer_fast_build,
		.mode = build_acceleration_structure_mode::build,
		.dst = dst,
		.geometries = std::span(&geometry, 1),
		.scratch_address = scratch_addr,
	};

	const acceleration_structure_build_range_info range{
		.primitive_count = prim_count,
	};
	const acceleration_structure_build_range_info* range_ptr = &range;

	rec.build_acceleration_structure(
		build_info,
		{ &range_ptr, 1 }
	);

	const memory_barrier post_barrier{
		.src_stages = pipeline_stage_flag::acceleration_structure_build,
		.src_access = access_flag::acceleration_structure_write,
		.dst_stages = pipeline_stage_flag::acceleration_structure_build,
		.dst_access = { access_flag::acceleration_structure_read, access_flag::acceleration_structure_write },
	};
	rec.pipeline_barrier(dependency_info{
		.memory_barriers = std::span(&post_barrier, 1)
	});
}

auto gse::gpu::build_tlas(gpu::device& device, const std::uint32_t max_instances) -> tlas {
	return device.create_tlas(max_instances);
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
			.dst_access = access_flag::acceleration_structure_read,
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
		.flags = build_acceleration_structure_flag::prefer_fast_build,
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
		.dst_stages = { pipeline_stage_flag::acceleration_structure_build, pipeline_stage_flag::fragment_shader, pipeline_stage_flag::compute_shader },
		.dst_access = { access_flag::acceleration_structure_read, access_flag::acceleration_structure_write },
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
			.dst_access = access_flag::acceleration_structure_read,
		},
		memory_barrier{
			.src_stages = pipeline_stage_flag::compute_shader,
			.src_access = access_flag::shader_write,
			.dst_stages = pipeline_stage_flag::acceleration_structure_build,
			.dst_access = access_flag::acceleration_structure_read,
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
		.flags = build_acceleration_structure_flag::prefer_fast_build,
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
		.dst_stages = { pipeline_stage_flag::acceleration_structure_build, pipeline_stage_flag::fragment_shader, pipeline_stage_flag::compute_shader },
		.dst_access = { access_flag::acceleration_structure_read, access_flag::acceleration_structure_write },
	};
	rec.pipeline_barrier(dependency_info{
		.memory_barriers = std::span(&post_barrier, 1)
	});
}

module gse.gpu;

import std;
import vulkan;

import gse.concurrency;

auto gse::build_blas_async(gpu::device& dev, const gpu::acceleration_structure_handle as_handle, gpu::acceleration_structure_geometry geometry, const std::uint32_t prim_count, const gpu::device_size scratch_size, const gpu::device_size scratch_alignment) -> async::task<> {
	auto& dev_cfg = dev.vulkan_device();

	auto scratch = dev_cfg.create_buffer(gpu::buffer_create_info{
		.size = scratch_size + scratch_alignment,
		.usage = gpu::buffer_flag::storage,
	}, nullptr);

	const auto scratch_addr = vulkan::buffer_device_address(dev_cfg, scratch.handle());
	const gpu::device_address aligned_scratch = (scratch_addr + scratch_alignment - 1) & ~(scratch_alignment - 1);

	auto cmd = co_await gpu::begin_transient(dev, gpu::queue_id::graphics);

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
	vulkan::commands(cmd_handle).build_acceleration_structures(build_info, std::span(&range_ptr, 1));

	const gpu::memory_barrier barrier{
		.src_stages = gpu::pipeline_stage_flag::acceleration_structure_build,
		.src_access = gpu::access_flag::acceleration_structure_write,
		.dst_stages = gpu::pipeline_stage_flag::acceleration_structure_build,
		.dst_access = gpu::access_flag::acceleration_structure_read,
	};
	vulkan::commands(cmd_handle).pipeline_barrier(gpu::dependency_info{ .memory_barriers = std::span(&barrier, 1) });

	co_await gpu::submit(dev, std::move(cmd), gpu::queue_id::graphics)
		.retain(std::move(scratch));
}
auto gse::to_vk_instance(const gpu::tlas_instance_desc& inst) -> vk::AccelerationStructureInstanceKHR {
	vk::TransformMatrixKHR transform{};
	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 4; ++col) {
			transform.matrix[row][col] = inst.transform[col][row];
		}
	}

	return {
		.transform = transform,
		.instanceCustomIndex = inst.custom_index,
		.mask = inst.mask,
		.instanceShaderBindingTableRecordOffset = inst.sbt_offset,
		.flags = static_cast<std::uint8_t>(inst.cull_disable ? vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable : vk::GeometryInstanceFlagBitsKHR{}),
		.accelerationStructureReference = inst.blas_address,
	};
}

auto gse::gpu::build_blas(context& ctx, const blas_geometry_desc& desc) -> vulkan::blas {
	auto& dev = ctx.device();
	auto& dev_cfg = dev.vulkan_device();

	const auto vertex_addr = vulkan::buffer_device_address(dev_cfg, desc.vertex_buffer->handle());
	const auto index_addr = vulkan::buffer_device_address(dev_cfg, desc.index_buffer->handle());

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

	auto result = vulkan::blas::create(dev_cfg, geometry, prim_count);

	const auto sizes = vulkan::query_blas_build_sizes(dev_cfg, geometry, prim_count);
	const auto scratch_alignment = vulkan::scratch_offset_alignment(dev_cfg);

	dispatch(dev, gse::build_blas_async(dev, result.handle(), geometry, prim_count, sizes.build_scratch_size, scratch_alignment));

	return result;
}

auto gse::gpu::build_tlas(context& ctx, const std::uint32_t max_instances) -> vulkan::tlas {
	return vulkan::tlas::create(ctx.device().vulkan_device(), max_instances);
}

auto gse::gpu::rebuild_tlas(context& ctx, vulkan::tlas& t, const std::span<const tlas_instance_desc> instances, vulkan::recording_context& rec) -> void {
	auto& dev = ctx.device();
	const auto& dev_cfg = dev.vulkan_device();

	std::vector<vk::AccelerationStructureInstanceKHR> vk_instances;
	vk_instances.reserve(instances.size());
	for (const auto& inst : instances) {
		vk_instances.push_back(to_vk_instance(inst));
	}

	if (auto* mapped = t.instance_buffer().mapped(); mapped && !vk_instances.empty()) {
		std::memcpy(mapped, vk_instances.data(), vk_instances.size() * sizeof(vk::AccelerationStructureInstanceKHR));
	}

	const auto instance_addr = vulkan::buffer_device_address(dev_cfg, t.instance_buffer().handle());
	const auto scratch_alignment = vulkan::scratch_offset_alignment(dev_cfg);
	const auto scratch_raw = vulkan::buffer_device_address(dev_cfg, t.scratch_buffer().handle());
	const device_address scratch_addr = (scratch_raw + scratch_alignment - 1) & ~(scratch_alignment - 1);

	const memory_barrier host_write_barrier{
		.src_stages = pipeline_stage_flag::host,
		.src_access = access_flag::host_write,
		.dst_stages = pipeline_stage_flag::acceleration_structure_build,
		.dst_access = access_flag::acceleration_structure_read,
	};
	rec.pipeline_barrier(dependency_info{ .memory_barriers = std::span(&host_write_barrier, 1) });

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
		.primitive_count = static_cast<std::uint32_t>(vk_instances.size()),
	};
	const acceleration_structure_build_range_info* range_ptr = &range;

	rec.build_acceleration_structure(build_info, { &range_ptr, 1 });

	const memory_barrier barrier{
		.src_stages = pipeline_stage_flag::acceleration_structure_build,
		.src_access = access_flag::acceleration_structure_write,
		.dst_stages = pipeline_stage_flag::acceleration_structure_build | pipeline_stage_flag::fragment_shader,
		.dst_access = access_flag::acceleration_structure_read | access_flag::acceleration_structure_write,
	};
	rec.pipeline_barrier(dependency_info{ .memory_barriers = std::span(&barrier, 1) });
}

auto gse::gpu::write_tlas_instances(vulkan::tlas& t, const std::span<const tlas_instance_desc> instances) -> void {
	std::vector<vk::AccelerationStructureInstanceKHR> vk_instances;
	vk_instances.reserve(instances.size());
	for (const auto& inst : instances) {
		vk_instances.push_back(to_vk_instance(inst));
	}

	if (auto* mapped = t.instance_buffer().mapped(); mapped && !vk_instances.empty()) {
		std::memcpy(mapped, vk_instances.data(), vk_instances.size() * sizeof(vk::AccelerationStructureInstanceKHR));
	}
}

auto gse::gpu::build_tlas_in_place(context& ctx, vulkan::tlas& t, const std::uint32_t instance_count, vulkan::recording_context& rec) -> void {
	const auto& dev_cfg = ctx.device().vulkan_device();

	const auto instance_addr = vulkan::buffer_device_address(dev_cfg, t.instance_buffer().handle());
	const auto scratch_alignment = vulkan::scratch_offset_alignment(dev_cfg);
	const auto scratch_raw = vulkan::buffer_device_address(dev_cfg, t.scratch_buffer().handle());
	const device_address scratch_addr = (scratch_raw + scratch_alignment - 1) & ~(scratch_alignment - 1);

	const memory_barrier pre_barrier{
		.src_stages = pipeline_stage_flag::compute_shader,
		.src_access = access_flag::shader_write,
		.dst_stages = pipeline_stage_flag::acceleration_structure_build,
		.dst_access = access_flag::acceleration_structure_read,
	};
	rec.pipeline_barrier(dependency_info{ .memory_barriers = std::span(&pre_barrier, 1) });

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

	rec.build_acceleration_structure(build_info, { &range_ptr, 1 });

	const memory_barrier post_barrier{
		.src_stages = pipeline_stage_flag::acceleration_structure_build,
		.src_access = access_flag::acceleration_structure_write,
		.dst_stages = pipeline_stage_flag::acceleration_structure_build | pipeline_stage_flag::fragment_shader,
		.dst_access = access_flag::acceleration_structure_read | access_flag::acceleration_structure_write,
	};
	rec.pipeline_barrier(dependency_info{ .memory_barriers = std::span(&post_barrier, 1) });
}

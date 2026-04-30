module gse.gpu;

import std;
import vulkan;

import :acceleration_structure;
import :gpu_task;
import :transient_api;
import :types;
import :vulkan_acceleration_structure;
import :vulkan_buffer;
import :vulkan_commands;
import :vulkan_device;
import :device;

import gse.concurrency;

auto gse::build_blas_async(gpu::device& dev, const gpu::acceleration_structure_handle as_handle, gpu::acceleration_structure_geometry geometry, const std::uint32_t prim_count, const gpu::device_size scratch_size, const gpu::device_size scratch_alignment) -> async::task<> {
	const auto& dev_cfg = dev.vulkan_device();

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

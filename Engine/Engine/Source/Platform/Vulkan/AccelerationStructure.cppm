module;

#include <vulkan/vulkan_hpp_macros.hpp>

export module gse.platform:acceleration_structure;

import std;

import :gpu_types;
import :gpu_buffer;
import :gpu_device;
import :gpu_factory;
import :vulkan_allocator;
import :render_graph;

import gse.utility;
import gse.math;

export namespace gse::gpu {
	struct blas_geometry_desc {
		const buffer* vertex_buffer = nullptr;
		std::uint32_t vertex_count  = 0;
		std::uint32_t vertex_stride = 0;
		const buffer* index_buffer  = nullptr;
		std::uint32_t index_count   = 0;
	};

	struct blas {
		buffer storage;
		vk::raii::AccelerationStructureKHR handle{ nullptr };
		vk::DeviceAddress device_address = 0;
	};

	struct tlas {
		buffer storage;
		buffer scratch;
		buffer instance_buffer;
		vk::raii::AccelerationStructureKHR handle{ nullptr };
	};

	auto build_blas(device& dev, const blas_geometry_desc& desc) -> blas;
	auto build_tlas(device& dev, std::uint32_t max_instances) -> tlas;

	auto rebuild_tlas(
		device& dev,
		tlas& t,
		std::span<const vk::AccelerationStructureInstanceKHR> instances,
		vulkan::recording_context& ctx
	) -> void;
}

auto gse::gpu::build_blas(device& dev, const blas_geometry_desc& desc) -> blas {
	const auto& vk_device = dev.logical_device();

	const vk::DeviceAddress vertex_addr = vk_device.getBufferAddress({ .buffer = desc.vertex_buffer->native().buffer });
	const vk::DeviceAddress index_addr  = vk_device.getBufferAddress({ .buffer = desc.index_buffer->native().buffer });

	const vk::AccelerationStructureGeometryTrianglesDataKHR triangles{
		.vertexFormat = vk::Format::eR32G32B32Sfloat,
		.vertexData   = { .deviceAddress = vertex_addr },
		.vertexStride = desc.vertex_stride,
		.maxVertex    = desc.vertex_count - 1,
		.indexType    = vk::IndexType::eUint32,
		.indexData    = { .deviceAddress = index_addr }
	};

	const vk::AccelerationStructureGeometryKHR geometry{
		.geometryType = vk::GeometryTypeKHR::eTriangles,
		.geometry     = { .triangles = triangles },
		.flags        = vk::GeometryFlagBitsKHR::eOpaque
	};

	const std::uint32_t prim_count = desc.index_count / 3;

	vk::AccelerationStructureBuildGeometryInfoKHR build_info{
		.type          = vk::AccelerationStructureTypeKHR::eBottomLevel,
		.flags         = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace,
		.mode          = vk::BuildAccelerationStructureModeKHR::eBuild,
		.geometryCount = 1,
		.pGeometries   = &geometry
	};

	const auto sizes = vk_device.getAccelerationStructureBuildSizesKHR(
		vk::AccelerationStructureBuildTypeKHR::eDevice,
		build_info,
		prim_count
	);

	auto storage_buf = create_buffer(dev, {
		.size  = sizes.accelerationStructureSize,
		.usage = buffer_flag::acceleration_structure_storage
	});

	auto as = vk_device.createAccelerationStructureKHR({
		.buffer = storage_buf.native().buffer,
		.size   = sizes.accelerationStructureSize,
		.type   = vk::AccelerationStructureTypeKHR::eBottomLevel
	});

	build_info.dstAccelerationStructure = *as;

	const vk::DeviceSize scratch_size = sizes.buildScratchSize;

	dev.add_transient_work([&dev, &geometry, &build_info, prim_count, scratch_size](const auto& cmd) {
		auto scratch = dev.allocator().create_buffer({
			.size  = scratch_size,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress
		}, nullptr);

		build_info.pGeometries = &geometry;
		build_info.scratchData.deviceAddress = dev.logical_device().getBufferAddress({ .buffer = scratch.buffer });

		const vk::AccelerationStructureBuildRangeInfoKHR range{ .primitiveCount = prim_count };
		const vk::AccelerationStructureBuildRangeInfoKHR* range_ptr = &range;

		cmd.buildAccelerationStructuresKHR(build_info, range_ptr);

		constexpr vk::MemoryBarrier2 barrier{
			.srcStageMask  = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
			.srcAccessMask = vk::AccessFlagBits2::eAccelerationStructureWriteKHR,
			.dstStageMask  = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
			.dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureReadKHR
		};
		const vk::DependencyInfo dep{ 
			.memoryBarrierCount = 1, 
			.pMemoryBarriers = &barrier
		};
		cmd.pipelineBarrier2(dep);

		std::vector<vulkan::buffer_resource> transient_buffers;
		transient_buffers.push_back(std::move(scratch));
		return transient_buffers;
	});

	const vk::DeviceAddress device_address = vk_device.getAccelerationStructureAddressKHR({ .accelerationStructure = *as });

	return blas{
		.storage        = std::move(storage_buf),
		.handle         = std::move(as),
		.device_address = device_address
	};
}

auto gse::gpu::build_tlas(device& dev, const std::uint32_t max_instances) -> tlas {
	const auto& vk_device = dev.logical_device();

	const vk::AccelerationStructureGeometryInstancesDataKHR instances_data{
		.arrayOfPointers = vk::False
	};
	const vk::AccelerationStructureGeometryKHR geometry{
		.geometryType = vk::GeometryTypeKHR::eInstances,
		.geometry     = { .instances = instances_data }
	};

	const vk::AccelerationStructureBuildGeometryInfoKHR build_info{
		.type          = vk::AccelerationStructureTypeKHR::eTopLevel,
		.flags         = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate,
		.mode          = vk::BuildAccelerationStructureModeKHR::eBuild,
		.geometryCount = 1,
		.pGeometries   = &geometry
	};

	const auto sizes = vk_device.getAccelerationStructureBuildSizesKHR(
		vk::AccelerationStructureBuildTypeKHR::eDevice,
		build_info,
		max_instances
	);

	auto storage_buf = create_buffer(dev, {
		.size  = sizes.accelerationStructureSize,
		.usage = buffer_flag::acceleration_structure_storage
	});

	auto scratch_buf = buffer(dev.allocator().create_buffer({
		.size  = std::max(sizes.buildScratchSize, sizes.updateScratchSize),
		.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress
	}));

	const vk::DeviceSize instance_buf_size = max_instances * sizeof(vk::AccelerationStructureInstanceKHR);
	auto instance_buf = dev.allocator().create_buffer({
		.size  = instance_buf_size,
		.usage = vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress
	});

	auto as = vk_device.createAccelerationStructureKHR({
		.buffer = storage_buf.native().buffer,
		.size   = sizes.accelerationStructureSize,
		.type   = vk::AccelerationStructureTypeKHR::eTopLevel
	});

	return tlas{
		.storage         = std::move(storage_buf),
		.scratch         = std::move(scratch_buf),
		.instance_buffer = buffer(std::move(instance_buf)),
		.handle          = std::move(as)
	};
}

auto gse::gpu::rebuild_tlas(
	device& dev,
	tlas& t,
	const std::span<const vk::AccelerationStructureInstanceKHR> instances,
	vulkan::recording_context& ctx
) -> void {
	const auto& vk_device = dev.logical_device();

	auto* mapped = t.instance_buffer.mapped();
	if (mapped && !instances.empty()) {
		std::memcpy(mapped, instances.data(), instances.size_bytes());
	}

	const vk::DeviceAddress instance_addr = vk_device.getBufferAddress({ .buffer = t.instance_buffer.native().buffer });
	const vk::DeviceAddress scratch_addr  = vk_device.getBufferAddress({ .buffer = t.scratch.native().buffer });

	const vk::AccelerationStructureGeometryInstancesDataKHR instances_data{
		.arrayOfPointers = vk::False,
		.data            = { .deviceAddress = instance_addr }
	};
	const vk::AccelerationStructureGeometryKHR geometry{
		.geometryType = vk::GeometryTypeKHR::eInstances,
		.geometry     = { .instances = instances_data }
	};

	const vk::AccelerationStructureBuildGeometryInfoKHR build_info{
		.type                      = vk::AccelerationStructureTypeKHR::eTopLevel,
		.flags                     = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate,
		.mode                      = vk::BuildAccelerationStructureModeKHR::eBuild,
		.dstAccelerationStructure  = *t.handle,
		.geometryCount             = 1,
		.pGeometries               = &geometry,
		.scratchData               = { .deviceAddress = scratch_addr }
	};

	const vk::AccelerationStructureBuildRangeInfoKHR range{
		.primitiveCount = static_cast<std::uint32_t>(instances.size())
	};
	const vk::AccelerationStructureBuildRangeInfoKHR* range_ptr = &range;

	ctx.build_acceleration_structure(build_info, { &range_ptr, 1 });

	const vk::MemoryBarrier2 barrier{
		.srcStageMask  = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
		.srcAccessMask = vk::AccessFlagBits2::eAccelerationStructureWriteKHR,
		.dstStageMask  = vk::PipelineStageFlagBits2::eRayTracingShaderKHR | vk::PipelineStageFlagBits2::eFragmentShader,
		.dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureReadKHR
	};
	const vk::DependencyInfo dep{ .memoryBarrierCount = 1, .pMemoryBarriers = &barrier };
	ctx.pipeline_barrier(dep);
}

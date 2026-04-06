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
import gse.log;

export namespace gse::gpu {
	struct blas_geometry_desc {
		const buffer* vertex_buffer = nullptr;
		std::uint32_t vertex_count  = 0;
		std::uint32_t vertex_stride = 0;
		const buffer* index_buffer  = nullptr;
		std::uint32_t index_count   = 0;
	};

	struct acceleration_structure_instance {
		mat4f transform = mat4(1.0f);
		std::uint32_t custom_index = 0;
		std::uint8_t mask = 0xFF;
		std::uint32_t sbt_offset = 0;
		bool cull_disable = true;
		std::uint64_t blas_address = 0;
	};

	class blas final : public non_copyable {
	public:
		blas() = default;
		blas(
			buffer&& storage,
			vk::raii::AccelerationStructureKHR&& handle,
			vk::DeviceAddress addr
		);
		blas(blas&&) noexcept = default;
		auto operator=(blas&&) noexcept -> blas& = default;

		[[nodiscard]] auto native_handle
		(this const blas& self
		) -> acceleration_structure_handle;

		[[nodiscard]] auto device_address(
			this const blas& self
		) -> std::uint64_t;

		explicit operator bool(
		) const;
	private:
		buffer m_storage;
		vk::raii::AccelerationStructureKHR m_handle{ nullptr };
		vk::DeviceAddress m_device_address = 0;
	};

	class tlas final : public non_copyable {
	public:
		tlas() = default;
		tlas(
			buffer&& storage,
			buffer&& scratch,
			buffer&& instance_buffer,
			vk::raii::AccelerationStructureKHR&& handle
		);

		tlas(tlas&&) noexcept = default;
		auto operator=(tlas&&) noexcept -> tlas& = default;

		[[nodiscard]] auto native_handle(
			this const tlas& self
		) -> acceleration_structure_handle;

		explicit operator bool(
		) const;
	private:
		buffer m_storage;
		buffer m_scratch;
		buffer m_instance_buffer;
		vk::raii::AccelerationStructureKHR m_handle{ nullptr };

		friend auto rebuild_tlas(device&, tlas&, std::span<const acceleration_structure_instance>, vulkan::recording_context&) -> void;
	};

	auto build_blas(
		device& dev, 
		const blas_geometry_desc& desc
	) -> blas;

	auto build_tlas(
		device& dev, 
		std::uint32_t max_instances
	) -> tlas;

	auto rebuild_tlas(
		device& dev,
		tlas& t,
		std::span<const acceleration_structure_instance> instances,
		vulkan::recording_context& ctx
	) -> void;

}

namespace gse::gpu {
	auto to_vk_instance(
		const acceleration_structure_instance& inst
	) -> vk::AccelerationStructureInstanceKHR;

	auto acceleration_structure_scratch_alignment(
		device& dev
	) -> vk::DeviceSize;
}

auto gse::gpu::to_vk_instance(const acceleration_structure_instance& inst) -> vk::AccelerationStructureInstanceKHR {
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
		.accelerationStructureReference = inst.blas_address
	};
}

auto gse::gpu::acceleration_structure_scratch_alignment(device& dev) -> vk::DeviceSize {
	const auto props = dev.physical_device().getProperties2<
		vk::PhysicalDeviceProperties2,
		vk::PhysicalDeviceAccelerationStructurePropertiesKHR
	>();
	const auto& as_props = props.get<vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();
	return std::max<vk::DeviceSize>(as_props.minAccelerationStructureScratchOffsetAlignment, 1);
}

auto gse::gpu::build_blas(device& dev, const blas_geometry_desc& desc) -> blas {
	const auto& vk_device = dev.logical_device();

	const vk::DeviceAddress vertex_addr = vk_device.getBufferAddress({
		.buffer = desc.vertex_buffer->native().buffer 
	});

	const vk::DeviceAddress index_addr  = vk_device.getBufferAddress({ 
		.buffer = desc.index_buffer->native().buffer
	});

	const vk::AccelerationStructureGeometryTrianglesDataKHR triangles{
		.vertexFormat = vk::Format::eR32G32B32Sfloat,
		.vertexData = { 
			.deviceAddress = vertex_addr
		},
		.vertexStride = desc.vertex_stride,
		.maxVertex = desc.vertex_count - 1,
		.indexType = vk::IndexType::eUint32,
		.indexData = { 
			.deviceAddress = index_addr
		}
	};

	const vk::AccelerationStructureGeometryKHR geometry{
		.geometryType = vk::GeometryTypeKHR::eTriangles,
		.geometry = { 
			.triangles = triangles 
		},
		.flags = vk::GeometryFlagBitsKHR::eOpaque
	};

	const std::uint32_t prim_count = desc.index_count / 3;

	vk::AccelerationStructureBuildGeometryInfoKHR build_info{
		.type = vk::AccelerationStructureTypeKHR::eBottomLevel,
		.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace,
		.mode = vk::BuildAccelerationStructureModeKHR::eBuild,
		.geometryCount = 1,
		.pGeometries = &geometry
	};

	const auto sizes = vk_device.getAccelerationStructureBuildSizesKHR(
		vk::AccelerationStructureBuildTypeKHR::eDevice,
		build_info,
		prim_count
	);

	auto storage_buf = create_buffer(dev, {
		.size = sizes.accelerationStructureSize,
		.usage = buffer_flag::acceleration_structure_storage
	});

	auto as = vk_device.createAccelerationStructureKHR({
		.buffer = storage_buf.native().buffer,
		.size = sizes.accelerationStructureSize,
		.type = vk::AccelerationStructureTypeKHR::eBottomLevel
	});

	build_info.dstAccelerationStructure = *as;

	const vk::DeviceSize scratch_size = sizes.buildScratchSize;
	const vk::DeviceSize scratch_alignment = acceleration_structure_scratch_alignment(dev);
	log::println(log::category::render,
		"AS BLAS build: prims={} vertex_addr={:#x} index_addr={:#x} storage_size={} scratch_size={} scratch_alignment={}",
		prim_count,
		vertex_addr,
		index_addr,
		sizes.accelerationStructureSize,
		scratch_size,
		scratch_alignment);

	dev.add_transient_work([&dev, &geometry, &build_info, prim_count, scratch_size, scratch_alignment](const vk::raii::CommandBuffer& cmd) {
		auto scratch = dev.allocator().create_buffer({
			.size  = scratch_size + scratch_alignment,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress
		}, nullptr);

		build_info.pGeometries = &geometry;
		const auto scratch_addr = dev.logical_device().getBufferAddress({
			.buffer = scratch.buffer
		});
		build_info.scratchData.deviceAddress = (scratch_addr + scratch_alignment - 1) & ~(scratch_alignment - 1);

		const vk::AccelerationStructureBuildRangeInfoKHR range{ 
			.primitiveCount = prim_count
		};
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

	const vk::DeviceAddress device_address = vk_device.getAccelerationStructureAddressKHR({ 
		.accelerationStructure = *as 
	});

	return blas{
		std::move(storage_buf),
		std::move(as),
		device_address
	};
}

auto gse::gpu::build_tlas(device& dev, const std::uint32_t max_instances) -> tlas {
	const auto& vk_device = dev.logical_device();

	constexpr vk::AccelerationStructureGeometryInstancesDataKHR instances_data{
		.arrayOfPointers = vk::False
	};

	constexpr vk::AccelerationStructureGeometryKHR geometry{
		.geometryType = vk::GeometryTypeKHR::eInstances,
		.geometry = { 
			.instances = instances_data 
		}
	};

	const vk::AccelerationStructureBuildGeometryInfoKHR build_info{
		.type = vk::AccelerationStructureTypeKHR::eTopLevel,
		.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate,
		.mode = vk::BuildAccelerationStructureModeKHR::eBuild,
		.geometryCount = 1,
		.pGeometries = &geometry
	};

	const auto sizes = vk_device.getAccelerationStructureBuildSizesKHR(
		vk::AccelerationStructureBuildTypeKHR::eDevice,
		build_info,
		max_instances
	);

	auto storage_buf = create_buffer(dev, {
		.size = sizes.accelerationStructureSize,
		.usage = buffer_flag::acceleration_structure_storage
	});

	const vk::DeviceSize scratch_alignment = acceleration_structure_scratch_alignment(dev);
	auto scratch_buf = buffer(dev.allocator().create_buffer({
		.size = std::max(sizes.buildScratchSize, sizes.updateScratchSize) + scratch_alignment,
		.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress
	}));

	const vk::DeviceSize instance_buf_size = max_instances * sizeof(vk::AccelerationStructureInstanceKHR);
	auto instance_buf = dev.allocator().create_buffer({
		.size = instance_buf_size,
		.usage = vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress
	});

	auto as = vk_device.createAccelerationStructureKHR({
		.buffer = storage_buf.native().buffer,
		.size = sizes.accelerationStructureSize,
		.type = vk::AccelerationStructureTypeKHR::eTopLevel
	});

	const auto scratch_buffer_addr = vk_device.getBufferAddress({
		.buffer = scratch_buf.native().buffer
	});
	const auto instance_buffer_addr = vk_device.getBufferAddress({
		.buffer = instance_buf.buffer
	});
	log::println(log::category::render,
		"AS TLAS create: max_instances={} handle={:#x} storage_size={} build_scratch={} update_scratch={} scratch_alignment={} scratch_addr={:#x} instance_addr={:#x}",
		max_instances,
		reinterpret_cast<std::uint64_t>(static_cast<VkAccelerationStructureKHR>(*as)),
		sizes.accelerationStructureSize,
		sizes.buildScratchSize,
		sizes.updateScratchSize,
		scratch_alignment,
		scratch_buffer_addr,
		instance_buffer_addr);

	return tlas{
		std::move(storage_buf),
		std::move(scratch_buf),
		buffer(std::move(instance_buf)),
		std::move(as)
	};
}

auto gse::gpu::rebuild_tlas(device& dev, tlas& t, const std::span<const acceleration_structure_instance> instances, vulkan::recording_context& ctx) -> void {
	const auto& vk_device = dev.logical_device();
	const auto transform_is_finite = [](const mat4f& transform) {
		for (int row = 0; row < 4; ++row) {
			for (int col = 0; col < 4; ++col) {
				if (!std::isfinite(transform[col][row])) {
					return false;
				}
			}
		}
		return true;
	};

	std::vector<vk::AccelerationStructureInstanceKHR> vk_instances;
	vk_instances.reserve(instances.size());
	std::size_t invalid_blas_count = 0;
	std::size_t nonfinite_transform_count = 0;
	for (const auto& inst : instances) {
		if (inst.blas_address == 0) {
			++invalid_blas_count;
		}
		if (!transform_is_finite(inst.transform)) {
			++nonfinite_transform_count;
		}
		vk_instances.push_back(to_vk_instance(inst));
	}

	if (auto* mapped = t.m_instance_buffer.mapped(); mapped && !vk_instances.empty()) {
		std::memcpy(mapped, vk_instances.data(), vk_instances.size() * sizeof(vk::AccelerationStructureInstanceKHR));
	}

	const vk::DeviceAddress instance_addr = vk_device.getBufferAddress({ 
		.buffer = t.m_instance_buffer.native().buffer
	});

	const vk::DeviceSize scratch_alignment = acceleration_structure_scratch_alignment(dev);
	const vk::DeviceAddress scratch_raw = vk_device.getBufferAddress({
		.buffer = t.m_scratch.native().buffer
	});
	const vk::DeviceAddress scratch_addr = (scratch_raw + scratch_alignment - 1) & ~(scratch_alignment - 1);
	log::println(log::category::render,
		"AS TLAS rebuild: instances={} handle={:#x} instance_addr={:#x} scratch_addr={:#x} invalid_blas={} nonfinite_transforms={}",
		instances.size(),
		reinterpret_cast<std::uint64_t>(static_cast<VkAccelerationStructureKHR>(*t.m_handle)),
		instance_addr,
		scratch_addr,
		invalid_blas_count,
		nonfinite_transform_count);
	if (invalid_blas_count > 0 || nonfinite_transform_count > 0) {
		log::println(log::level::warning, log::category::render,
			"AS TLAS rebuild has suspect inputs: invalid_blas={} nonfinite_transforms={}",
			invalid_blas_count,
			nonfinite_transform_count);
	}

	constexpr vk::MemoryBarrier2 host_write_barrier{
		.srcStageMask = vk::PipelineStageFlagBits2::eHost,
		.srcAccessMask = vk::AccessFlagBits2::eHostWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
		.dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureReadKHR
	};
	const vk::DependencyInfo host_dep{
		.memoryBarrierCount = 1,
		.pMemoryBarriers = &host_write_barrier
	};
	ctx.pipeline_barrier(host_dep);

	const vk::AccelerationStructureGeometryInstancesDataKHR instances_data{
		.arrayOfPointers = vk::False,
		.data = { 
			.deviceAddress = instance_addr
		}
	};
	const vk::AccelerationStructureGeometryKHR geometry{
		.geometryType = vk::GeometryTypeKHR::eInstances,
		.geometry = {
			.instances = instances_data
		}
	};

	const vk::AccelerationStructureBuildGeometryInfoKHR build_info{
		.type = vk::AccelerationStructureTypeKHR::eTopLevel,
		.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate,
		.mode = vk::BuildAccelerationStructureModeKHR::eBuild,
		.dstAccelerationStructure = *t.m_handle,
		.geometryCount = 1,
		.pGeometries = &geometry,
		.scratchData = { 
			.deviceAddress = scratch_addr 
		}
	};

	const vk::AccelerationStructureBuildRangeInfoKHR range{
		.primitiveCount = static_cast<std::uint32_t>(vk_instances.size())
	};
	const vk::AccelerationStructureBuildRangeInfoKHR* range_ptr = &range;

	ctx.build_acceleration_structure(build_info, { &range_ptr, 1 });

	constexpr vk::MemoryBarrier2 barrier{
		.srcStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
		.srcAccessMask = vk::AccessFlagBits2::eAccelerationStructureWriteKHR,
		.dstStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR | vk::PipelineStageFlagBits2::eFragmentShader,
		.dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureReadKHR | vk::AccessFlagBits2::eAccelerationStructureWriteKHR
	};
	const vk::DependencyInfo dep{
		.memoryBarrierCount = 1,
		.pMemoryBarriers = &barrier
	};
	ctx.pipeline_barrier(dep);
}

gse::gpu::blas::blas(buffer&& storage, vk::raii::AccelerationStructureKHR&& handle, const vk::DeviceAddress addr) : m_storage(std::move(storage)), m_handle(std::move(handle)), m_device_address(addr) {}

auto gse::gpu::blas::native_handle(this const blas& self) -> acceleration_structure_handle {
	return { reinterpret_cast<std::uint64_t>(static_cast<VkAccelerationStructureKHR>(*self.m_handle)) };
}

auto gse::gpu::blas::device_address(this const blas& self) -> std::uint64_t {
	return self.m_device_address;
}

gse::gpu::blas::operator bool() const {
	return *m_handle != nullptr;
}

gse::gpu::tlas::tlas(buffer&& storage, buffer&& scratch, buffer&& instance_buffer, vk::raii::AccelerationStructureKHR&& handle) : m_storage(std::move(storage)), m_scratch(std::move(scratch)), m_instance_buffer(std::move(instance_buffer)), m_handle(std::move(handle)) {}

auto gse::gpu::tlas::native_handle(this const tlas& self) -> acceleration_structure_handle {
	return { reinterpret_cast<std::uint64_t>(static_cast<VkAccelerationStructureKHR>(*self.m_handle)) };
}

gse::gpu::tlas::operator bool() const {
	return *m_handle != nullptr;
}

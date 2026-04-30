export module gse.gpu:vulkan_acceleration_structure;

import std;
import vulkan;

import :handles;
import :types;
import :vulkan_device;
import :vulkan_buffer;

import gse.core;

export namespace gse::vulkan {
	struct acceleration_structure_build_sizes {
		gpu::device_size acceleration_structure_size = 0;
		gpu::device_size build_scratch_size = 0;
		gpu::device_size update_scratch_size = 0;
	};

	[[nodiscard]] auto buffer_device_address(
		const device& dev,
		gpu::handle<buffer> buffer
	) -> gpu::device_address;

	[[nodiscard]] auto query_blas_build_sizes(
		const device& dev,
		const gpu::acceleration_structure_geometry& geometry,
		std::uint32_t prim_count
	) -> acceleration_structure_build_sizes;

	[[nodiscard]] auto query_tlas_build_sizes(
		const device& dev,
		std::uint32_t max_instances
	) -> acceleration_structure_build_sizes;

	[[nodiscard]] auto create_acceleration_structure(
		const device& dev,
		gpu::handle<buffer> storage_buffer,
		gpu::device_size size,
		gpu::acceleration_structure_type type
	) -> vk::raii::AccelerationStructureKHR;

	[[nodiscard]] auto acceleration_structure_address(
		const device& dev,
		const vk::raii::AccelerationStructureKHR& as
	) -> gpu::device_address;

	[[nodiscard]] auto acceleration_structure_address_from_handle(
		gpu::handle<gpu::device> device_handle,
		gpu::acceleration_structure_handle as_handle
	) -> gpu::device_address;

	[[nodiscard]] auto scratch_offset_alignment(
		const device& dev
	) -> gpu::device_size;
}

namespace {
	auto to_vk_geometry(const gse::gpu::acceleration_structure_geometry& g) -> vk::AccelerationStructureGeometryKHR {
		vk::AccelerationStructureGeometryDataKHR data{};
		vk::GeometryTypeKHR vk_type = vk::GeometryTypeKHR::eInstances;
		if (g.type == gse::gpu::acceleration_structure_geometry_type::triangles) {
			vk_type = vk::GeometryTypeKHR::eTriangles;
			data.triangles = vk::AccelerationStructureGeometryTrianglesDataKHR{
				.vertexFormat = gse::vulkan::to_vk(g.triangles.vertex_format),
				.vertexData = vk::DeviceOrHostAddressConstKHR{ .deviceAddress = g.triangles.vertex_data },
				.vertexStride = g.triangles.vertex_stride,
				.maxVertex = g.triangles.max_vertex,
				.indexType = gse::vulkan::to_vk(g.triangles.index_type),
				.indexData = vk::DeviceOrHostAddressConstKHR{ .deviceAddress = g.triangles.index_data },
			};
		} else {
			data.instances = vk::AccelerationStructureGeometryInstancesDataKHR{
				.arrayOfPointers = g.instances.array_of_pointers ? vk::True : vk::False,
				.data = vk::DeviceOrHostAddressConstKHR{ .deviceAddress = g.instances.data },
			};
		}
		return vk::AccelerationStructureGeometryKHR{
			.geometryType = vk_type,
			.geometry = data,
			.flags = gse::vulkan::to_vk(g.flags),
		};
	}
}

auto gse::vulkan::buffer_device_address(const device& dev, const gpu::handle<buffer> buffer) -> gpu::device_address {
	return dev.raii_device().getBufferAddress({
		.buffer = std::bit_cast<vk::Buffer>(buffer),
	});
}

auto gse::vulkan::query_blas_build_sizes(const device& dev, const gpu::acceleration_structure_geometry& geometry, const std::uint32_t prim_count) -> acceleration_structure_build_sizes {
	const auto vk_geometry = to_vk_geometry(geometry);
	const vk::AccelerationStructureBuildGeometryInfoKHR sizing_info{
		.type = vk::AccelerationStructureTypeKHR::eBottomLevel,
		.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild,
		.mode = vk::BuildAccelerationStructureModeKHR::eBuild,
		.geometryCount = 1,
		.pGeometries = &vk_geometry,
	};
	const auto sizes = dev.raii_device().getAccelerationStructureBuildSizesKHR(
		vk::AccelerationStructureBuildTypeKHR::eDevice,
		sizing_info,
		prim_count
	);
	return {
		.acceleration_structure_size = sizes.accelerationStructureSize,
		.build_scratch_size = sizes.buildScratchSize,
		.update_scratch_size = sizes.updateScratchSize,
	};
}

auto gse::vulkan::query_tlas_build_sizes(const device& dev, const std::uint32_t max_instances) -> acceleration_structure_build_sizes {
	constexpr vk::AccelerationStructureGeometryInstancesDataKHR instances_data{
		.arrayOfPointers = vk::False,
	};
	constexpr vk::AccelerationStructureGeometryKHR vk_geometry{
		.geometryType = vk::GeometryTypeKHR::eInstances,
		.geometry = { .instances = instances_data },
	};
	const vk::AccelerationStructureBuildGeometryInfoKHR sizing_info{
		.type = vk::AccelerationStructureTypeKHR::eTopLevel,
		.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate,
		.mode = vk::BuildAccelerationStructureModeKHR::eBuild,
		.geometryCount = 1,
		.pGeometries = &vk_geometry,
	};
	const auto sizes = dev.raii_device().getAccelerationStructureBuildSizesKHR(
		vk::AccelerationStructureBuildTypeKHR::eDevice,
		sizing_info,
		max_instances
	);
	return {
		.acceleration_structure_size = sizes.accelerationStructureSize,
		.build_scratch_size = sizes.buildScratchSize,
		.update_scratch_size = sizes.updateScratchSize,
	};
}

auto gse::vulkan::create_acceleration_structure(const device& dev, const gpu::handle<buffer> storage_buffer, const gpu::device_size size, const gpu::acceleration_structure_type type) -> vk::raii::AccelerationStructureKHR {
	return dev.raii_device().createAccelerationStructureKHR({
		.buffer = std::bit_cast<vk::Buffer>(storage_buffer),
		.size = size,
		.type = to_vk(type),
	});
}

auto gse::vulkan::acceleration_structure_address(const device& dev, const vk::raii::AccelerationStructureKHR& as) -> gpu::device_address {
	return dev.raii_device().getAccelerationStructureAddressKHR({
		.accelerationStructure = *as,
	});
}

auto gse::vulkan::acceleration_structure_address_from_handle(const gpu::handle<gpu::device> device_handle, const gpu::acceleration_structure_handle as_handle) -> gpu::device_address {
	const auto vk_device = std::bit_cast<vk::Device>(device_handle);
	const auto vk_as = std::bit_cast<vk::AccelerationStructureKHR>(as_handle.value);
	return vk_device.getAccelerationStructureAddressKHR({
		.accelerationStructure = vk_as,
	});
}

auto gse::vulkan::scratch_offset_alignment(const device& dev) -> gpu::device_size {
	const auto props = dev.physical_device().getProperties2<
		vk::PhysicalDeviceProperties2,
		vk::PhysicalDeviceAccelerationStructurePropertiesKHR
	>();
	const auto& as_props = props.get<vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();
	return std::max<gpu::device_size>(as_props.minAccelerationStructureScratchOffsetAlignment, 1);
}

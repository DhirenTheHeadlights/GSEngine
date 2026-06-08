export module gse.vulkan:acceleration_structure;

import std;
import vulkan;

import :handles;
import :types;

import gse.core;
import gse.math;

export namespace gse::vulkan {
	using as_instance = vk::AccelerationStructureInstanceKHR;

	[[nodiscard]]
	auto pack_instance(
		const spatial_matrix& transform,
		std::uint32_t custom_index,
		std::uint8_t mask,
		std::uint32_t sbt_offset,
		bool cull_disable,
		std::uint64_t blas_address
	) -> as_instance;

	[[nodiscard]]
	auto acceleration_structure_address_from_handle(
		gpu::device_handle device_handle,
		gpu::acceleration_structure as_handle
	) -> gpu::device_address;
}

auto gse::vulkan::pack_instance(const spatial_matrix& transform, const std::uint32_t custom_index, const std::uint8_t mask, const std::uint32_t sbt_offset, const bool cull_disable, const std::uint64_t blas_address) -> as_instance {
	const auto* m = reinterpret_cast<const float*>(&transform);
	vk::TransformMatrixKHR vk_transform{};
	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 4; ++col) {
			vk_transform.matrix[row][col] = m[col * 4 + row];
		}
	}

	return {
		.transform = vk_transform,
		.instanceCustomIndex = custom_index,
		.mask = mask,
		.instanceShaderBindingTableRecordOffset = sbt_offset,
		.flags = static_cast<std::uint8_t>(
			cull_disable ? vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable
						 : vk::GeometryInstanceFlagBitsKHR{}
		),
		.accelerationStructureReference = blas_address,
	};
}

auto gse::vulkan::acceleration_structure_address_from_handle(const gpu::device_handle device_handle, const gpu::acceleration_structure as_handle) -> gpu::device_address {
	const auto vk_device = std::bit_cast<vk::Device>(device_handle);
	const auto vk_as = std::bit_cast<vk::AccelerationStructureKHR>(as_handle.value);
	return vk_device.getAccelerationStructureAddressKHR({
		.accelerationStructure = vk_as,
	});
}

export module gse.vulkan:acceleration_structure;

import std;
import vulkan;

import :handles;
import :types;
import :buffer;

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

	class blas final : public non_copyable {
	public:
		blas() {}
		blas(
			buffer storage,
			gpu::acceleration_structure acceleration_structure,
			gpu::device_address device_address
		);

		~blas() = default;

		blas(
			blas&&
		) noexcept = default;

		auto operator=(
			blas&&
		) noexcept -> blas& = default;

		[[nodiscard]] auto handle() const -> gpu::acceleration_structure;

		[[nodiscard]] auto device_address() const -> gpu::device_address;

		[[nodiscard]] auto valid() const -> bool;

	private:
		buffer m_storage;
		gpu::acceleration_structure m_acceleration_structure;
		gpu::device_address m_device_address = 0;
	};

	class tlas final : public non_copyable {
	public:
		tlas() {}
		tlas(
			buffer storage,
			buffer scratch,
			buffer instance_buffer,
			gpu::acceleration_structure acceleration_structure
		);

		~tlas() = default;

		tlas(
			tlas&&
		) noexcept = default;

		auto operator=(
			tlas&&
		) noexcept -> tlas& = default;

		[[nodiscard]] auto handle() const -> gpu::acceleration_structure;

		[[nodiscard]] auto instance_buffer(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto scratch_buffer() const -> const buffer&;

		[[nodiscard]] auto valid() const -> bool;

	private:
		buffer m_storage;
		buffer m_scratch;
		buffer m_instance_buffer;
		gpu::acceleration_structure m_acceleration_structure;
	};
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

gse::vulkan::blas::blas(buffer storage, const gpu::acceleration_structure acceleration_structure, const gpu::device_address device_address)
	: m_storage(std::move(storage)), m_acceleration_structure(acceleration_structure), m_device_address(device_address) {
}

auto gse::vulkan::blas::handle() const -> gpu::acceleration_structure {
	return m_acceleration_structure;
}

auto gse::vulkan::blas::device_address() const -> gpu::device_address {
	return m_device_address;
}

auto gse::vulkan::blas::valid() const -> bool {
	return static_cast<bool>(m_acceleration_structure);
}

gse::vulkan::tlas::tlas(buffer storage, buffer scratch, buffer instance_buffer, const gpu::acceleration_structure acceleration_structure)
	: m_storage(std::move(storage)), m_scratch(std::move(scratch)), m_instance_buffer(std::move(instance_buffer)), m_acceleration_structure(acceleration_structure) {
}

auto gse::vulkan::tlas::handle() const -> gpu::acceleration_structure {
	return m_acceleration_structure;
}

auto gse::vulkan::tlas::instance_buffer(this auto& self) -> auto& {
	return self.m_instance_buffer;
}

auto gse::vulkan::tlas::scratch_buffer() const -> const buffer& {
	return m_scratch;
}

auto gse::vulkan::tlas::valid() const -> bool {
	return static_cast<bool>(m_acceleration_structure);
}

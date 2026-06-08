export module gse.vulkan:physical_device;

import std;
import vulkan;

import :handles;
import :types;

import gse.core;
import gse.assert;

export namespace gse::vulkan {
	class physical_device : public non_copyable {
	public:
		physical_device() {}
		explicit physical_device(
			vk::raii::PhysicalDevice&& device
		);

		~physical_device() = default;

		physical_device(
			physical_device&&
		) noexcept = default;

		auto operator=(
			physical_device&&
		) noexcept -> physical_device& = default;

		[[nodiscard]] auto handle() const -> gpu::handle<gpu::physical_device>;

		[[nodiscard]] auto valid() const -> bool;

		[[nodiscard]] auto is_discrete() const -> bool;

		[[nodiscard]] auto timestamp_period() const -> float;

		[[nodiscard]] auto memory_properties() const -> vk::PhysicalDeviceMemoryProperties;

		[[nodiscard]] auto scratch_offset_alignment() const -> gpu::device_size;

		[[nodiscard]] auto create_device(
			const vk::DeviceCreateInfo& create_info
		) const -> vk::raii::Device;

	private:
		vk::raii::PhysicalDevice m_physical_device{ nullptr };
	};
}

gse::vulkan::physical_device::physical_device(vk::raii::PhysicalDevice&& device)
	: m_physical_device(std::move(device)) {
}

auto gse::vulkan::physical_device::handle() const -> gpu::handle<gpu::physical_device> {
	return std::bit_cast<gpu::handle<gpu::physical_device>>(*m_physical_device);
}

auto gse::vulkan::physical_device::valid() const -> bool {
	return *m_physical_device != nullptr;
}

auto gse::vulkan::physical_device::is_discrete() const -> bool {
	return m_physical_device.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
}

auto gse::vulkan::physical_device::timestamp_period() const -> float {
	return m_physical_device.getProperties().limits.timestampPeriod;
}

auto gse::vulkan::physical_device::memory_properties() const -> vk::PhysicalDeviceMemoryProperties {
	return m_physical_device.getMemoryProperties();
}

auto gse::vulkan::physical_device::scratch_offset_alignment() const -> gpu::device_size {
	const auto props =
		m_physical_device
		.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();
	const auto& as_props = props.get<vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();
	return std::max<gpu::device_size>(as_props.minAccelerationStructureScratchOffsetAlignment, 1);
}

auto gse::vulkan::physical_device::create_device(const vk::DeviceCreateInfo& create_info) const -> vk::raii::Device {
	auto [result, device] = m_physical_device.createDevice(create_info);
	assert(result == vk::Result::eSuccess, "failed to create logical device: {}", vk::to_string(result));
	return std::move(device);
}

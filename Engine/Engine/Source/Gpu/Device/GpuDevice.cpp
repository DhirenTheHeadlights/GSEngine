module gse.gpu;

import std;
import vulkan;

import :device;
import :vulkan_runtime;
import :command_pools;
import :descriptor_heap;
import :device_bootstrap;
import :types;

import gse.os;
import gse.log;
import gse.save;

auto gse::gpu::device::create(const window& win, save::state& save) -> std::unique_ptr<device> {
    return std::unique_ptr<device>(new device(vulkan::bootstrap_device(win, save)));
}

gse::gpu::device::device(vulkan::bootstrap_result&& boot)
    : m_instance(std::move(boot.instance)),
      m_device_config(std::move(boot.device)),
      m_allocator(std::move(boot.alloc)),
      m_queue(std::move(boot.queue)),
      m_command(std::move(boot.command)),
      m_worker_pools(std::move(boot.worker_pools)),
      m_descriptor_heap(std::move(boot.desc_heap)),
      m_descriptor_buffer_props(std::move(boot.desc_buf_props)),
      m_surface_format(boot.surface_format),
      m_video_encode_enabled(boot.video_encode_enabled) {}

gse::gpu::device::~device() {
    log::println(log::category::runtime, "Destroying Device");
}

auto gse::gpu::device::physical_device() -> const vk::raii::PhysicalDevice& {
    return m_device_config.physical_device;
}

auto gse::gpu::device::allocator() -> vulkan::allocator& {
    return *m_allocator;
}

auto gse::gpu::device::allocator() const -> const vulkan::allocator& {
    return *m_allocator;
}

auto gse::gpu::device::surface_format() const -> vk::Format {
    return m_surface_format;
}

auto gse::gpu::device::compute_queue_family() const -> std::uint32_t {
    return m_queue.compute_family_index;
}

auto gse::gpu::device::compute_queue() -> const vk::raii::Queue& {
    return m_queue.compute;
}

auto gse::gpu::device::wait_idle() const -> void {
    m_device_config.device.waitIdle();
}

auto gse::gpu::device::report_device_lost(const std::string_view operation) -> void {
    if (bool expected = false; !m_device_lost_reported.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
        return;
    }

    log::println(log::level::error, log::category::vulkan, "Vulkan device lost during {}", operation);

    if (!m_device_config.device_fault_enabled) {
        log::println(log::level::warning, log::category::vulkan, "VK_EXT_device_fault is unavailable on this device");
        return;
    }

    vk::DeviceFaultCountsEXT counts{};
    if (const auto result = m_device_config.device.getFaultInfoEXT(&counts, nullptr); result != vk::Result::eSuccess) {
        log::println(log::level::warning, log::category::vulkan, "Failed to query device fault counts: {}", vk::to_string(result));
        return;
    }

    std::vector<vk::DeviceFaultAddressInfoEXT> address_infos(counts.addressInfoCount);
    std::vector<vk::DeviceFaultVendorInfoEXT> vendor_infos(counts.vendorInfoCount);
    std::vector<std::byte> vendor_binary(
        m_device_config.device_fault_vendor_binary_enabled ? counts.vendorBinarySize : 0
    );

    counts.addressInfoCount = static_cast<std::uint32_t>(address_infos.size());
    counts.vendorInfoCount = static_cast<std::uint32_t>(vendor_infos.size());
    counts.vendorBinarySize = vendor_binary.size();

    vk::DeviceFaultInfoEXT fault_info{
        .pAddressInfos = address_infos.empty() ? nullptr : address_infos.data(),
        .pVendorInfos = vendor_infos.empty() ? nullptr : vendor_infos.data(),
        .pVendorBinaryData = vendor_binary.empty() ? nullptr : vendor_binary.data(),
    };

    if (const auto result = m_device_config.device.getFaultInfoEXT(&counts, &fault_info); result != vk::Result::eSuccess) {
        log::println(log::level::warning, log::category::vulkan, "Failed to query device fault info: {}", vk::to_string(result));
        return;
    }

    const char* description = fault_info.description.data();
    log::println(
        log::level::error,
        log::category::vulkan,
        "Device fault description: {}",
        (description && description[0] != '\0') ? std::string_view(description) : std::string_view("(no description)")
    );

    for (std::size_t i = 0; i < address_infos.size(); ++i) {
        const auto& [addressType, reportedAddress, addressPrecision] = address_infos[i];
        log::println(
            log::level::error,
            log::category::vulkan,
            "Fault address {}: type={}, reported=0x{:x}, precision=0x{:x}",
            i,
            vk::to_string(addressType),
            reportedAddress,
            addressPrecision
        );
    }

    for (std::size_t i = 0; i < vendor_infos.size(); ++i) {
        const auto& [description, vendorFaultCode, vendorFaultData] = vendor_infos[i];
        const char* vendor_description = description.data();
        log::println(
            log::level::error,
            log::category::vulkan,
            "Vendor fault {}: '{}' code=0x{:x} data=0x{:x}",
            i,
            (vendor_description && vendor_description[0] != '\0') ? std::string_view(vendor_description) : std::string_view("(no description)"),
            vendorFaultCode,
            vendorFaultData
        );
    }

    if (!vendor_binary.empty()) {
        log::println(log::level::error, log::category::vulkan, "Device fault vendor binary size: {} bytes", vendor_binary.size());
    }
}

auto gse::gpu::device::queue_config() -> vulkan::queue_config& {
    return m_queue;
}

auto gse::gpu::device::command_config() -> vulkan::command_config& {
    return m_command;
}

auto gse::gpu::device::worker_command_pools() -> vulkan::worker_command_pools& {
    return m_worker_pools;
}

auto gse::gpu::device::descriptor_buffer_props() const -> const gpu::descriptor_buffer_properties& {
    return m_descriptor_buffer_props;
}

auto gse::gpu::device::video_encode_enabled() const -> bool {
    return m_video_encode_enabled;
}

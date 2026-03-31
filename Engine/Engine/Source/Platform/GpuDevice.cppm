export module gse.platform:gpu_device;

import std;

import :vulkan_runtime;
import :vulkan_allocator;
import :descriptor_heap;

import gse.utility;

export namespace gse::gpu {
	class device final : public non_copyable {
	public:
		explicit device(vulkan::runtime& runtime);

		[[nodiscard]] auto logical_device(
			this auto&& self
		) -> decltype(auto);

		[[nodiscard]] auto physical_device(
		) -> const vk::raii::PhysicalDevice&;

		[[nodiscard]] auto allocator(
		) -> vulkan::allocator&;

		[[nodiscard]] auto allocator(
		) const -> const vulkan::allocator&;

		[[nodiscard]] auto descriptor_heap(
			this auto& self
		) -> decltype(auto);

		[[nodiscard]] auto surface_format(
		) const -> vk::Format;

		[[nodiscard]] auto compute_queue_family(
		) const -> std::uint32_t;

		[[nodiscard]] auto compute_queue(
		) -> const vk::raii::Queue&;

		auto add_transient_work(
			const auto& commands
		) -> void;

		auto wait_idle(
		) -> void;

		auto report_device_lost(
			std::string_view operation
		) -> void;

		[[nodiscard]] auto mesh_shaders_enabled(
		) const -> bool;

		[[nodiscard]] auto runtime_ref(
		) -> vulkan::runtime&;

	private:
		vulkan::runtime* m_runtime;
	};
}

gse::gpu::device::device(vulkan::runtime& runtime)
	: m_runtime(&runtime) {}

auto gse::gpu::device::logical_device(this auto&& self) -> decltype(auto) {
	using self_t = std::remove_reference_t<decltype(self)>;
	if constexpr (std::is_const_v<self_t>) {
		return static_cast<const vk::raii::Device&>(self.m_runtime->device_config().device);
	} else {
		return static_cast<vk::raii::Device&>(self.m_runtime->device_config().device);
	}
}

auto gse::gpu::device::physical_device() -> const vk::raii::PhysicalDevice& {
	return m_runtime->device_config().physical_device;
}

auto gse::gpu::device::allocator() -> vulkan::allocator& {
	return m_runtime->allocator();
}

auto gse::gpu::device::allocator() const -> const vulkan::allocator& {
	return m_runtime->allocator();
}

auto gse::gpu::device::descriptor_heap(this auto& self) -> decltype(auto) {
	return self.m_runtime->descriptor_heap();
}

auto gse::gpu::device::surface_format() const -> vk::Format {
	return m_runtime->swap_chain_config().surface_format.format;
}

auto gse::gpu::device::compute_queue_family() const -> std::uint32_t {
	return m_runtime->queue_config().compute_family_index;
}

auto gse::gpu::device::compute_queue() -> const vk::raii::Queue& {
	return m_runtime->queue_config().compute;
}

auto gse::gpu::device::add_transient_work(const auto& commands) -> void {
	m_runtime->add_transient_work(commands);
}

auto gse::gpu::device::wait_idle() -> void {
	m_runtime->device_config().device.waitIdle();
}

auto gse::gpu::device::report_device_lost(std::string_view operation) -> void {
	m_runtime->report_device_lost(operation);
}

auto gse::gpu::device::mesh_shaders_enabled() const -> bool {
	return m_runtime->mesh_shaders_enabled();
}

auto gse::gpu::device::runtime_ref() -> vulkan::runtime& {
	return *m_runtime;
}

export module gse.gpu:vulkan_instance;

import std;
import vulkan;

import :handles;
import :types;

import gse.core;
import gse.os;

export namespace gse::vulkan {
	class device;

	class instance : public non_copyable {
	public:
		~instance() override = default;

		instance(
			instance&&
		) noexcept = default;

		auto operator=(
			instance&&
		) noexcept -> instance& = default;

		[[nodiscard]]
		static auto create(
			std::span<const char* const> required_extensions,
			bool enable_validation
		) -> instance;

		auto create_surface(
			const window::data& win
		) -> void;

	private:
		friend class device;
		friend class swap_chain;
		friend auto pick_surface_format(
			const device& dev,
			const instance& inst
		) -> gpu::image_format;

		instance(
			vk::raii::Context&& context,
			vk::raii::Instance&& instance,
			vk::raii::DebugUtilsMessengerEXT&& debug_messenger
		);

		[[nodiscard]] auto enumerate_physical_devices() const -> std::vector<vk::raii::PhysicalDevice>;

		[[nodiscard]] auto raii_instance() const -> const vk::raii::Instance&;

		[[nodiscard]] auto raii_surface() const -> const vk::raii::SurfaceKHR&;

		vk::raii::Context m_context;
		vk::raii::Instance m_instance;
		vk::raii::SurfaceKHR m_surface;
		vk::raii::DebugUtilsMessengerEXT m_debug_messenger;
	};
}

gse::vulkan::instance::instance(vk::raii::Context&& context, vk::raii::Instance&& instance, vk::raii::DebugUtilsMessengerEXT&& debug_messenger)
	: m_context(std::move(context)), m_instance(std::move(instance)), m_surface(nullptr), m_debug_messenger(std::move(debug_messenger)) {
}

auto gse::vulkan::instance::create_surface(const window::data& win) -> void {
	const auto raw_surface = window::create_vulkan_surface(win, *m_instance);
	m_surface = vk::raii::SurfaceKHR(m_instance, raw_surface);
}

auto gse::vulkan::instance::enumerate_physical_devices() const -> std::vector<vk::raii::PhysicalDevice> {
	return m_instance.enumeratePhysicalDevices();
}

auto gse::vulkan::instance::raii_instance() const -> const vk::raii::Instance& {
	return m_instance;
}

auto gse::vulkan::instance::raii_surface() const -> const vk::raii::SurfaceKHR& {
	return m_surface;
}

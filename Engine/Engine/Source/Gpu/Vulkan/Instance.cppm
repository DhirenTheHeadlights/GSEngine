export module gse.gpu:vulkan_instance;

import std;
import vulkan;

import :handles;

import gse.core;
import gse.save;

export namespace gse {
	class window;
}

export namespace gse::vulkan {
	class instance : public non_copyable {
	public:
		~instance(
		) = default;

		instance(
			instance&&
		) noexcept = default;

		auto operator=(
			instance&&
		) noexcept -> instance& = default;

		[[nodiscard]] static auto create(
			std::span<const char* const> required_extensions,
			save::state& save
		) -> instance;

		auto set_surface(
			vk::raii::SurfaceKHR&& surface
		) -> void;

		[[nodiscard]] auto enumerate_physical_devices(
		) const -> std::vector<vk::raii::PhysicalDevice>;

		[[nodiscard]] auto raii_instance(
		) const -> const vk::raii::Instance&;

		[[nodiscard]] auto raii_surface(
		) const -> const vk::raii::SurfaceKHR&;

	private:
		instance(
			vk::raii::Context&& context,
			vk::raii::Instance&& instance,
			vk::raii::DebugUtilsMessengerEXT&& debug_messenger
		);

		vk::raii::Context m_context;
		vk::raii::Instance m_instance;
		vk::raii::SurfaceKHR m_surface;
		vk::raii::DebugUtilsMessengerEXT m_debug_messenger;
	};

	auto create_surface(
		const window& win,
		instance& instance
	) -> void;
}

gse::vulkan::instance::instance(vk::raii::Context&& context, vk::raii::Instance&& instance, vk::raii::DebugUtilsMessengerEXT&& debug_messenger)
	: m_context(std::move(context)), m_instance(std::move(instance)), m_surface(nullptr), m_debug_messenger(std::move(debug_messenger)) {}

auto gse::vulkan::instance::set_surface(vk::raii::SurfaceKHR&& surface) -> void {
	m_surface = std::move(surface);
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

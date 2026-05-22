export module gse.gpu:swap_chain;

import std;

import :aliases;
import :vulkan_device;
import :vulkan_image;
import :vulkan_instance;
import :vulkan_swapchain;
import :device;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.math;
import gse.log;

export namespace gse::gpu {
	class swap_chain final : public non_copyable {
	public:
		[[nodiscard]]
		static auto create(
			vec2i framebuffer_size,
			present_mode preferred_present_mode,
			device& dev
		) -> std::unique_ptr<swap_chain>;

		swap_chain(
			vulkan::swap_chain&& config,
			vulkan::image&& depth_image,
			device& dev
		);

		[[nodiscard]] auto extent() const -> vec2u;

		[[nodiscard]] auto depth_image(
			this auto& self
		) -> auto&;

		using recreate_callback = std::function<void()>;
		auto on_recreate(
			recreate_callback callback
		) -> void;

		auto notify_recreated() -> void;

		auto recreate(
			vec2i framebuffer_size,
			present_mode preferred_present_mode
		) -> void;

		[[nodiscard]] auto image(
			std::uint32_t index
		) const -> handle<vulkan::image>;

		[[nodiscard]] auto image_view(
			std::uint32_t index
		) const -> handle<vulkan::image_view>;

		[[nodiscard]] auto format() const -> image_format;

		[[nodiscard]] auto is_bgra() const -> bool;

		[[nodiscard]] auto config() -> vulkan::swap_chain&;

		[[nodiscard]] auto config() const -> const vulkan::swap_chain&;

	private:
		vulkan::swap_chain m_config;
		vulkan::image m_depth_image;
		device* m_device;
		std::vector<recreate_callback> m_recreate_callbacks;
	};
}

namespace gse::gpu {
	auto create_swapchain_depth(device& dev, const vec2u extent) -> vulkan::image {
		return vulkan::image::create(
			dev.vulkan_device(),
			image_desc{
				.size = extent,
				.format = image_format::d32_sfloat,
				.usage = image_flag::depth_attachment | image_flag::sampled,
			},
			"swapchain.depth"
		);
	}
}

auto gse::gpu::swap_chain::create(const vec2i framebuffer_size, const present_mode preferred_present_mode, device& dev) -> std::unique_ptr<swap_chain> {
	auto config = vulkan::swap_chain::create(
		framebuffer_size,
		preferred_present_mode,
		dev.vulkan_instance(),
		dev.vulkan_device()
	);
	auto depth = create_swapchain_depth(dev, config.extent());
	return std::make_unique<swap_chain>(std::move(config), std::move(depth), dev);
}

gse::gpu::swap_chain::swap_chain(vulkan::swap_chain&& config, vulkan::image&& depth_image, device& dev)
	: m_config(std::move(config)),
	  m_depth_image(std::move(depth_image)),
	  m_device(&dev) {
}

auto gse::gpu::swap_chain::extent() const -> vec2u {
	return m_config.extent();
}

auto gse::gpu::swap_chain::depth_image(this auto& self) -> auto& {
	return (self.m_depth_image);
}

auto gse::gpu::swap_chain::on_recreate(recreate_callback callback) -> void {
	m_recreate_callbacks.push_back(std::move(callback));
}

auto gse::gpu::swap_chain::notify_recreated() -> void {
	for (const auto& callback : m_recreate_callbacks) {
		callback();
	}
}

auto gse::gpu::swap_chain::recreate(const vec2i framebuffer_size, const present_mode preferred_present_mode) -> void {
	m_depth_image = {};

	if (m_device->vulkan_device().swapchain_maintenance1_enabled()) {
		m_config.wait_release_fences(m_device->vulkan_device());
		const auto old_handle = m_config.handle();
		m_config = vulkan::swap_chain::create(
			framebuffer_size,
			preferred_present_mode,
			m_device->vulkan_instance(),
			m_device->vulkan_device(),
			old_handle
		);
	}
	else {
		m_config.reset_swapchain();
		m_config = vulkan::swap_chain::create(
			framebuffer_size,
			preferred_present_mode,
			m_device->vulkan_instance(),
			m_device->vulkan_device()
		);
	}

	m_depth_image = create_swapchain_depth(*m_device, m_config.extent());
}

auto gse::gpu::swap_chain::image(const std::uint32_t index) const -> handle<vulkan::image> {
	return m_config.image(index);
}

auto gse::gpu::swap_chain::image_view(const std::uint32_t index) const -> handle<vulkan::image_view> {
	return m_config.image_view(index);
}

auto gse::gpu::swap_chain::format() const -> image_format {
	return m_config.format();
}

auto gse::gpu::swap_chain::is_bgra() const -> bool {
	return m_config.format() == image_format::b8g8r8a8_srgb || m_config.format() == image_format::b8g8r8a8_unorm;
}

auto gse::gpu::swap_chain::config() -> vulkan::swap_chain& {
	return m_config;
}

auto gse::gpu::swap_chain::config() const -> const vulkan::swap_chain& {
	return m_config;
}

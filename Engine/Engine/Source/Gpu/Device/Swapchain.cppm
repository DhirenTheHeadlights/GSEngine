export module gse.gpu:swap_chain;

import std;

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
		[[nodiscard]] static auto create(
			vec2i framebuffer_size,
			device& dev
		) -> std::unique_ptr<swap_chain>;

		swap_chain(vulkan::swap_chain&& config, device& dev);

		[[nodiscard]] auto extent(
		) const -> vec2u;

		[[nodiscard]] auto depth_image(
			this auto& self
		) -> auto&;

		auto clear_depth_image(
		) -> void;

		using recreate_callback = std::function<void()>;
		auto on_recreate(
			recreate_callback callback
		) -> void;

		auto notify_recreated(
		) -> void;

		auto set_config(
			vulkan::swap_chain&& config
		) -> void;

		auto recreate(
			vec2i framebuffer_size
		) -> void;

		[[nodiscard]] auto image(
			std::uint32_t index
		) const -> handle<vulkan::image>;

		[[nodiscard]] auto image_view(
			std::uint32_t index
		) const -> handle<vulkan::image_view>;

		[[nodiscard]] auto format(
		) const -> image_format;

		[[nodiscard]] auto is_bgra(
		) const -> bool;

		[[nodiscard]] auto generation(
		) const -> std::uint64_t;

		[[nodiscard]] auto config(
		) -> vulkan::swap_chain&;

		[[nodiscard]] auto config(
		) const -> const vulkan::swap_chain&;

	private:
		vulkan::swap_chain m_config;
		device* m_device;
		std::vector<recreate_callback> m_recreate_callbacks;
		std::uint64_t m_generation = 0;
	};
}

auto gse::gpu::swap_chain::create(const vec2i framebuffer_size, device& dev) -> std::unique_ptr<swap_chain> {
	auto config = vulkan::swap_chain::create(framebuffer_size, dev.vulkan_instance(), dev.vulkan_device());
	return std::make_unique<swap_chain>(std::move(config), dev);
}

gse::gpu::swap_chain::swap_chain(vulkan::swap_chain&& config, device& dev)
	: m_config(std::move(config)), m_device(&dev) {}

auto gse::gpu::swap_chain::extent() const -> vec2u {
	return m_config.extent();
}

auto gse::gpu::swap_chain::depth_image(this auto& self) -> auto& {
	return (self.m_config.depth());
}

auto gse::gpu::swap_chain::clear_depth_image() -> void {
	m_config.clear_depth();
}

auto gse::gpu::swap_chain::on_recreate(recreate_callback callback) -> void {
	m_recreate_callbacks.push_back(std::move(callback));
}

auto gse::gpu::swap_chain::notify_recreated() -> void {
	++m_generation;
	for (const auto& callback : m_recreate_callbacks) {
		callback();
	}
}

auto gse::gpu::swap_chain::set_config(vulkan::swap_chain&& config) -> void {
	m_config = std::move(config);
}

auto gse::gpu::swap_chain::recreate(const vec2i framebuffer_size) -> void {
	m_config.reset_swapchain();
	m_config = vulkan::swap_chain::create(framebuffer_size, m_device->vulkan_instance(), m_device->vulkan_device());
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
	return m_config.format() == image_format::b8g8r8a8_srgb
		|| m_config.format() == image_format::b8g8r8a8_unorm;
}

auto gse::gpu::swap_chain::generation() const -> std::uint64_t {
	return m_generation;
}

auto gse::gpu::swap_chain::config() -> vulkan::swap_chain& {
	return m_config;
}

auto gse::gpu::swap_chain::config() const -> const vulkan::swap_chain& {
	return m_config;
}

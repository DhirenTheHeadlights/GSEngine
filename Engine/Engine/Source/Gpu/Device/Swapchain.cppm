export module gse.gpu:swap_chain;

import std;

import :aliases;
import :device;
import :image;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.vulkan;
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

		[[nodiscard]] auto extent() const -> vec2u;

		[[nodiscard]] auto image_count() const -> std::uint32_t;

		[[nodiscard]] auto format() const -> image_format;

		[[nodiscard]] auto is_bgra() const -> bool;

		[[nodiscard]] auto present_mode() const -> gpu::present_mode;

		auto set_present_mode(
			gpu::present_mode mode
		) -> void;

		[[nodiscard]] auto image(
			std::uint32_t index
		) const -> handle<gpu::image>;

		[[nodiscard]] auto image_view(
			std::uint32_t index
		) const -> handle<gpu::image_view>;

		[[nodiscard]] auto acquire(
			handle<semaphore> wait_semaphore,
			std::uint64_t timeout_ns = std::numeric_limits<std::uint64_t>::max()
		) const -> vulkan::acquire_next_image_result;

		[[nodiscard]] auto present(
			handle<semaphore> wait_semaphore,
			std::uint32_t image_index,
			std::uint64_t present_id
		) -> result;

		[[nodiscard]] auto release_fence(
			std::uint32_t image_index
		) const -> handle<fence>;

		auto wait_release_fences() -> void;

		auto reset_release_fence(
			std::uint32_t image_index
		) -> void;

		[[nodiscard]] auto wait_for_present(
			std::uint64_t present_id,
			std::uint64_t timeout_ns = std::numeric_limits<std::uint64_t>::max()
		) const -> result;

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
			gpu::present_mode preferred_present_mode
		) -> void;

	private:
		swap_chain(
			vulkan::swap_chain&& config,
			gpu::image&& depth_image,
			device& dev
		);

		vulkan::swap_chain m_config;
		gpu::image m_depth_image;
		device* m_device;
		std::vector<recreate_callback> m_recreate_callbacks;
	};
}

namespace gse::gpu {
	auto create_swapchain_depth(
		device& dev,
		vec2u extent
	) -> gpu::image;
}

auto gse::gpu::swap_chain::create(const vec2i framebuffer_size, const gpu::present_mode preferred_present_mode, device& dev) -> std::unique_ptr<swap_chain> {
	auto config = vulkan::swap_chain::create(
		framebuffer_size,
		preferred_present_mode,
		dev.vulkan_instance(),
		dev.vulkan_device()
	);
	auto depth = create_swapchain_depth(dev, config.extent());
	return std::unique_ptr<swap_chain>(new swap_chain(std::move(config), std::move(depth), dev));
}

gse::gpu::swap_chain::swap_chain(vulkan::swap_chain&& config, gpu::image&& depth_image, device& dev)
	: m_config(std::move(config)), m_depth_image(std::move(depth_image)), m_device(&dev) {
}

auto gse::gpu::swap_chain::extent() const -> vec2u {
	return m_config.extent();
}

auto gse::gpu::swap_chain::image_count() const -> std::uint32_t {
	return m_config.image_count();
}

auto gse::gpu::swap_chain::format() const -> image_format {
	return m_config.format();
}

auto gse::gpu::swap_chain::is_bgra() const -> bool {
	return m_config.format() == image_format::b8g8r8a8_srgb || m_config.format() == image_format::b8g8r8a8_unorm;
}

auto gse::gpu::swap_chain::present_mode() const -> gpu::present_mode {
	return m_config.present_mode();
}

auto gse::gpu::swap_chain::set_present_mode(const gpu::present_mode mode) -> void {
	m_config.set_present_mode(mode);
}

auto gse::gpu::swap_chain::image(const std::uint32_t index) const -> handle<gpu::image> {
	return m_config.image(index);
}

auto gse::gpu::swap_chain::image_view(const std::uint32_t index) const -> handle<gpu::image_view> {
	return m_config.image_view(index);
}

auto gse::gpu::swap_chain::acquire(const handle<semaphore> wait_semaphore, const std::uint64_t timeout_ns) const -> vulkan::acquire_next_image_result {
	return vulkan::acquire_next_image(m_device->vulkan_device(), m_config.handle(), wait_semaphore, timeout_ns);
}

auto gse::gpu::swap_chain::present(const handle<semaphore> wait_semaphore, const std::uint32_t image_index, const std::uint64_t present_id) -> result {
	reset_release_fence(image_index);

	const auto swapchain_handle = m_config.handle();
	const auto current_present_mode = m_config.present_mode();
	const auto release_fence_handle = m_config.release_fence(image_index);

	const present_info info{
		.wait_semaphores = std::span(&wait_semaphore, 1),
		.swapchains = std::span(&swapchain_handle, 1),
		.image_indices = std::span(&image_index, 1),
		.present_modes = std::span(&current_present_mode, 1),
		.release_fences = std::span(&release_fence_handle, 1),
		.present_ids = std::span(&present_id, 1),
	};

	return m_device->present(info);
}

auto gse::gpu::swap_chain::release_fence(const std::uint32_t image_index) const -> handle<fence> {
	return m_config.release_fence(image_index);
}

auto gse::gpu::swap_chain::wait_release_fences() -> void {
	m_config.wait_release_fences(m_device->vulkan_device());
}

auto gse::gpu::swap_chain::reset_release_fence(const std::uint32_t image_index) -> void {
	m_config.reset_release_fence(m_device->vulkan_device(), image_index);
}

auto gse::gpu::swap_chain::wait_for_present(const std::uint64_t present_id, const std::uint64_t timeout_ns) const -> result {
	return m_config.wait_for_present(present_id, timeout_ns);
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

auto gse::gpu::swap_chain::recreate(const vec2i framebuffer_size, const gpu::present_mode preferred_present_mode) -> void {
	m_depth_image = {};

	m_config.wait_release_fences(m_device->vulkan_device());
	const auto old_handle = m_config.handle();
	m_config = vulkan::swap_chain::create(
		framebuffer_size,
		preferred_present_mode,
		m_device->vulkan_instance(),
		m_device->vulkan_device(),
		old_handle
	);

	m_depth_image = create_swapchain_depth(*m_device, m_config.extent());
}

auto gse::gpu::create_swapchain_depth(device& dev, const vec2u extent) -> image {
	auto img = image::create(
		dev.vulkan_device(),
		image_desc{
			.size = extent,
			.format = image_format::d32_sfloat,
			.usage = image_flag::depth_attachment | image_flag::sampled,
		},
		"swapchain.depth"
	);
	gpu::transition_image_to(dev, img);
	return img;
}

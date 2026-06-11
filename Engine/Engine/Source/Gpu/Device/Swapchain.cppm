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

		[[nodiscard]]
		auto acquire(
			handle<gpu::semaphore> wait_semaphore,
			std::uint64_t timeout_ns = std::numeric_limits<std::uint64_t>::max()
		) const -> gpu::acquire_next_image_result;

		[[nodiscard]]
		auto present(
			handle<gpu::semaphore> wait_semaphore,
			std::uint32_t image_index,
			std::uint64_t present_id,
			time_t<std::uint64_t> relative_target
		) -> result;

		[[nodiscard]] auto release_fence(
			std::uint32_t image_index
		) const -> handle<gpu::fence>;

		auto wait_release_fences() -> void;

		auto reset_release_fence(
			std::uint32_t image_index
		) -> void;

		[[nodiscard]]
		auto past_presentation_timing() const -> std::vector<past_present_timing>;

		[[nodiscard]] auto refresh_interval() const -> time_t<std::uint64_t>;

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
			swap_chain_info&& info,
			gpu::image&& depth_image,
			device& dev
		);

		swap_chain_info m_info;
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
	auto info = dev.create_swapchain(framebuffer_size, preferred_present_mode);
	auto depth = create_swapchain_depth(dev, info.extent);
	return std::unique_ptr<swap_chain>(new swap_chain(std::move(info), std::move(depth), dev));
}

gse::gpu::swap_chain::swap_chain(swap_chain_info&& info, gpu::image&& depth_image, device& dev)
	: m_info(std::move(info)), m_depth_image(std::move(depth_image)), m_device(&dev) {
}

auto gse::gpu::swap_chain::extent() const -> vec2u {
	return m_info.extent;
}

auto gse::gpu::swap_chain::image_count() const -> std::uint32_t {
	return static_cast<std::uint32_t>(m_info.images.size());
}

auto gse::gpu::swap_chain::format() const -> image_format {
	return m_info.format;
}

auto gse::gpu::swap_chain::is_bgra() const -> bool {
	return m_info.format == image_format::b8g8r8a8_srgb || m_info.format == image_format::b8g8r8a8_unorm;
}

auto gse::gpu::swap_chain::present_mode() const -> gpu::present_mode {
	return m_info.present_mode;
}

auto gse::gpu::swap_chain::set_present_mode(const gpu::present_mode mode) -> void {
	m_info.present_mode = mode;
}

auto gse::gpu::swap_chain::image(const std::uint32_t index) const -> handle<gpu::image> {
	return m_info.images[index];
}

auto gse::gpu::swap_chain::image_view(const std::uint32_t index) const -> handle<gpu::image_view> {
	return m_info.image_views[index];
}

auto gse::gpu::swap_chain::acquire(const handle<gpu::semaphore> wait_semaphore, const std::uint64_t timeout_ns) const -> gpu::acquire_next_image_result {
	return m_device->acquire_swapchain_image(m_info.handle, wait_semaphore, timeout_ns);
}

auto gse::gpu::swap_chain::present(const handle<gpu::semaphore> wait_semaphore, const std::uint32_t image_index, const std::uint64_t present_id, const time_t<std::uint64_t> relative_target) -> result {
	reset_release_fence(image_index);

	const auto swapchain_handle = m_info.handle;
	const auto current_present_mode = m_info.present_mode;
	const auto release_fence_handle = m_device->swapchain_release_fence(m_info.handle, image_index);

	present_info info{
		.wait_semaphores = std::span(&wait_semaphore, 1),
		.swapchains = std::span(&swapchain_handle, 1),
		.image_indices = std::span(&image_index, 1),
		.present_modes = std::span(&current_present_mode, 1),
		.release_fences = std::span(&release_fence_handle, 1),
		.present_ids = std::span(&present_id, 1),
		.time_domain_id = m_info.time_domain_id,
	};

	if (m_info.time_domain_id != 0) {
		info.target_present_times = std::span(&relative_target, 1);
		info.present_stage_queries = present_stage_flags(present_stage_flag::image_first_pixel_out);
	}

	return m_device->present(info);
}

auto gse::gpu::swap_chain::release_fence(const std::uint32_t image_index) const -> handle<gpu::fence> {
	return m_device->swapchain_release_fence(m_info.handle, image_index);
}

auto gse::gpu::swap_chain::wait_release_fences() -> void {
	m_device->wait_swapchain_release_fences(m_info.handle);
}

auto gse::gpu::swap_chain::reset_release_fence(const std::uint32_t image_index) -> void {
	m_device->reset_swapchain_release_fence(m_info.handle, image_index);
}

auto gse::gpu::swap_chain::past_presentation_timing() const -> std::vector<past_present_timing> {
	return m_device->swapchain_past_presentation_timing(m_info.handle);
}

auto gse::gpu::swap_chain::refresh_interval() const -> time_t<std::uint64_t> {
	return m_info.refresh_interval;
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

	m_device->wait_swapchain_release_fences(m_info.handle);
	const auto old_handle = m_info.handle;
	m_info = m_device->create_swapchain(framebuffer_size, preferred_present_mode, old_handle);

	m_depth_image = create_swapchain_depth(*m_device, m_info.extent);
}

auto gse::gpu::create_swapchain_depth(device& dev, const vec2u extent) -> image {
	auto img = dev.create_image(
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

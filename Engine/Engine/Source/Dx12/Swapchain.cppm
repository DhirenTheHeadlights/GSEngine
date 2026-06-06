export module gse.dx12:swapchain;

import std;

import gse.gpu_types;
import gse.core;
import gse.math;

import :resources;
import :sync;

export namespace gse::dx12 {
	class device;

	class swap_chain final : public non_copyable {
	public:
		swap_chain() {}
		~swap_chain() = default;

		swap_chain(
			swap_chain&&
		) noexcept = default;

		auto operator=(
			swap_chain&&
		) noexcept -> swap_chain& = default;

		[[nodiscard]] auto handle() const -> gpu::swap_chain_handle;

		[[nodiscard]] auto extent() const -> vec2u;

		[[nodiscard]] auto image_count() const -> std::uint32_t;

		[[nodiscard]] auto format() const -> gpu::image_format;

		[[nodiscard]] auto present_mode() const -> gpu::present_mode;

		auto set_present_mode(
			gpu::present_mode mode
		) -> void;

		[[nodiscard]] auto image(
			std::uint32_t index
		) const -> gpu::image_handle;

		[[nodiscard]] auto image_view(
			std::uint32_t index
		) const -> gpu::image_view_handle;

		[[nodiscard]] auto release_fence(
			std::uint32_t image_index
		) const -> gpu::fence_handle;

		[[nodiscard]] auto wait_for_present(
			std::uint64_t present_id,
			std::uint64_t timeout_ns = std::numeric_limits<std::uint64_t>::max()
		) const -> gpu::result;

		auto wait_release_fences(
			const device& dev
		) const -> void;

		auto reset_release_fence(
			const device& dev,
			std::uint32_t image_index
		) -> void;

	private:
		gpu::swap_chain_handle m_handle;
		vec2u m_extent;
		gpu::present_mode m_present_mode = gpu::present_mode::fifo;
	};
}

auto gse::dx12::swap_chain::handle() const -> gpu::swap_chain_handle {
	return m_handle;
}

auto gse::dx12::swap_chain::extent() const -> vec2u {
	return m_extent;
}

auto gse::dx12::swap_chain::image_count() const -> std::uint32_t {
	return 0;
}

auto gse::dx12::swap_chain::format() const -> gpu::image_format {
	return gpu::image_format::b8g8r8a8_unorm;
}

auto gse::dx12::swap_chain::present_mode() const -> gpu::present_mode {
	return m_present_mode;
}

auto gse::dx12::swap_chain::set_present_mode(const gpu::present_mode mode) -> void {
	m_present_mode = mode;
}

auto gse::dx12::swap_chain::image(const std::uint32_t) const -> gpu::image_handle {
	return {};
}

auto gse::dx12::swap_chain::image_view(const std::uint32_t) const -> gpu::image_view_handle {
	return {};
}

auto gse::dx12::swap_chain::release_fence(const std::uint32_t) const -> gpu::fence_handle {
	return {};
}

auto gse::dx12::swap_chain::wait_for_present(const std::uint64_t, const std::uint64_t) const -> gpu::result {
	return gpu::result::success;
}

auto gse::dx12::swap_chain::wait_release_fences(const device&) const -> void {
}

auto gse::dx12::swap_chain::reset_release_fence(const device&, const std::uint32_t) -> void {
}

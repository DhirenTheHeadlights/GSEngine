export module gse.dx12:swapchain;

import std;
import gse.gpu_backend;
import gse.directx;
import gse.math;
import gse.log;

import :conversions;
import :device;

export namespace gse::dx12 {
	class swapchain {
	public:
		auto bind(
			device* owner
		) -> void;

		[[nodiscard]] auto create(
			vec2i framebuffer_size,
			gpu::present_mode mode,
			gpu::swap_chain_handle old_handle
		) -> gpu::swap_chain_info;

		[[nodiscard]] auto acquire_image(
			gpu::handle<gpu::semaphore> wait_semaphore
		) const -> gpu::acquire_next_image_result;

		auto wait_release_fences() const -> void;

		auto reset_release_fence(
			std::uint32_t image_index
		) const -> void;

		[[nodiscard]] auto release_fence(
			std::uint32_t image_index
		) const -> gpu::handle<gpu::fence>;

		[[nodiscard]] auto past_presentation_timing() const -> std::vector<gpu::past_present_timing>;

		[[nodiscard]] auto present(
			const gpu::present_info& info
		) -> gpu::result;

	private:
		device* m_owner = nullptr;
		directx::com_ptr<directx::IDXGISwapChain3> m_swapchain;
		directx::com_ptr<directx::ID3D12DescriptorHeap> m_rtv_heap;
		std::vector<directx::com_ptr<directx::ID3D12Resource>> m_backbuffers;
		std::uint32_t m_image_count = 0;
		std::uint32_t m_rtv_size = 0;
		vec2u m_extent;
		gpu::image_format m_surface_fmt = gpu::image_format::b8g8r8a8_unorm;
	};
}

auto gse::dx12::swapchain::bind(device* owner) -> void {
	m_owner = owner;
}

auto gse::dx12::swapchain::create(const vec2i framebuffer_size, gpu::present_mode, gpu::swap_chain_handle) -> gpu::swap_chain_info {
	m_owner->wait_idle();

	m_backbuffers.clear();
	m_rtv_heap.reset();
	m_swapchain.reset();

	m_image_count = 3;
	m_extent = vec2u{ static_cast<std::uint32_t>(framebuffer_size.x()), static_cast<std::uint32_t>(framebuffer_size.y()) };

	log::println(log::category::dx12, "create_swapchain begin {}x{} hwnd={} queue={} factory={}", m_extent.x(), m_extent.y(), m_owner->hwnd(), static_cast<void*>(m_owner->graphics_queue()), static_cast<void*>(m_owner->factory()));

	m_swapchain = directx::create_swapchain(m_owner->factory(), m_owner->graphics_queue(), m_owner->hwnd(), m_extent.x(), m_extent.y(), m_image_count, dxgi_format_of(m_surface_fmt));
	log::println(log::category::dx12, "swapchain={}", static_cast<void*>(m_swapchain.get()));

	gpu::swap_chain_info out = {
		.handle = std::bit_cast<gpu::swap_chain_handle>(m_swapchain.get()),
		.extent = m_extent,
		.format = m_surface_fmt,
	};

	if (!m_swapchain) {
		log::println(log::level::error, log::category::dx12, "create_swapchain FAILED (null swapchain)");
		return out;
	}

	m_rtv_heap = directx::create_rtv_heap(m_owner->raw_device(), m_image_count);
	m_rtv_size = directx::rtv_descriptor_size(m_owner->raw_device());
	log::println(log::category::dx12, "rtv_heap={} rtv_size={}", static_cast<void*>(m_rtv_heap.get()), m_rtv_size);

	m_backbuffers.resize(m_image_count);
	for (std::uint32_t i = 0; i < m_image_count; ++i) {
		m_backbuffers[i] = directx::swapchain_buffer(m_swapchain.get(), i);
		log::println(log::category::dx12, "backbuffer[{}]={}", i, static_cast<void*>(m_backbuffers[i].get()));
		if (!m_backbuffers[i] || !m_rtv_heap) {
			continue;
		}
		auto rtv = directx::descriptor_heap_cpu_start(m_rtv_heap.get());
		rtv.ptr += static_cast<std::size_t>(i) * m_rtv_size;
		directx::create_render_target_view(m_owner->raw_device(), m_backbuffers[i].get(), rtv);
		m_owner->register_view_format(rtv.ptr, dxgi_format_of(m_surface_fmt));
		out.images.push_back(std::bit_cast<gpu::handle<gpu::image>>(m_backbuffers[i].get()));
		out.image_views.push_back(std::bit_cast<gpu::handle<gpu::image_view>>(rtv.ptr));
	}

	log::println(log::category::dx12, "create_swapchain end images={}", out.images.size());

	return out;
}

auto gse::dx12::swapchain::acquire_image(const gpu::handle<gpu::semaphore> wait_semaphore) const -> gpu::acquire_next_image_result {
	const auto index = m_swapchain ? m_swapchain->GetCurrentBackBufferIndex() : 0u;
	if (auto* sp = std::bit_cast<sync_point*>(wait_semaphore); sp && sp->fence) {
		const auto value = ++sp->value;
		m_owner->record_queue_op(queue_op_kind::cpu_signal, gpu::queue_type::graphics, sp->fence.get(), value, 0);
		sp->fence->Signal(value);
	}
	return {
		.result = gpu::result::success,
		.image_index = index,
	};
}

auto gse::dx12::swapchain::wait_release_fences() const -> void {}

auto gse::dx12::swapchain::reset_release_fence(std::uint32_t) const -> void {}

auto gse::dx12::swapchain::release_fence(std::uint32_t) const -> gpu::handle<gpu::fence> {
	return {};
}

auto gse::dx12::swapchain::past_presentation_timing() const -> std::vector<gpu::past_present_timing> {
	return {};
}

auto gse::dx12::swapchain::present(const gpu::present_info& info) -> gpu::result {
	for (const auto& w : info.wait_semaphores) {
		if (auto* sp = std::bit_cast<sync_point*>(w); sp && sp->fence) {
			m_owner->record_queue_op(queue_op_kind::cpu_wait, gpu::queue_type::graphics, sp->fence.get(), sp->value, 0);
			directx::wait_fence(sp->fence.get(), sp->value, m_owner->idle_event());
		}
	}
	if (m_swapchain) {
		m_swapchain->Present(1, 0);
	}
	return gpu::result::success;
}

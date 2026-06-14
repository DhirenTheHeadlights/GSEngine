export module gse.dx12:device;

import std;

import gse.gpu_backend;
import gse.core;
import gse.os;
import gse.ecs;
import gse.math;
import gse.win32;
import gse.directx;
import gse.log;

namespace gse::dx12 {
	struct sync_point {
		directx::com_ptr<directx::ID3D12Fence> fence;
		std::uint64_t value = 0;
	};

	struct frame_target {
		directx::com_ptr<directx::ID3D12CommandAllocator> allocator;
		directx::com_ptr<directx::ID3D12GraphicsCommandList> list;
	};

	struct transient_entry {
		directx::com_ptr<directx::ID3D12CommandAllocator> allocator;
		directx::com_ptr<directx::ID3D12GraphicsCommandList> list;
	};

	struct transient_pool {
		std::vector<transient_entry> entries;
		std::size_t used = 0;
		std::uint64_t high_water = 0;
	};

	[[nodiscard]] auto dxgi_format_of(
		gpu::image_format fmt
	) -> directx::DXGI_FORMAT;

	[[nodiscard]] auto state_from_access(
		gpu::access_flags access
	) -> directx::D3D12_RESOURCE_STATES;
}

export namespace gse::dx12 {
	class device final : public non_copyable {
	public:
		device(
			shared_view<window> win,
			gpu::device_settings& cfg
		);

		[[nodiscard]] auto handle() const -> gpu::device_handle;

		[[nodiscard]] auto queue_family(
			gpu::queue_type queue_type
		) const -> std::uint32_t;

		auto wait_idle() const -> void;

		[[nodiscard]] auto timestamp_period() const -> float;

		auto wait_for_crash_dump() -> void;

		[[nodiscard]] auto fault_enabled() const -> bool;

		[[nodiscard]] auto vendor_binary_fault_enabled() const -> bool;

		[[nodiscard]] auto query_fault_counts(
			gpu::device_fault_counts& counts
		) const -> gpu::result;

		[[nodiscard]] auto query_fault_info(
			gpu::device_fault_counts& counts,
			gpu::device_fault_info& info
		) const -> gpu::result;

		auto record_buffer_fill_u32(
			gpu::command_buffer_handle cmd,
			gpu::handle<gpu::buffer> buf,
			gpu::device_size offset,
			std::uint32_t value
		) -> void;

		auto cmd_reset(
			gpu::command_buffer_handle cmd
		) -> void;

		auto cmd_begin(
			gpu::command_buffer_handle cmd
		) -> void;

		auto cmd_end(
			gpu::command_buffer_handle cmd
		) -> void;

		auto cmd_pipeline_barrier(
			gpu::command_buffer_handle cmd,
			const gpu::dependency_info& dep
		) -> void;

		auto cmd_release_swapchain_to_present(
			gpu::command_buffer_handle cmd,
			gpu::handle<gpu::image> img,
			gpu::pipeline_stage_flags src_stages,
			gpu::access_flags src_access
		) -> void;

		[[nodiscard]] auto frame_command_buffer(
			gpu::queue_type queue_type,
			std::uint32_t frame_index
		) const -> gpu::command_buffer_handle;

		auto submit(
			gpu::queue_type queue_type,
			const gpu::submit_info& info,
			gpu::handle<gpu::fence> signal_fence
		) -> void;

		[[nodiscard]] auto present(
			const gpu::present_info& info
		) -> gpu::result;

		[[nodiscard]] auto wait_for_fence(
			gpu::handle<gpu::fence> f,
			std::uint64_t timeout_ns
		) const -> gpu::result;

		auto reset_fence(
			gpu::handle<gpu::fence> f
		) const -> void;

		auto reset_worker_command_pools(
			std::uint32_t frame_index
		) -> void;

		[[nodiscard]] auto acquire_worker_command_buffer(
			gpu::queue_type queue_type,
			std::size_t worker_index,
			std::uint32_t frame_index
		) -> gpu::command_buffer_handle;

		[[nodiscard]] auto create_image_unbound(
			const gpu::image_create_info& info
		) const -> std::pair<gpu::handle<gpu::image>, gpu::memory_requirements>;

		[[nodiscard]] auto create_buffer_unbound(
			const gpu::buffer_desc& info
		) const -> std::pair<gpu::handle<gpu::buffer>, gpu::memory_requirements>;

		auto bind_image_memory(
			gpu::handle<gpu::image> img,
			gpu::device_memory mem,
			gpu::device_size offset
		) const -> void;

		auto bind_buffer_memory(
			gpu::handle<gpu::buffer> buf,
			gpu::device_memory mem,
			gpu::device_size offset
		) const -> void;

		[[nodiscard]] auto create_image_view(
			gpu::handle<gpu::image> img,
			const gpu::image_view_create_info& info
		) const -> gpu::handle<gpu::image_view>;

		[[nodiscard]] auto allocate_aliased_memory(
			gpu::device_size size,
			std::uint32_t memory_type_index
		) const -> gpu::device_memory;

		auto free_aliased_memory(
			gpu::device_memory mem
		) const -> void;

		[[nodiscard]] auto find_memory_type_index(
			std::uint32_t type_bits,
			gpu::memory_property_flags required
		) const -> std::uint32_t;

		auto host_upload_image_layers(
			gpu::handle<gpu::image> img,
			std::span<const void* const> layer_pointers,
			vec2u extent
		) const -> void;

		auto begin_one_time_commands(
			gpu::command_buffer_handle cmd
		) -> void;

		auto end_commands(
			gpu::command_buffer_handle cmd
		) -> void;

		[[nodiscard]] auto create_transient_command_pool(
			std::uint32_t family
		) -> gpu::transient_pool_handle;

		[[nodiscard]] auto allocate_transient_primary(
			gpu::transient_pool_handle pool
		) -> gpu::command_buffer_handle;

		auto transient_pool_try_reset(
			gpu::transient_pool_handle pool,
			std::uint64_t queue_progress
		) -> void;

		auto transient_pool_mark_in_use(
			gpu::transient_pool_handle pool,
			std::uint64_t value
		) -> void;

		auto transient_pool_reset_all(
			gpu::transient_pool_handle pool
		) -> void;

		[[nodiscard]] auto create_shader_program(
			const gpu::shader_program_create_info& info
		) -> gpu::shader_program;

		[[nodiscard]] auto create_semaphore() -> gpu::handle<gpu::semaphore>;

		[[nodiscard]] auto create_timeline_semaphore(
			std::uint64_t initial_value
		) -> gpu::handle<gpu::semaphore>;

		[[nodiscard]] auto create_fence(
			bool signaled
		) -> gpu::handle<gpu::fence>;

		auto retire_semaphore(
			gpu::handle<gpu::semaphore> semaphore
		) -> void;

		auto retire_fence(
			gpu::handle<gpu::fence> fence
		) -> void;

		[[nodiscard]] auto semaphore_counter_value(
			gpu::handle<gpu::semaphore> semaphore
		) const -> std::uint64_t;

		auto wait_semaphore(
			gpu::handle<gpu::semaphore> semaphore,
			std::uint64_t value
		) const -> void;

		[[nodiscard]] auto create_timestamp_query_pool(
			std::uint32_t capacity,
			std::string_view label
		) -> gpu::handle<gpu::query_pool>;

		[[nodiscard]] auto create_pipeline_stats_query_pool(
			std::uint32_t capacity,
			gpu::pipeline_statistic_flags statistics,
			std::string_view label
		) -> gpu::handle<gpu::query_pool>;

		[[nodiscard]] auto query_pool_results(
			gpu::handle<gpu::query_pool> pool,
			std::uint32_t first_query,
			std::uint32_t query_count,
			std::uint64_t stride
		) const -> std::pair<gpu::query_status, std::vector<std::uint64_t>>;

		[[nodiscard]] auto create_swapchain(
			vec2i framebuffer_size,
			gpu::present_mode mode,
			gpu::swap_chain_handle old_handle
		) -> gpu::swap_chain_info;

		[[nodiscard]] auto acquire_swapchain_image(
			gpu::swap_chain_handle swapchain,
			gpu::handle<gpu::semaphore> wait_semaphore,
			std::uint64_t timeout_ns
		) const -> gpu::acquire_next_image_result;

		auto wait_swapchain_release_fences(
			gpu::swap_chain_handle swapchain
		) const -> void;

		auto reset_swapchain_release_fence(
			gpu::swap_chain_handle swapchain,
			std::uint32_t image_index
		) const -> void;

		[[nodiscard]] auto swapchain_release_fence(
			gpu::swap_chain_handle swapchain,
			std::uint32_t image_index
		) const -> gpu::handle<gpu::fence>;

		[[nodiscard]] auto swapchain_past_presentation_timing(
			gpu::swap_chain_handle swapchain
		) const -> std::vector<gpu::past_present_timing>;

		[[nodiscard]] auto create_blas(
			const gpu::acceleration_structure_geometry& geometry,
			std::uint32_t prim_count
		) -> gpu::blas;

		[[nodiscard]] auto create_tlas(
			std::uint32_t max_instances
		) -> gpu::tlas;

		[[nodiscard]] auto query_blas_build_sizes(
			const gpu::acceleration_structure_geometry& geometry,
			std::uint32_t prim_count
		) const -> gpu::acceleration_structure_build_sizes;

		[[nodiscard]] auto acceleration_structure_scratch_alignment() const -> gpu::device_size;

		[[nodiscard]] auto create_buffer(
			const gpu::buffer_desc& desc,
			std::string_view tag,
			const std::source_location& loc
		) -> gpu::buffer;

		[[nodiscard]] auto create_image(
			const gpu::image_desc& desc,
			std::string_view tag
		) -> gpu::image;

		[[nodiscard]] auto allocate_buffer_slot() -> gpu::bindless_handle;

		[[nodiscard]] auto allocate_image_slot() -> gpu::bindless_handle;

		auto write_storage_buffer(
			gpu::bindless_slot slot,
			gpu::device_address address,
			gpu::device_size size
		) -> void;

		auto write_uniform_buffer(
			gpu::bindless_slot slot,
			gpu::device_address address,
			gpu::device_size size
		) -> void;

		auto write_acceleration_structure(
			gpu::bindless_slot slot,
			gpu::device_address as_address
		) -> void;

		auto write_sampled_image(
			gpu::bindless_slot slot,
			const gpu::image& img
		) -> void;

		[[nodiscard]] auto register_sampler(
			const gpu::sampler_desc& desc
		) -> gpu::bindless_handle;

		[[nodiscard]] auto register_texture(
			const gpu::image& img,
			const gpu::sampler_desc& desc
		) -> gpu::bindless_handle;

		[[nodiscard]] auto bindless_layout() const -> gpu::bindless_layout;

		[[nodiscard]] auto bindless_resource_heap_binding() const -> gpu::bindless_heap_binding;

		[[nodiscard]] auto bindless_sampler_heap_binding() const -> gpu::bindless_heap_binding;

		[[nodiscard]] auto create_sampler(
			const gpu::sampler_desc& desc
		) -> gpu::handle<gpu::sampler>;

		auto collect_garbage() -> void;

	private:
		directx::com_ptr<directx::IDXGIFactory4> m_factory;
		directx::com_ptr<directx::ID3D12Device> m_device;
		directx::com_ptr<directx::ID3D12CommandQueue> m_graphics_queue;
		directx::com_ptr<directx::ID3D12Fence> m_idle_fence;
		directx::com_ptr<directx::IDXGISwapChain3> m_swapchain;
		directx::com_ptr<directx::ID3D12DescriptorHeap> m_rtv_heap;
		std::vector<directx::com_ptr<directx::ID3D12Resource>> m_backbuffers;
		std::vector<frame_target> m_frames;
		std::deque<sync_point> m_sync_points;
		std::vector<transient_pool> m_transient_pools;
		mutable std::mutex m_mutex;
		std::vector<directx::com_ptr<directx::ID3D12Resource>> m_owned_buffers;
		std::vector<directx::com_ptr<directx::ID3D12Resource>> m_owned_images;
		gpu::bindless_slot_pool m_image_pool;
		gpu::bindless_slot_pool m_buffer_pool;
		gpu::bindless_slot_pool m_texture_pool;
		gpu::bindless_slot_pool m_sampler_pool;
		void* m_hwnd = nullptr;
		void* m_idle_event = nullptr;
		std::uint32_t m_rtv_size = 0;
		std::uint32_t m_image_count = 0;
		gpu::image_format m_surface_fmt = gpu::image_format::b8g8r8a8_unorm;
		vec2u m_extent;
	};
}

auto gse::dx12::dxgi_format_of(const gpu::image_format fmt) -> directx::DXGI_FORMAT {
	switch (fmt) {
		case gpu::image_format::r8g8b8a8_unorm: return directx::format_r8g8b8a8_unorm;
		case gpu::image_format::r8g8b8a8_srgb: return directx::format_r8g8b8a8_srgb;
		case gpu::image_format::b8g8r8a8_srgb: return directx::format_b8g8r8a8_srgb;
		case gpu::image_format::d32_sfloat: return directx::format_d32_float;
		default: return directx::format_b8g8r8a8_unorm;
	}
}

auto gse::dx12::state_from_access(const gpu::access_flags access) -> directx::D3D12_RESOURCE_STATES {
	if (access.test(gpu::access_flag::depth_stencil_attachment_write)) {
		return directx::resource_state_depth_write;
	}
	if (access.test(gpu::access_flag::depth_stencil_attachment_read)) {
		return directx::resource_state_depth_read;
	}
	if (access.test(gpu::access_flag::color_attachment_write)) {
		return directx::resource_state_render_target;
	}
	if (access.test(gpu::access_flag::shader_storage_write) || access.test(gpu::access_flag::shader_write)) {
		return directx::resource_state_unordered_access;
	}
	if (access.test(gpu::access_flag::transfer_write)) {
		return directx::resource_state_copy_dest;
	}
	if (access.test(gpu::access_flag::transfer_read)) {
		return directx::resource_state_copy_source;
	}
	if (access.test(gpu::access_flag::shader_read) || access.test(gpu::access_flag::shader_sampled_read) || access.test(gpu::access_flag::shader_storage_read)) {
		return directx::resource_state_shader_resource;
	}
	return directx::resource_state_common;
}

gse::dx12::device::device(const shared_view<window> win, gpu::device_settings&) {
	log::println(log::category::render, "dx12: ctor begin");
	log::flush();

	directx::enable_debug_layer();
	m_factory = directx::create_factory();
	m_device = directx::create_device();
	log::println(log::category::render, "dx12: factory={} device={}", static_cast<void*>(m_factory.get()), static_cast<void*>(m_device.get()));
	log::flush();

	m_graphics_queue = directx::create_direct_queue(m_device.get());
	m_hwnd = win32::glfwGetWin32Window(window::raw_handle(win));
	log::println(log::category::render, "dx12: queue={} hwnd={}", static_cast<void*>(m_graphics_queue.get()), m_hwnd);
	log::flush();

	m_idle_fence = directx::create_fence(m_device.get(), 0);
	m_idle_event = directx::create_wait_event();

	m_frames.resize(3);
	for (auto& f : m_frames) {
		f.allocator = directx::create_command_allocator(m_device.get());
		f.list = directx::create_command_list(m_device.get(), f.allocator.get());
	}

	m_image_pool.reset(16384);
	m_buffer_pool.reset(16384);
	m_texture_pool.reset(1024);
	m_sampler_pool.reset(512);

	log::println(log::category::render, "dx12: ctor end");
	log::flush();
}

auto gse::dx12::device::handle() const -> gpu::device_handle {
	return std::bit_cast<gpu::device_handle>(m_device.get());
}

auto gse::dx12::device::queue_family(gpu::queue_type) const -> std::uint32_t {
	return 0;
}

auto gse::dx12::device::wait_idle() const -> void {
	const auto target = m_idle_fence->GetCompletedValue() + 1;
	m_graphics_queue->Signal(m_idle_fence.get(), target);
	directx::wait_fence(m_idle_fence.get(), target, m_idle_event);
}

auto gse::dx12::device::timestamp_period() const -> float {
	return 1.0f;
}

auto gse::dx12::device::wait_for_crash_dump() -> void {}

auto gse::dx12::device::fault_enabled() const -> bool {
	return false;
}

auto gse::dx12::device::vendor_binary_fault_enabled() const -> bool {
	return false;
}

auto gse::dx12::device::query_fault_counts(gpu::device_fault_counts&) const -> gpu::result {
	return gpu::result::success;
}

auto gse::dx12::device::query_fault_info(gpu::device_fault_counts&, gpu::device_fault_info&) const -> gpu::result {
	return gpu::result::success;
}

auto gse::dx12::device::record_buffer_fill_u32(gpu::command_buffer_handle, gpu::handle<gpu::buffer>, gpu::device_size, std::uint32_t) -> void {}

auto gse::dx12::device::cmd_reset(const gpu::command_buffer_handle cmd) -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd);
	for (auto& f : m_frames) {
		if (f.list.get() == list) {
			f.allocator->Reset();
			list->Reset(f.allocator.get(), nullptr);
			return;
		}
	}
}

auto gse::dx12::device::cmd_begin(gpu::command_buffer_handle) -> void {}

auto gse::dx12::device::cmd_end(const gpu::command_buffer_handle cmd) -> void {
	std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd)->Close();
}

auto gse::dx12::device::cmd_pipeline_barrier(const gpu::command_buffer_handle cmd, const gpu::dependency_info& dep) -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd);
	if (!list) {
		return;
	}
	std::vector<directx::D3D12_RESOURCE_BARRIER> barriers;
	barriers.reserve(dep.image_barriers.size());
	for (const auto& ib : dep.image_barriers) {
		auto* res = std::bit_cast<directx::ID3D12Resource*>(ib.image);
		if (!res) {
			continue;
		}
		const auto before = ib.discard_contents ? directx::resource_state_common : state_from_access(ib.src_access);
		const auto after = state_from_access(ib.dst_access);
		if (before == after) {
			continue;
		}
		barriers.push_back({
			.Type = directx::barrier_type_transition,
			.Transition = {
				.pResource = res,
				.Subresource = directx::resource_barrier_all_subresources,
				.StateBefore = before,
				.StateAfter = after,
			},
		});
	}
	if (!barriers.empty()) {
		list->ResourceBarrier(static_cast<std::uint32_t>(barriers.size()), barriers.data());
	}
}

auto gse::dx12::device::cmd_release_swapchain_to_present(const gpu::command_buffer_handle cmd, const gpu::handle<gpu::image> img, gpu::pipeline_stage_flags, gpu::access_flags) -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd);
	auto* res = std::bit_cast<directx::ID3D12Resource*>(img);
	const auto index = m_swapchain ? m_swapchain->GetCurrentBackBufferIndex() : 0u;
	auto rtv = m_rtv_heap->GetCPUDescriptorHandleForHeapStart();
	rtv.ptr += static_cast<std::size_t>(index) * m_rtv_size;
	const std::array<float, 4> clear = { 0.10f, 0.45f, 0.85f, 1.0f };
	list->ClearRenderTargetView(rtv, clear.data(), 0, nullptr);
	const directx::D3D12_RESOURCE_BARRIER b = {
		.Type = directx::barrier_type_transition,
		.Transition = {
			.pResource = res,
			.Subresource = directx::resource_barrier_all_subresources,
			.StateBefore = directx::resource_state_render_target,
			.StateAfter = directx::resource_state_present,
		},
	};
	list->ResourceBarrier(1, &b);
}

auto gse::dx12::device::frame_command_buffer(gpu::queue_type, const std::uint32_t frame_index) const -> gpu::command_buffer_handle {
	if (m_frames.empty()) {
		return {};
	}
	return std::bit_cast<gpu::command_buffer_handle>(m_frames[frame_index % m_frames.size()].list.get());
}

auto gse::dx12::device::submit(gpu::queue_type, const gpu::submit_info& info, const gpu::handle<gpu::fence> signal_fence) -> void {
	for (const auto& w : info.wait_semaphores) {
		if (auto* sp = std::bit_cast<sync_point*>(w.semaphore); sp && sp->fence) {
			m_graphics_queue->Wait(sp->fence.get(), sp->value);
		}
	}
	std::vector<directx::ID3D12CommandList*> lists;
	for (const auto& cb : info.command_buffers) {
		if (auto* list = std::bit_cast<directx::ID3D12CommandList*>(cb.command_buffer)) {
			lists.push_back(list);
		}
	}
	if (!lists.empty()) {
		m_graphics_queue->ExecuteCommandLists(static_cast<std::uint32_t>(lists.size()), lists.data());
		if (const auto r = m_device->GetDeviceRemovedReason(); r != 0) {
			log::println(log::category::render, "dx12: post-ExecuteCommandLists removed=0x{:08x} lists={}", static_cast<std::uint32_t>(r), lists.size());
			log::flush();
		}
	}
	for (const auto& s : info.signal_semaphores) {
		if (auto* sp = std::bit_cast<sync_point*>(s.semaphore); sp && sp->fence) {
			m_graphics_queue->Signal(sp->fence.get(), ++sp->value);
		}
	}
	if (auto* sp = std::bit_cast<sync_point*>(signal_fence); sp && sp->fence) {
		m_graphics_queue->Signal(sp->fence.get(), ++sp->value);
	}
}

auto gse::dx12::device::present(const gpu::present_info& info) -> gpu::result {
	for (const auto& w : info.wait_semaphores) {
		if (auto* sp = std::bit_cast<sync_point*>(w); sp && sp->fence) {
			directx::wait_fence(sp->fence.get(), sp->value, m_idle_event);
		}
	}
	if (m_swapchain) {
		m_swapchain->Present(1, 0);
	}
	return gpu::result::success;
}

auto gse::dx12::device::wait_for_fence(const gpu::handle<gpu::fence> f, std::uint64_t) const -> gpu::result {
	if (auto* sp = std::bit_cast<sync_point*>(f); sp && sp->fence && sp->value != 0) {
		directx::wait_fence(sp->fence.get(), sp->value, m_idle_event);
	}
	return gpu::result::success;
}

auto gse::dx12::device::reset_fence(gpu::handle<gpu::fence>) const -> void {}

auto gse::dx12::device::reset_worker_command_pools(std::uint32_t) -> void {}

auto gse::dx12::device::acquire_worker_command_buffer(gpu::queue_type, std::size_t, const std::uint32_t frame_index) -> gpu::command_buffer_handle {
	return frame_command_buffer(gpu::queue_type::graphics, frame_index);
}

auto gse::dx12::device::create_image_unbound(const gpu::image_create_info&) const -> std::pair<gpu::handle<gpu::image>, gpu::memory_requirements> {
	return {};
}

auto gse::dx12::device::create_buffer_unbound(const gpu::buffer_desc&) const -> std::pair<gpu::handle<gpu::buffer>, gpu::memory_requirements> {
	return {};
}

auto gse::dx12::device::bind_image_memory(gpu::handle<gpu::image>, gpu::device_memory, gpu::device_size) const -> void {}

auto gse::dx12::device::bind_buffer_memory(gpu::handle<gpu::buffer>, gpu::device_memory, gpu::device_size) const -> void {}

auto gse::dx12::device::create_image_view(gpu::handle<gpu::image>, const gpu::image_view_create_info&) const -> gpu::handle<gpu::image_view> {
	return {};
}

auto gse::dx12::device::allocate_aliased_memory(gpu::device_size, std::uint32_t) const -> gpu::device_memory {
	return {};
}

auto gse::dx12::device::free_aliased_memory(gpu::device_memory) const -> void {}

auto gse::dx12::device::find_memory_type_index(std::uint32_t, gpu::memory_property_flags) const -> std::uint32_t {
	return 0;
}

auto gse::dx12::device::host_upload_image_layers(gpu::handle<gpu::image>, std::span<const void* const>, vec2u) const -> void {}

auto gse::dx12::device::begin_one_time_commands(gpu::command_buffer_handle) -> void {}

auto gse::dx12::device::end_commands(const gpu::command_buffer_handle cmd) -> void {
	if (auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd)) {
		list->Close();
	}
}

auto gse::dx12::device::create_transient_command_pool(std::uint32_t) -> gpu::transient_pool_handle {
	const auto index = static_cast<std::uint32_t>(m_transient_pools.size());
	m_transient_pools.emplace_back();
	return gpu::transient_pool_handle{ .index = index };
}

auto gse::dx12::device::allocate_transient_primary(const gpu::transient_pool_handle pool) -> gpu::command_buffer_handle {
	auto& p = m_transient_pools[pool.index];
	if (p.used == p.entries.size()) {
		auto allocator = directx::create_command_allocator(m_device.get());
		auto list = directx::create_command_list(m_device.get(), allocator.get());
		p.entries.push_back(transient_entry{
			.allocator = std::move(allocator),
			.list = std::move(list),
		});
	}
	auto& e = p.entries[p.used++];
	if (!e.list || !e.allocator) {
		return {};
	}
	e.list->Reset(e.allocator.get(), nullptr);
	return std::bit_cast<gpu::command_buffer_handle>(e.list.get());
}

auto gse::dx12::device::transient_pool_try_reset(const gpu::transient_pool_handle pool, const std::uint64_t queue_progress) -> void {
	auto& p = m_transient_pools[pool.index];
	if (p.used == 0 || queue_progress < p.high_water) {
		return;
	}
	for (std::size_t i = 0; i < p.used; ++i) {
		p.entries[i].allocator->Reset();
	}
	p.used = 0;
}

auto gse::dx12::device::transient_pool_mark_in_use(const gpu::transient_pool_handle pool, const std::uint64_t value) -> void {
	auto& p = m_transient_pools[pool.index];
	if (value > p.high_water) {
		p.high_water = value;
	}
}

auto gse::dx12::device::transient_pool_reset_all(const gpu::transient_pool_handle pool) -> void {
	auto& p = m_transient_pools[pool.index];
	for (std::size_t i = 0; i < p.used; ++i) {
		p.entries[i].allocator->Reset();
	}
	p.used = 0;
}

auto gse::dx12::device::create_shader_program(const gpu::shader_program_create_info&) -> gpu::shader_program {
	return {};
}

auto gse::dx12::device::create_semaphore() -> gpu::handle<gpu::semaphore> {
	const std::lock_guard lock(m_mutex);
	m_sync_points.push_back({
		.fence = directx::create_fence(m_device.get(), 0),
		.value = 0,
	});
	return std::bit_cast<gpu::handle<gpu::semaphore>>(&m_sync_points.back());
}

auto gse::dx12::device::create_timeline_semaphore(const std::uint64_t initial_value) -> gpu::handle<gpu::semaphore> {
	const std::lock_guard lock(m_mutex);
	m_sync_points.push_back({
		.fence = directx::create_fence(m_device.get(), initial_value),
		.value = initial_value,
	});
	return std::bit_cast<gpu::handle<gpu::semaphore>>(&m_sync_points.back());
}

auto gse::dx12::device::create_fence(const bool signaled) -> gpu::handle<gpu::fence> {
	const std::lock_guard lock(m_mutex);
	m_sync_points.push_back({
		.fence = directx::create_fence(m_device.get(), 0),
		.value = 0,
	});
	auto& sp = m_sync_points.back();
	if (signaled && sp.fence) {
		sp.value = 1;
		sp.fence->Signal(1);
	}
	return std::bit_cast<gpu::handle<gpu::fence>>(&sp);
}

auto gse::dx12::device::retire_semaphore(const gpu::handle<gpu::semaphore> semaphore) -> void {
	if (auto* sp = std::bit_cast<sync_point*>(semaphore)) {
		sp->fence.reset();
	}
}

auto gse::dx12::device::retire_fence(const gpu::handle<gpu::fence> fence) -> void {
	if (auto* sp = std::bit_cast<sync_point*>(fence)) {
		sp->fence.reset();
	}
}

auto gse::dx12::device::semaphore_counter_value(const gpu::handle<gpu::semaphore> semaphore) const -> std::uint64_t {
	auto* sp = std::bit_cast<sync_point*>(semaphore);
	return sp && sp->fence ? sp->fence->GetCompletedValue() : 0;
}

auto gse::dx12::device::wait_semaphore(const gpu::handle<gpu::semaphore> semaphore, const std::uint64_t value) const -> void {
	if (auto* sp = std::bit_cast<sync_point*>(semaphore); sp && sp->fence) {
		directx::wait_fence(sp->fence.get(), value, m_idle_event);
	}
}

auto gse::dx12::device::create_timestamp_query_pool(std::uint32_t, std::string_view) -> gpu::handle<gpu::query_pool> {
	return {};
}

auto gse::dx12::device::create_pipeline_stats_query_pool(std::uint32_t, gpu::pipeline_statistic_flags, std::string_view) -> gpu::handle<gpu::query_pool> {
	return {};
}

auto gse::dx12::device::query_pool_results(gpu::handle<gpu::query_pool>, std::uint32_t, std::uint32_t, std::uint64_t) const -> std::pair<gpu::query_status, std::vector<std::uint64_t>> {
	return { gpu::query_status::error, {} };
}

auto gse::dx12::device::create_swapchain(const vec2i framebuffer_size, gpu::present_mode, gpu::swap_chain_handle) -> gpu::swap_chain_info {
	wait_idle();

	m_backbuffers.clear();
	m_rtv_heap.reset();
	m_swapchain.reset();

	m_image_count = 3;
	m_extent = vec2u{ static_cast<std::uint32_t>(framebuffer_size.x()), static_cast<std::uint32_t>(framebuffer_size.y()) };

	log::println(log::category::render, "dx12: create_swapchain begin {}x{} hwnd={} queue={} factory={}", m_extent.x(), m_extent.y(), m_hwnd, static_cast<void*>(m_graphics_queue.get()), static_cast<void*>(m_factory.get()));
	log::flush();

	m_swapchain = directx::create_swapchain(m_factory.get(), m_graphics_queue.get(), m_hwnd, m_extent.x(), m_extent.y(), m_image_count, dxgi_format_of(m_surface_fmt));
	log::println(log::category::render, "dx12: swapchain={}", static_cast<void*>(m_swapchain.get()));
	log::flush();

	gpu::swap_chain_info out = {
		.handle = std::bit_cast<gpu::swap_chain_handle>(m_swapchain.get()),
		.extent = m_extent,
		.format = m_surface_fmt,
	};

	if (!m_swapchain) {
		log::println(log::category::render, "dx12: create_swapchain FAILED (null swapchain)");
		log::flush();
		return out;
	}

	m_rtv_heap = directx::create_rtv_heap(m_device.get(), m_image_count);
	m_rtv_size = directx::rtv_descriptor_size(m_device.get());
	log::println(log::category::render, "dx12: rtv_heap={} rtv_size={}", static_cast<void*>(m_rtv_heap.get()), m_rtv_size);
	log::flush();

	m_backbuffers.resize(m_image_count);
	for (std::uint32_t i = 0; i < m_image_count; ++i) {
		m_backbuffers[i] = directx::swapchain_buffer(m_swapchain.get(), i);
		log::println(log::category::render, "dx12: backbuffer[{}]={}", i, static_cast<void*>(m_backbuffers[i].get()));
		log::flush();
		if (!m_backbuffers[i] || !m_rtv_heap) {
			continue;
		}
		auto rtv = m_rtv_heap->GetCPUDescriptorHandleForHeapStart();
		rtv.ptr += static_cast<std::size_t>(i) * m_rtv_size;
		directx::create_render_target_view(m_device.get(), m_backbuffers[i].get(), rtv);
		out.images.push_back(std::bit_cast<gpu::handle<gpu::image>>(m_backbuffers[i].get()));
		out.image_views.push_back(std::bit_cast<gpu::handle<gpu::image_view>>(rtv.ptr));
	}

	log::println(log::category::render, "dx12: create_swapchain end images={}", out.images.size());
	log::flush();

	return out;
}

auto gse::dx12::device::acquire_swapchain_image(gpu::swap_chain_handle, const gpu::handle<gpu::semaphore> wait_semaphore, std::uint64_t) const -> gpu::acquire_next_image_result {
	const auto index = m_swapchain ? m_swapchain->GetCurrentBackBufferIndex() : 0u;
	if (auto* sp = std::bit_cast<sync_point*>(wait_semaphore); sp && sp->fence) {
		sp->fence->Signal(++sp->value);
	}
	return {
		.result = gpu::result::success,
		.image_index = index,
	};
}

auto gse::dx12::device::wait_swapchain_release_fences(gpu::swap_chain_handle) const -> void {}

auto gse::dx12::device::reset_swapchain_release_fence(gpu::swap_chain_handle, std::uint32_t) const -> void {}

auto gse::dx12::device::swapchain_release_fence(gpu::swap_chain_handle, std::uint32_t) const -> gpu::handle<gpu::fence> {
	return {};
}

auto gse::dx12::device::swapchain_past_presentation_timing(gpu::swap_chain_handle) const -> std::vector<gpu::past_present_timing> {
	return {};
}

auto gse::dx12::device::create_blas(const gpu::acceleration_structure_geometry&, std::uint32_t) -> gpu::blas {
	return {};
}

auto gse::dx12::device::create_tlas(std::uint32_t) -> gpu::tlas {
	return {};
}

auto gse::dx12::device::query_blas_build_sizes(const gpu::acceleration_structure_geometry&, std::uint32_t) const -> gpu::acceleration_structure_build_sizes {
	return {};
}

auto gse::dx12::device::acceleration_structure_scratch_alignment() const -> gpu::device_size {
	return 256;
}

auto gse::dx12::device::create_buffer(const gpu::buffer_desc& desc, std::string_view, const std::source_location&) -> gpu::buffer {
	const std::lock_guard lock(m_mutex);
	auto resource = directx::create_upload_buffer(m_device.get(), desc.size);
	if (!resource) {
		log::println(log::category::render, "dx12: create_buffer FAILED size={} removed=0x{:08x}", desc.size, static_cast<std::uint32_t>(m_device->GetDeviceRemovedReason()));
		log::flush();
		return {};
	}
	auto* mapped = static_cast<std::byte*>(directx::map_buffer(resource.get()));
	const auto address = directx::gpu_address(resource.get());
	if (desc.data && mapped) {
		std::memcpy(mapped, desc.data, desc.size);
	}
	auto* raw = resource.get();
	m_owned_buffers.push_back(std::move(resource));

	gpu::bindless_slot slot;
	if (desc.bindless) {
		slot = m_buffer_pool.allocate();
	}

	return gpu::buffer(
		std::bit_cast<gpu::handle<gpu::buffer>>(raw),
		desc.size,
		address,
		mapped,
		slot
	);
}

auto gse::dx12::device::create_image(const gpu::image_desc& desc, std::string_view) -> gpu::image {
	const std::lock_guard lock(m_mutex);
	const auto dimension = desc.depth > 1 ? directx::dimension_texture_3d : directx::dimension_texture_2d;
	const std::uint32_t depth_or_layers = desc.depth > 1 ? desc.depth : 1;

	int flag_bits = static_cast<int>(directx::resource_flag_none);
	if (desc.usage.test(gpu::image_flag::color_attachment)) {
		flag_bits |= static_cast<int>(directx::resource_flag_allow_render_target);
	}
	if (desc.usage.test(gpu::image_flag::depth_attachment)) {
		flag_bits |= static_cast<int>(directx::resource_flag_allow_depth_stencil);
	}
	if (desc.usage.test(gpu::image_flag::storage)) {
		flag_bits |= static_cast<int>(directx::resource_flag_allow_unordered_access);
	}
	const auto flags = static_cast<directx::D3D12_RESOURCE_FLAGS>(flag_bits);

	auto resource = directx::create_committed_texture(m_device.get(), dimension, dxgi_format_of(desc.format), desc.size.x(), desc.size.y(), depth_or_layers, 1, flags);
	if (!resource) {
		log::println(log::category::render, "dx12: create_image FAILED {}x{}x{} fmt={} flags={}", desc.size.x(), desc.size.y(), depth_or_layers, static_cast<int>(desc.format), static_cast<int>(flags));
		log::flush();
		return {};
	}
	auto* raw = resource.get();
	m_owned_images.push_back(std::move(resource));

	gpu::bindless_slot storage_slot;
	gpu::bindless_slot sampled_slot;
	if (desc.bindless) {
		storage_slot = m_image_pool.allocate();
		sampled_slot = m_image_pool.allocate();
	}

	return gpu::image(
		std::bit_cast<gpu::handle<gpu::image>>(raw),
		std::bit_cast<gpu::handle<gpu::image_view>>(raw),
		static_cast<gpu::image_format_value>(desc.format),
		vec3u{ desc.size.x(), desc.size.y(), desc.depth },
		{},
		storage_slot,
		sampled_slot
	);
}

auto gse::dx12::device::allocate_buffer_slot() -> gpu::bindless_handle {
	const std::lock_guard lock(m_mutex);
	return gpu::bindless_handle(&m_buffer_pool, m_buffer_pool.allocate());
}

auto gse::dx12::device::allocate_image_slot() -> gpu::bindless_handle {
	const std::lock_guard lock(m_mutex);
	return gpu::bindless_handle(&m_image_pool, m_image_pool.allocate());
}

auto gse::dx12::device::write_storage_buffer(gpu::bindless_slot, gpu::device_address, gpu::device_size) -> void {}

auto gse::dx12::device::write_uniform_buffer(gpu::bindless_slot, gpu::device_address, gpu::device_size) -> void {}

auto gse::dx12::device::write_acceleration_structure(gpu::bindless_slot, gpu::device_address) -> void {}

auto gse::dx12::device::write_sampled_image(gpu::bindless_slot, const gpu::image&) -> void {}

auto gse::dx12::device::register_sampler(const gpu::sampler_desc&) -> gpu::bindless_handle {
	const std::lock_guard lock(m_mutex);
	return gpu::bindless_handle(&m_sampler_pool, m_sampler_pool.allocate());
}

auto gse::dx12::device::register_texture(const gpu::image&, const gpu::sampler_desc&) -> gpu::bindless_handle {
	const std::lock_guard lock(m_mutex);
	return gpu::bindless_handle(&m_texture_pool, m_texture_pool.allocate());
}

auto gse::dx12::device::bindless_layout() const -> gpu::bindless_layout {
	return {};
}

auto gse::dx12::device::bindless_resource_heap_binding() const -> gpu::bindless_heap_binding {
	return {};
}

auto gse::dx12::device::bindless_sampler_heap_binding() const -> gpu::bindless_heap_binding {
	return {};
}

auto gse::dx12::device::create_sampler(const gpu::sampler_desc&) -> gpu::handle<gpu::sampler> {
	return {};
}

auto gse::dx12::device::collect_garbage() -> void {}

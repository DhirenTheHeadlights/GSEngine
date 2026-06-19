module;

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d3d12.h>
#include <dxgi1_6.h>

export module gse.directx;

import std;

export namespace gse::directx {
	using ::ID3D12CommandAllocator;
	using ::ID3D12CommandQueue;
	using ::ID3D12Debug;
	using ::ID3D12DescriptorHeap;
	using ::ID3D12Device;
	using ::ID3D12Fence;
	using ::ID3D12GraphicsCommandList;
	using ::ID3D12Resource;
	using ::ID3D12PipelineState;
	using ::ID3D12RootSignature;
	using ::IDXGIFactory4;
	using ::IDXGISwapChain1;
	using ::IDXGISwapChain3;

	using ::ID3D12CommandList;

	using ::D3D12_CPU_DESCRIPTOR_HANDLE;
	using ::D3D12_GPU_DESCRIPTOR_HANDLE;
	using ::D3D12_RESOURCE_BARRIER;
	using ::D3D12_RESOURCE_DIMENSION;
	using ::D3D12_RESOURCE_FLAGS;
	using ::D3D12_RESOURCE_STATES;
	using ::DXGI_FORMAT;

	constexpr DXGI_FORMAT format_r8g8b8a8_unorm = DXGI_FORMAT_R8G8B8A8_UNORM;
	constexpr DXGI_FORMAT format_r8g8b8a8_srgb = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	constexpr DXGI_FORMAT format_b8g8r8a8_unorm = DXGI_FORMAT_B8G8R8A8_UNORM;
	constexpr DXGI_FORMAT format_b8g8r8a8_srgb = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

	constexpr DXGI_FORMAT format_d32_float = DXGI_FORMAT_D32_FLOAT;

	constexpr auto barrier_type_transition = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	constexpr auto resource_state_common = D3D12_RESOURCE_STATE_COMMON;
	constexpr auto resource_state_present = D3D12_RESOURCE_STATE_PRESENT;
	constexpr auto resource_state_render_target = D3D12_RESOURCE_STATE_RENDER_TARGET;
	constexpr auto resource_state_depth_write = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	constexpr auto resource_state_depth_read = D3D12_RESOURCE_STATE_DEPTH_READ;
	constexpr auto resource_state_unordered_access = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	constexpr auto resource_state_copy_dest = D3D12_RESOURCE_STATE_COPY_DEST;
	constexpr auto resource_state_copy_source = D3D12_RESOURCE_STATE_COPY_SOURCE;
	constexpr auto resource_state_shader_resource = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	constexpr auto resource_barrier_all_subresources = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	constexpr auto dimension_texture_2d = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	constexpr auto dimension_texture_3d = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	constexpr auto resource_flag_none = D3D12_RESOURCE_FLAG_NONE;
	constexpr auto resource_flag_allow_render_target = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	constexpr auto resource_flag_allow_depth_stencil = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	constexpr auto resource_flag_allow_unordered_access = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	constexpr auto resource_flag_color_target = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	template <typename T>
	class com_ptr {
	public:
		com_ptr() = default;

		explicit com_ptr(
			T* ptr
		);

		com_ptr(
			com_ptr&& other
		) noexcept;

		auto operator=(
			com_ptr&& other
		) noexcept -> com_ptr&;

		com_ptr(const com_ptr&) = delete;
		auto operator=(const com_ptr&) -> com_ptr& = delete;

		~com_ptr();

		[[nodiscard]] auto get() const -> T*;

		auto operator->() const -> T*;

		explicit operator bool() const;

		auto reset() -> void;

		[[nodiscard]] auto put() -> T**;

	private:
		T* m_ptr = nullptr;
	};

	auto enable_debug_layer() -> void;

	[[nodiscard]] auto create_factory() -> com_ptr<IDXGIFactory4>;

	[[nodiscard]] auto create_device() -> com_ptr<ID3D12Device>;

	[[nodiscard]] auto create_direct_queue(
		ID3D12Device* device
	) -> com_ptr<ID3D12CommandQueue>;

	[[nodiscard]] auto create_fence(
		ID3D12Device* device,
		std::uint64_t initial_value
	) -> com_ptr<ID3D12Fence>;

	[[nodiscard]] auto create_command_allocator(
		ID3D12Device* device
	) -> com_ptr<ID3D12CommandAllocator>;

	[[nodiscard]] auto create_command_list(
		ID3D12Device* device,
		ID3D12CommandAllocator* allocator
	) -> com_ptr<ID3D12GraphicsCommandList>;

	[[nodiscard]] auto create_rtv_heap(
		ID3D12Device* device,
		std::uint32_t descriptor_count
	) -> com_ptr<ID3D12DescriptorHeap>;

	[[nodiscard]] auto rtv_descriptor_size(
		ID3D12Device* device
	) -> std::uint32_t;

	[[nodiscard]] auto create_swapchain(
		IDXGIFactory4* factory,
		ID3D12CommandQueue* queue,
		void* hwnd,
		std::uint32_t width,
		std::uint32_t height,
		std::uint32_t buffer_count,
		DXGI_FORMAT format
	) -> com_ptr<IDXGISwapChain3>;

	[[nodiscard]] auto swapchain_buffer(
		IDXGISwapChain3* swapchain,
		std::uint32_t index
	) -> com_ptr<ID3D12Resource>;

	auto create_render_target_view(
		ID3D12Device* device,
		ID3D12Resource* resource,
		D3D12_CPU_DESCRIPTOR_HANDLE handle
	) -> void;

	[[nodiscard]] auto create_wait_event() -> void*;

	auto wait_fence(
		ID3D12Fence* fence,
		std::uint64_t value,
		void* event
	) -> void;

	[[nodiscard]] auto create_upload_buffer(
		ID3D12Device* device,
		std::uint64_t size
	) -> com_ptr<ID3D12Resource>;

	[[nodiscard]] auto map_buffer(
		ID3D12Resource* buffer
	) -> void*;

	[[nodiscard]] auto gpu_address(
		ID3D12Resource* buffer
	) -> std::uint64_t;

	[[nodiscard]] auto create_committed_texture(
		ID3D12Device* device,
		D3D12_RESOURCE_DIMENSION dimension,
		DXGI_FORMAT format,
		std::uint32_t width,
		std::uint32_t height,
		std::uint32_t depth_or_array_layers,
		std::uint32_t mip_levels,
		D3D12_RESOURCE_FLAGS flags
	) -> com_ptr<ID3D12Resource>;

	[[nodiscard]] auto create_cbv_srv_uav_heap(
		ID3D12Device* device,
		std::uint32_t descriptor_count,
		bool shader_visible
	) -> com_ptr<ID3D12DescriptorHeap>;

	[[nodiscard]] auto create_sampler_heap(
		ID3D12Device* device,
		std::uint32_t descriptor_count,
		bool shader_visible
	) -> com_ptr<ID3D12DescriptorHeap>;

	[[nodiscard]] auto create_dsv_heap(
		ID3D12Device* device,
		std::uint32_t descriptor_count
	) -> com_ptr<ID3D12DescriptorHeap>;

	[[nodiscard]] auto cbv_srv_uav_descriptor_size(
		ID3D12Device* device
	) -> std::uint32_t;

	[[nodiscard]] auto sampler_descriptor_size(
		ID3D12Device* device
	) -> std::uint32_t;

	[[nodiscard]] auto dsv_descriptor_size(
		ID3D12Device* device
	) -> std::uint32_t;

	[[nodiscard]] auto descriptor_heap_cpu_start(
		ID3D12DescriptorHeap* heap
	) -> D3D12_CPU_DESCRIPTOR_HANDLE;

	[[nodiscard]] auto descriptor_heap_gpu_start(
		ID3D12DescriptorHeap* heap
	) -> D3D12_GPU_DESCRIPTOR_HANDLE;

	[[nodiscard]] auto offset_cpu_handle(
		D3D12_CPU_DESCRIPTOR_HANDLE base,
		std::uint32_t index,
		std::uint32_t increment
	) -> D3D12_CPU_DESCRIPTOR_HANDLE;

	[[nodiscard]] auto offset_gpu_handle(
		D3D12_GPU_DESCRIPTOR_HANDLE base,
		std::uint32_t index,
		std::uint32_t increment
	) -> D3D12_GPU_DESCRIPTOR_HANDLE;

	auto create_depth_stencil_view(
		ID3D12Device* device,
		ID3D12Resource* resource,
		DXGI_FORMAT format,
		D3D12_CPU_DESCRIPTOR_HANDLE handle
	) -> void;

	auto create_texture_srv(
		ID3D12Device* device,
		ID3D12Resource* resource,
		DXGI_FORMAT format,
		D3D12_CPU_DESCRIPTOR_HANDLE handle
	) -> void;

	auto create_texture_uav(
		ID3D12Device* device,
		ID3D12Resource* resource,
		DXGI_FORMAT format,
		D3D12_CPU_DESCRIPTOR_HANDLE handle
	) -> void;

	auto create_constant_buffer_view(
		ID3D12Device* device,
		std::uint64_t address,
		std::uint32_t size,
		D3D12_CPU_DESCRIPTOR_HANDLE handle
	) -> void;

	struct sampler_params {
		bool min_linear = true;
		bool mag_linear = true;
		bool mip_linear = true;
		bool anisotropy = false;
		bool comparison = false;
		std::uint32_t max_anisotropy = 1;
		std::uint32_t comparison_func = 0;
		std::uint32_t address_u = 0;
		std::uint32_t address_v = 0;
		std::uint32_t address_w = 0;
		std::uint32_t border = 0;
		float min_lod = 0.0f;
		float max_lod = 0.0f;
	};

	auto create_sampler_descriptor(
		ID3D12Device* device,
		const sampler_params& params,
		D3D12_CPU_DESCRIPTOR_HANDLE handle
	) -> void;

	[[nodiscard]] auto create_bindless_root_signature(
		ID3D12Device* device,
		std::uint32_t num_root_constants
	) -> com_ptr<ID3D12RootSignature>;

	[[nodiscard]] auto create_gpu_upload_buffer(
		ID3D12Device* device,
		std::uint64_t size
	) -> com_ptr<ID3D12Resource>;

	auto create_raw_buffer_uav(
		ID3D12Device* device,
		ID3D12Resource* resource,
		std::uint32_t first_element,
		std::uint32_t num_elements,
		D3D12_CPU_DESCRIPTOR_HANDLE handle
	) -> void;

	[[nodiscard]] auto create_compute_pipeline_state(
		ID3D12Device* device,
		ID3D12RootSignature* root_signature,
		const void* bytecode,
		std::size_t bytecode_size
	) -> com_ptr<ID3D12PipelineState>;
}

template <typename T>
gse::directx::com_ptr<T>::com_ptr(T* ptr) : m_ptr(ptr) {}

template <typename T>
gse::directx::com_ptr<T>::com_ptr(com_ptr&& other) noexcept : m_ptr(other.m_ptr) {
	other.m_ptr = nullptr;
}

template <typename T>
auto gse::directx::com_ptr<T>::operator=(com_ptr&& other) noexcept -> com_ptr& {
	if (this != &other) {
		reset();
		m_ptr = other.m_ptr;
		other.m_ptr = nullptr;
	}
	return *this;
}

template <typename T>
gse::directx::com_ptr<T>::~com_ptr() {
	reset();
}

template <typename T>
auto gse::directx::com_ptr<T>::get() const -> T* {
	return m_ptr;
}

template <typename T>
auto gse::directx::com_ptr<T>::operator->() const -> T* {
	return m_ptr;
}

template <typename T>
gse::directx::com_ptr<T>::operator bool() const {
	return m_ptr != nullptr;
}

template <typename T>
auto gse::directx::com_ptr<T>::reset() -> void {
	if (m_ptr) {
		m_ptr->Release();
		m_ptr = nullptr;
	}
}

template <typename T>
auto gse::directx::com_ptr<T>::put() -> T** {
	reset();
	return &m_ptr;
}

auto gse::directx::enable_debug_layer() -> void {
	ID3D12Debug* debug = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
		debug->EnableDebugLayer();
		debug->Release();
	}
}

auto gse::directx::create_factory() -> com_ptr<IDXGIFactory4> {
	com_ptr<IDXGIFactory4> factory;
	CreateDXGIFactory2(0, IID_PPV_ARGS(factory.put()));
	return factory;
}

auto gse::directx::create_device() -> com_ptr<ID3D12Device> {
	com_ptr<ID3D12Device> device;
	D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.put()));
	return device;
}

auto gse::directx::create_direct_queue(ID3D12Device* device) -> com_ptr<ID3D12CommandQueue> {
	const D3D12_COMMAND_QUEUE_DESC desc = {
		.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
	};
	com_ptr<ID3D12CommandQueue> queue;
	device->CreateCommandQueue(&desc, IID_PPV_ARGS(queue.put()));
	return queue;
}

auto gse::directx::create_fence(ID3D12Device* device, const std::uint64_t initial_value) -> com_ptr<ID3D12Fence> {
	com_ptr<ID3D12Fence> fence;
	device->CreateFence(initial_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.put()));
	return fence;
}

auto gse::directx::create_command_allocator(ID3D12Device* device) -> com_ptr<ID3D12CommandAllocator> {
	com_ptr<ID3D12CommandAllocator> allocator;
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(allocator.put()));
	return allocator;
}

auto gse::directx::create_command_list(ID3D12Device* device, ID3D12CommandAllocator* allocator) -> com_ptr<ID3D12GraphicsCommandList> {
	com_ptr<ID3D12GraphicsCommandList> list;
	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(list.put()));
	if (list) {
		list->Close();
	}
	return list;
}

auto gse::directx::create_rtv_heap(ID3D12Device* device, const std::uint32_t descriptor_count) -> com_ptr<ID3D12DescriptorHeap> {
	const D3D12_DESCRIPTOR_HEAP_DESC desc = {
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		.NumDescriptors = descriptor_count,
	};
	com_ptr<ID3D12DescriptorHeap> heap;
	device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(heap.put()));
	return heap;
}

auto gse::directx::rtv_descriptor_size(ID3D12Device* device) -> std::uint32_t {
	return device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

auto gse::directx::create_swapchain(IDXGIFactory4* factory, ID3D12CommandQueue* queue, void* hwnd, const std::uint32_t width, const std::uint32_t height, const std::uint32_t buffer_count, const DXGI_FORMAT format) -> com_ptr<IDXGISwapChain3> {
	const DXGI_SWAP_CHAIN_DESC1 desc = {
		.Width = width,
		.Height = height,
		.Format = format,
		.SampleDesc = {
			.Count = 1,
		},
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = buffer_count,
		.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
	};

	IDXGISwapChain1* sc1 = nullptr;
	factory->CreateSwapChainForHwnd(queue, static_cast<HWND>(hwnd), &desc, nullptr, nullptr, &sc1);

	com_ptr<IDXGISwapChain3> swapchain;
	if (sc1) {
		sc1->QueryInterface(IID_PPV_ARGS(swapchain.put()));
		sc1->Release();
	}
	return swapchain;
}

auto gse::directx::swapchain_buffer(IDXGISwapChain3* swapchain, const std::uint32_t index) -> com_ptr<ID3D12Resource> {
	com_ptr<ID3D12Resource> buffer;
	swapchain->GetBuffer(index, IID_PPV_ARGS(buffer.put()));
	return buffer;
}

auto gse::directx::create_render_target_view(ID3D12Device* device, ID3D12Resource* resource, const D3D12_CPU_DESCRIPTOR_HANDLE handle) -> void {
	device->CreateRenderTargetView(resource, nullptr, handle);
}

auto gse::directx::create_wait_event() -> void* {
	return CreateEventW(nullptr, FALSE, FALSE, nullptr);
}

auto gse::directx::wait_fence(ID3D12Fence* fence, const std::uint64_t value, void* event) -> void {
	if (fence->GetCompletedValue() < value) {
		fence->SetEventOnCompletion(value, event);
		WaitForSingleObject(event, INFINITE);
	}
}

auto gse::directx::create_upload_buffer(ID3D12Device* device, const std::uint64_t size) -> com_ptr<ID3D12Resource> {
	const D3D12_HEAP_PROPERTIES heap = {
		.Type = D3D12_HEAP_TYPE_UPLOAD,
	};

	const D3D12_RESOURCE_DESC desc = {
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Width = size == 0 ? 1 : size,
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_UNKNOWN,
		.SampleDesc = {
			.Count = 1,
		},
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
	};

	com_ptr<ID3D12Resource> resource;
	device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(resource.put()));
	return resource;
}

auto gse::directx::map_buffer(ID3D12Resource* buffer) -> void* {
	void* ptr = nullptr;
	buffer->Map(0, nullptr, &ptr);
	return ptr;
}

auto gse::directx::gpu_address(ID3D12Resource* buffer) -> std::uint64_t {
	return buffer->GetGPUVirtualAddress();
}

auto gse::directx::create_committed_texture(ID3D12Device* device, const D3D12_RESOURCE_DIMENSION dimension, const DXGI_FORMAT format, const std::uint32_t width, const std::uint32_t height, const std::uint32_t depth_or_array_layers, const std::uint32_t mip_levels, const D3D12_RESOURCE_FLAGS flags) -> com_ptr<ID3D12Resource> {
	const D3D12_HEAP_PROPERTIES heap = {
		.Type = D3D12_HEAP_TYPE_DEFAULT,
	};

	const D3D12_RESOURCE_DESC desc = {
		.Dimension = dimension,
		.Width = width,
		.Height = height,
		.DepthOrArraySize = static_cast<std::uint16_t>(depth_or_array_layers),
		.MipLevels = static_cast<std::uint16_t>(mip_levels),
		.Format = format,
		.SampleDesc = {
			.Count = 1,
		},
		.Flags = flags,
	};

	com_ptr<ID3D12Resource> resource;
	device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(resource.put()));
	return resource;
}

auto gse::directx::create_cbv_srv_uav_heap(ID3D12Device* device, const std::uint32_t descriptor_count, const bool shader_visible) -> com_ptr<ID3D12DescriptorHeap> {
	const D3D12_DESCRIPTOR_HEAP_DESC desc = {
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		.NumDescriptors = descriptor_count,
		.Flags = shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
	};
	com_ptr<ID3D12DescriptorHeap> heap;
	device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(heap.put()));
	return heap;
}

auto gse::directx::create_sampler_heap(ID3D12Device* device, const std::uint32_t descriptor_count, const bool shader_visible) -> com_ptr<ID3D12DescriptorHeap> {
	const D3D12_DESCRIPTOR_HEAP_DESC desc = {
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
		.NumDescriptors = descriptor_count,
		.Flags = shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
	};
	com_ptr<ID3D12DescriptorHeap> heap;
	device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(heap.put()));
	return heap;
}

auto gse::directx::create_dsv_heap(ID3D12Device* device, const std::uint32_t descriptor_count) -> com_ptr<ID3D12DescriptorHeap> {
	const D3D12_DESCRIPTOR_HEAP_DESC desc = {
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		.NumDescriptors = descriptor_count,
	};
	com_ptr<ID3D12DescriptorHeap> heap;
	device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(heap.put()));
	return heap;
}

auto gse::directx::cbv_srv_uav_descriptor_size(ID3D12Device* device) -> std::uint32_t {
	return device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

auto gse::directx::sampler_descriptor_size(ID3D12Device* device) -> std::uint32_t {
	return device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
}

auto gse::directx::dsv_descriptor_size(ID3D12Device* device) -> std::uint32_t {
	return device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

auto gse::directx::descriptor_heap_cpu_start(ID3D12DescriptorHeap* heap) -> D3D12_CPU_DESCRIPTOR_HANDLE {
	return heap->GetCPUDescriptorHandleForHeapStart();
}

auto gse::directx::descriptor_heap_gpu_start(ID3D12DescriptorHeap* heap) -> D3D12_GPU_DESCRIPTOR_HANDLE {
	return heap->GetGPUDescriptorHandleForHeapStart();
}

auto gse::directx::offset_cpu_handle(const D3D12_CPU_DESCRIPTOR_HANDLE base, const std::uint32_t index, const std::uint32_t increment) -> D3D12_CPU_DESCRIPTOR_HANDLE {
	return {
		.ptr = base.ptr + static_cast<std::size_t>(index) * increment,
	};
}

auto gse::directx::offset_gpu_handle(const D3D12_GPU_DESCRIPTOR_HANDLE base, const std::uint32_t index, const std::uint32_t increment) -> D3D12_GPU_DESCRIPTOR_HANDLE {
	return {
		.ptr = base.ptr + static_cast<std::uint64_t>(index) * increment,
	};
}

auto gse::directx::create_depth_stencil_view(ID3D12Device* device, ID3D12Resource* resource, const DXGI_FORMAT format, const D3D12_CPU_DESCRIPTOR_HANDLE handle) -> void {
	const D3D12_DEPTH_STENCIL_VIEW_DESC desc = {
		.Format = format,
		.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
	};
	device->CreateDepthStencilView(resource, &desc, handle);
}

auto gse::directx::create_texture_srv(ID3D12Device* device, ID3D12Resource* resource, const DXGI_FORMAT format, const D3D12_CPU_DESCRIPTOR_HANDLE handle) -> void {
	const D3D12_SHADER_RESOURCE_VIEW_DESC desc = {
		.Format = format,
		.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Texture2D = {
			.MipLevels = 1,
		},
	};
	device->CreateShaderResourceView(resource, &desc, handle);
}

auto gse::directx::create_texture_uav(ID3D12Device* device, ID3D12Resource* resource, const DXGI_FORMAT format, const D3D12_CPU_DESCRIPTOR_HANDLE handle) -> void {
	const D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {
		.Format = format,
		.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
	};
	device->CreateUnorderedAccessView(resource, nullptr, &desc, handle);
}

auto gse::directx::create_constant_buffer_view(ID3D12Device* device, const std::uint64_t address, const std::uint32_t size, const D3D12_CPU_DESCRIPTOR_HANDLE handle) -> void {
	const D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {
		.BufferLocation = address,
		.SizeInBytes = (size + 255u) & ~255u,
	};
	device->CreateConstantBufferView(&desc, handle);
}

auto gse::directx::create_sampler_descriptor(ID3D12Device* device, const sampler_params& params, const D3D12_CPU_DESCRIPTOR_HANDLE handle) -> void {
	const auto address = [](const std::uint32_t code) -> D3D12_TEXTURE_ADDRESS_MODE {
		switch (code) {
			case 1: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			case 2: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			case 3: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			default: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		}
	};
	const auto reduction = params.comparison ? D3D12_FILTER_REDUCTION_TYPE_COMPARISON : D3D12_FILTER_REDUCTION_TYPE_STANDARD;
	const D3D12_FILTER filter = params.anisotropy
		? D3D12_ENCODE_ANISOTROPIC_FILTER(reduction)
		: D3D12_ENCODE_BASIC_FILTER(
			params.min_linear ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT,
			params.mag_linear ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT,
			params.mip_linear ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT,
			reduction);

	D3D12_SAMPLER_DESC desc = {
		.Filter = filter,
		.AddressU = address(params.address_u),
		.AddressV = address(params.address_v),
		.AddressW = address(params.address_w),
		.MipLODBias = 0.0f,
		.MaxAnisotropy = params.max_anisotropy,
		.ComparisonFunc = static_cast<D3D12_COMPARISON_FUNC>(D3D12_COMPARISON_FUNC_NEVER + params.comparison_func),
		.MinLOD = params.min_lod,
		.MaxLOD = params.max_lod,
	};
	if (params.border == 0) {
		desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 1.0f;
	} else if (params.border == 1) {
		desc.BorderColor[3] = 1.0f;
	}
	device->CreateSampler(&desc, handle);
}

auto gse::directx::create_bindless_root_signature(ID3D12Device* device, const std::uint32_t num_root_constants) -> com_ptr<ID3D12RootSignature> {
	const D3D12_ROOT_PARAMETER param = {
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
		.Constants = {
			.ShaderRegister = 0,
			.RegisterSpace = 0,
			.Num32BitValues = num_root_constants,
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
	};
	const D3D12_ROOT_SIGNATURE_DESC desc = {
		.NumParameters = 1,
		.pParameters = &param,
		.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED,
	};
	com_ptr<ID3DBlob> blob;
	com_ptr<ID3DBlob> error;
	D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, blob.put(), error.put());
	com_ptr<ID3D12RootSignature> root_signature;
	if (blob) {
		device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(root_signature.put()));
	}
	return root_signature;
}

auto gse::directx::create_gpu_upload_buffer(ID3D12Device* device, const std::uint64_t size) -> com_ptr<ID3D12Resource> {
	constexpr auto gpu_upload = static_cast<D3D12_HEAP_TYPE>(5);
	const D3D12_HEAP_PROPERTIES heap = {
		.Type = gpu_upload,
	};

	const D3D12_RESOURCE_DESC desc = {
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Width = size == 0 ? 1 : size,
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_UNKNOWN,
		.SampleDesc = {
			.Count = 1,
		},
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
	};

	com_ptr<ID3D12Resource> resource;
	device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(resource.put()));
	return resource;
}

auto gse::directx::create_raw_buffer_uav(ID3D12Device* device, ID3D12Resource* resource, const std::uint32_t first_element, const std::uint32_t num_elements, const D3D12_CPU_DESCRIPTOR_HANDLE handle) -> void {
	const D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {
		.Format = DXGI_FORMAT_R32_TYPELESS,
		.ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
		.Buffer = {
			.FirstElement = first_element,
			.NumElements = num_elements,
			.Flags = D3D12_BUFFER_UAV_FLAG_RAW,
		},
	};
	device->CreateUnorderedAccessView(resource, nullptr, &desc, handle);
}

auto gse::directx::create_compute_pipeline_state(ID3D12Device* device, ID3D12RootSignature* root_signature, const void* bytecode, const std::size_t bytecode_size) -> com_ptr<ID3D12PipelineState> {
	const D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {
		.pRootSignature = root_signature,
		.CS = {
			.pShaderBytecode = bytecode,
			.BytecodeLength = bytecode_size,
		},
	};
	com_ptr<ID3D12PipelineState> pso;
	device->CreateComputePipelineState(&desc, IID_PPV_ARGS(pso.put()));
	return pso;
}

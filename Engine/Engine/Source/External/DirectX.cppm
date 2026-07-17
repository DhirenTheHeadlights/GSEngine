module;

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <directx/d3d12.h>
#include <dxgi1_6.h>
#include <directx/d3dx12_pipeline_state_stream.h>

__CRT_UUID_DECL(ID3D12Debug, 0x344488b7, 0x6846, 0x474b, 0xb9, 0x89, 0xf0, 0x27, 0x44, 0x82, 0x45, 0xe0)
__CRT_UUID_DECL(ID3D12Debug1, 0xaffaa4ca, 0x63fe, 0x4d8e, 0xb8, 0xad, 0x15, 0x90, 0x00, 0xaf, 0x43, 0x04)
__CRT_UUID_DECL(ID3D12InfoQueue, 0x0742a90b, 0xc387, 0x483f, 0xb9, 0x46, 0x30, 0xa7, 0xe4, 0xe6, 0x14, 0x58)
__CRT_UUID_DECL(ID3D12Device, 0x189819f1, 0x1db6, 0x4b57, 0xbe, 0x54, 0x18, 0x21, 0x33, 0x9b, 0x85, 0xf7)
__CRT_UUID_DECL(ID3D12CommandQueue, 0x0ec870a6, 0x5d7e, 0x4c22, 0x8c, 0xfc, 0x5b, 0xaa, 0xe0, 0x76, 0x16, 0xed)
__CRT_UUID_DECL(ID3D12Fence, 0x0a753dcf, 0xc4d8, 0x4b91, 0xad, 0xf6, 0xbe, 0x5a, 0x60, 0xd9, 0x5a, 0x76)
__CRT_UUID_DECL(ID3D12CommandAllocator, 0x6102dee4, 0xaf59, 0x4b09, 0xb9, 0x99, 0xb4, 0x4d, 0x73, 0xf0, 0x9b, 0x24)
__CRT_UUID_DECL(ID3D12GraphicsCommandList, 0x5b160d0f, 0xac1b, 0x4185, 0x8b, 0xa8, 0xb3, 0xae, 0x42, 0xa5, 0xa4, 0x55)
__CRT_UUID_DECL(ID3D12CommandSignature, 0xc36a797c, 0xec80, 0x4f0a, 0x89, 0x85, 0xa7, 0xb2, 0x47, 0x50, 0x82, 0xd1)
__CRT_UUID_DECL(ID3D12DescriptorHeap, 0x8efb471d, 0x616c, 0x4f49, 0x90, 0xf7, 0x12, 0x7b, 0xb7, 0x63, 0xfa, 0x51)
__CRT_UUID_DECL(ID3D12Resource, 0x696442be, 0xa72e, 0x4059, 0xbc, 0x79, 0x5b, 0x5c, 0x98, 0x04, 0x0f, 0xad)
__CRT_UUID_DECL(ID3D12RootSignature, 0xc54a6b66, 0x72df, 0x4ee8, 0x8b, 0xe5, 0xa9, 0x46, 0xa1, 0x42, 0x92, 0x14)
__CRT_UUID_DECL(ID3D12PipelineState, 0x765a30f3, 0xf624, 0x4c6f, 0xa8, 0x28, 0xac, 0xe9, 0x48, 0x62, 0x24, 0x45)
__CRT_UUID_DECL(ID3D12Device2, 0x30baa41e, 0xb15b, 0x475c, 0xa0, 0xbb, 0x1a, 0xf5, 0xc5, 0xb6, 0x43, 0x28)
__CRT_UUID_DECL(ID3D12GraphicsCommandList6, 0xc3827890, 0xe548, 0x4cfa, 0x96, 0xcf, 0x56, 0x89, 0xa9, 0x37, 0x0f, 0x80)
__CRT_UUID_DECL(ID3D12Device5, 0x8b4f173b, 0x2fea, 0x4b80, 0x8f, 0x58, 0x43, 0x07, 0x19, 0x1a, 0xb9, 0x5d)
__CRT_UUID_DECL(ID3D12GraphicsCommandList4, 0x8754318e, 0xd3a9, 0x4541, 0x98, 0xcf, 0x64, 0x5b, 0x50, 0xdc, 0x48, 0x74)
__CRT_UUID_DECL(ID3D12QueryHeap, 0x0d9658ae, 0xed45, 0x469e, 0xa6, 0x1d, 0x97, 0x0e, 0xc5, 0x83, 0xca, 0xb4)
__CRT_UUID_DECL(ID3D12DeviceRemovedExtendedDataSettings, 0x82bc481c, 0x6b9b, 0x4030, 0xae, 0xdb, 0x7e, 0xe3, 0xd1, 0xdf, 0x1e, 0x63)
__CRT_UUID_DECL(ID3D12DeviceRemovedExtendedData, 0x98931d33, 0x5ae8, 0x4791, 0xaa, 0x3c, 0x1a, 0x73, 0xa2, 0x93, 0x4e, 0x71)

export module gse.directx;

import std;

export namespace gse::directx {
	using ::ID3D12CommandAllocator;
	using ::ID3D12CommandQueue;
	using ::ID3D12Debug;
	using ::ID3D12Debug1;
	using ::ID3D12InfoQueue;
	using ::ID3D12DescriptorHeap;
	using ::ID3D12Device;
	using ::ID3D12Device2;
	using ::ID3D12Fence;
	using ::ID3D12GraphicsCommandList;
	using ::ID3D12GraphicsCommandList6;
	using ::ID3D12Resource;
	using ::ID3D12QueryHeap;
	using ::ID3D12PipelineState;
	using ::ID3D12RootSignature;
	using ::ID3D12CommandSignature;
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
	using ::D3D12_AUTO_BREADCRUMB_OP;
	using ::DXGI_FORMAT;

	constexpr DXGI_FORMAT format_r8g8b8a8_unorm = DXGI_FORMAT_R8G8B8A8_UNORM;
	constexpr DXGI_FORMAT format_r8g8b8a8_srgb = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	constexpr DXGI_FORMAT format_b8g8r8a8_unorm = DXGI_FORMAT_B8G8R8A8_UNORM;
	constexpr DXGI_FORMAT format_b8g8r8a8_srgb = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

	constexpr DXGI_FORMAT format_d32_float = DXGI_FORMAT_D32_FLOAT;
	constexpr DXGI_FORMAT format_r32_typeless = DXGI_FORMAT_R32_TYPELESS;
	constexpr DXGI_FORMAT format_r32_float = DXGI_FORMAT_R32_FLOAT;

	constexpr DXGI_FORMAT format_r16g16b16a16_float = DXGI_FORMAT_R16G16B16A16_FLOAT;
	constexpr DXGI_FORMAT format_r16g16_float = DXGI_FORMAT_R16G16_FLOAT;
	constexpr DXGI_FORMAT format_r11g11b10_float = DXGI_FORMAT_R11G11B10_FLOAT;
	constexpr DXGI_FORMAT format_r8g8_unorm = DXGI_FORMAT_R8G8_UNORM;
	constexpr DXGI_FORMAT format_r8g8_snorm = DXGI_FORMAT_R8G8_SNORM;
	constexpr DXGI_FORMAT format_r8_unorm = DXGI_FORMAT_R8_UNORM;
	constexpr DXGI_FORMAT format_r32g32b32_float = DXGI_FORMAT_R32G32B32_FLOAT;
	constexpr DXGI_FORMAT format_r32_uint = DXGI_FORMAT_R32_UINT;

	constexpr auto barrier_type_transition = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	constexpr auto barrier_type_uav = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	constexpr auto resource_state_common = D3D12_RESOURCE_STATE_COMMON;
	constexpr auto resource_state_present = D3D12_RESOURCE_STATE_PRESENT;
	constexpr auto resource_state_render_target = D3D12_RESOURCE_STATE_RENDER_TARGET;
	constexpr auto resource_state_depth_write = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	constexpr auto resource_state_depth_read = D3D12_RESOURCE_STATE_DEPTH_READ;
	constexpr auto resource_state_unordered_access = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	constexpr auto resource_state_copy_dest = D3D12_RESOURCE_STATE_COPY_DEST;
	constexpr auto resource_state_copy_source = D3D12_RESOURCE_STATE_COPY_SOURCE;
	constexpr auto resource_state_shader_resource = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	constexpr auto resource_state_raytracing_acceleration_structure = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	constexpr auto resource_state_indirect_argument = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
	constexpr auto resource_barrier_all_subresources = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	constexpr auto dimension_texture_2d = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	constexpr auto dimension_texture_3d = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	constexpr auto resource_flag_none = D3D12_RESOURCE_FLAG_NONE;
	constexpr auto resource_flag_allow_render_target = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	constexpr auto resource_flag_allow_depth_stencil = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	constexpr auto resource_flag_allow_unordered_access = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	constexpr auto resource_flag_color_target = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	constexpr auto format_unknown = DXGI_FORMAT_UNKNOWN;

	using ::D3D12_BLEND;
	using ::D3D12_BLEND_OP;
	using ::D3D12_CULL_MODE;
	using ::D3D12_FILL_MODE;
	using ::D3D12_COMPARISON_FUNC;
	using ::D3D12_PRIMITIVE_TOPOLOGY_TYPE;
	using ::D3D12_PRIMITIVE_TOPOLOGY;

	constexpr auto blend_zero = D3D12_BLEND_ZERO;
	constexpr auto blend_one = D3D12_BLEND_ONE;
	constexpr auto blend_src_color = D3D12_BLEND_SRC_COLOR;
	constexpr auto blend_inv_src_color = D3D12_BLEND_INV_SRC_COLOR;
	constexpr auto blend_dst_color = D3D12_BLEND_DEST_COLOR;
	constexpr auto blend_inv_dst_color = D3D12_BLEND_INV_DEST_COLOR;
	constexpr auto blend_src_alpha = D3D12_BLEND_SRC_ALPHA;
	constexpr auto blend_inv_src_alpha = D3D12_BLEND_INV_SRC_ALPHA;
	constexpr auto blend_dst_alpha = D3D12_BLEND_DEST_ALPHA;
	constexpr auto blend_inv_dst_alpha = D3D12_BLEND_INV_DEST_ALPHA;

	constexpr auto blend_op_add = D3D12_BLEND_OP_ADD;
	constexpr auto blend_op_subtract = D3D12_BLEND_OP_SUBTRACT;
	constexpr auto blend_op_reverse_subtract = D3D12_BLEND_OP_REV_SUBTRACT;
	constexpr auto blend_op_min = D3D12_BLEND_OP_MIN;
	constexpr auto blend_op_max = D3D12_BLEND_OP_MAX;

	constexpr auto cull_none = D3D12_CULL_MODE_NONE;
	constexpr auto cull_front = D3D12_CULL_MODE_FRONT;
	constexpr auto cull_back = D3D12_CULL_MODE_BACK;

	constexpr auto fill_solid = D3D12_FILL_MODE_SOLID;
	constexpr auto fill_wireframe = D3D12_FILL_MODE_WIREFRAME;

	constexpr auto compare_never = D3D12_COMPARISON_FUNC_NEVER;
	constexpr auto compare_less = D3D12_COMPARISON_FUNC_LESS;
	constexpr auto compare_equal = D3D12_COMPARISON_FUNC_EQUAL;
	constexpr auto compare_less_equal = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	constexpr auto compare_greater = D3D12_COMPARISON_FUNC_GREATER;
	constexpr auto compare_not_equal = D3D12_COMPARISON_FUNC_NOT_EQUAL;
	constexpr auto compare_greater_equal = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
	constexpr auto compare_always = D3D12_COMPARISON_FUNC_ALWAYS;

	constexpr auto topology_type_point = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	constexpr auto topology_type_line = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	constexpr auto topology_type_triangle = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	constexpr auto topology_point_list = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	constexpr auto topology_line_list = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	constexpr auto topology_triangle_list = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	constexpr auto clear_flag_depth = D3D12_CLEAR_FLAG_DEPTH;

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

		com_ptr(
			const com_ptr&
		) = delete;

		auto operator=(
			const com_ptr&
		) -> com_ptr& = delete;

		~com_ptr();

		[[nodiscard]] auto get() const -> T*;

		auto operator->() const -> T*;

		explicit operator bool() const;

		auto reset() -> void;

		[[nodiscard]] auto put() -> T**;

	private:
		T* m_ptr = nullptr;
	};

	auto enable_debug_layer(bool gpu_based_validation) -> bool;

	auto set_resource_name(
		ID3D12Resource* resource,
		const char* name,
		std::size_t length
	) -> void;

	auto set_object_name(
		ID3D12Object* object,
		const char* name,
		std::size_t length
	) -> void;

	auto disable_debug_break(
		ID3D12Device* device
	) -> void;

	auto drain_debug_messages(
		ID3D12Device* device,
		void (*sink)(void* context, const char* message),
		void* context
	) -> void;

	auto enable_dred() -> bool;

	auto dump_dred(
		ID3D12Device* device,
		void (*sink)(void* context, const char* message),
		void (*op_sink)(void* context, unsigned int index, unsigned int completed, D3D12_AUTO_BREADCRUMB_OP op),
		void* context
	) -> void;

	[[nodiscard]] auto create_factory() -> com_ptr<IDXGIFactory4>;

	[[nodiscard]] auto create_device(
		IDXGIFactory4* factory,
		long* out_hr = nullptr,
		long* out_nvidia_hr = nullptr
	) -> com_ptr<ID3D12Device>;

	[[nodiscard]] auto create_direct_queue(
		ID3D12Device* device
	) -> com_ptr<ID3D12CommandQueue>;

	[[nodiscard]] auto create_compute_queue(
		ID3D12Device* device
	) -> com_ptr<ID3D12CommandQueue>;

	[[nodiscard]] auto create_fence(
		ID3D12Device* device,
		std::uint64_t initial_value
	) -> com_ptr<ID3D12Fence>;

	[[nodiscard]] auto create_command_allocator(
		ID3D12Device* device,
		bool compute = false
	) -> com_ptr<ID3D12CommandAllocator>;

	[[nodiscard]] auto create_command_list(
		ID3D12Device* device,
		ID3D12CommandAllocator* allocator,
		bool compute = false
	) -> com_ptr<ID3D12GraphicsCommandList>;

	[[nodiscard]] auto is_compute_command_list(
		ID3D12GraphicsCommandList* list
	) -> bool;

	[[nodiscard]] auto strip_graphics_only_states(
		D3D12_RESOURCE_STATES state
	) -> D3D12_RESOURCE_STATES;

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

	auto upload_texture(
		ID3D12Device* device,
		ID3D12CommandQueue* queue,
		ID3D12Fence* fence,
		void* wait_event,
		ID3D12Resource* texture,
		const void* const* layer_pointers,
		std::uint32_t layer_count
	) -> void;

	struct blas_triangles {
		DXGI_FORMAT vertex_format;
		std::uint64_t vertex_address;
		std::uint64_t vertex_stride;
		std::uint32_t vertex_count;
		DXGI_FORMAT index_format;
		std::uint64_t index_address;
		std::uint32_t prim_count;
	};

	[[nodiscard]] auto create_default_buffer(
		ID3D12Device* device,
		std::uint64_t size,
		D3D12_RESOURCE_STATES initial_state
	) -> com_ptr<ID3D12Resource>;

	auto blas_prebuild_info(
		ID3D12Device* device,
		const blas_triangles& triangles,
		std::uint64_t* out_acceleration_structure_size,
		std::uint64_t* out_scratch_size
	) -> void;

	auto tlas_prebuild_info(
		ID3D12Device* device,
		std::uint32_t max_instances,
		std::uint64_t* out_acceleration_structure_size,
		std::uint64_t* out_build_scratch_size,
		std::uint64_t* out_update_scratch_size
	) -> void;

	auto build_blas(
		ID3D12GraphicsCommandList* list,
		std::uint64_t dst_address,
		std::uint64_t scratch_address,
		const blas_triangles& triangles
	) -> void;

	auto build_tlas(
		ID3D12GraphicsCommandList* list,
		std::uint64_t dst_address,
		std::uint64_t scratch_address,
		std::uint64_t instance_address,
		std::uint32_t instance_count,
		bool allow_update
	) -> void;

	auto create_acceleration_structure_srv(
		ID3D12Device* device,
		std::uint64_t acceleration_structure_address,
		D3D12_CPU_DESCRIPTOR_HANDLE handle
	) -> void;

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

	auto create_texture_srv_3d(
		ID3D12Device* device,
		ID3D12Resource* resource,
		DXGI_FORMAT format,
		D3D12_CPU_DESCRIPTOR_HANDLE handle
	) -> void;

	auto create_texture_uav_3d(
		ID3D12Device* device,
		ID3D12Resource* resource,
		DXGI_FORMAT format,
		std::uint32_t depth,
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

	[[nodiscard]] auto gpu_upload_heap_supported(
		ID3D12Device* device
	) -> bool;

	[[nodiscard]] auto mesh_shader_tier(
		ID3D12Device* device
	) -> std::uint32_t;

	[[nodiscard]] auto adapter_vendor_id(
		IDXGIFactory4* factory,
		ID3D12Device* device
	) -> std::uint32_t;

	[[nodiscard]] auto create_timestamp_query_heap(
		ID3D12Device* device,
		std::uint32_t count
	) -> com_ptr<ID3D12QueryHeap>;

	[[nodiscard]] auto create_readback_buffer(
		ID3D12Device* device,
		std::uint64_t size
	) -> com_ptr<ID3D12Resource>;

	auto resolve_timestamp_query(
		ID3D12GraphicsCommandList* list,
		ID3D12QueryHeap* heap,
		ID3D12Resource* readback,
		std::uint32_t index
	) -> void;

	[[nodiscard]] auto timestamp_frequency(
		ID3D12CommandQueue* queue
	) -> std::uint64_t;

	auto create_raw_buffer_uav(
		ID3D12Device* device,
		ID3D12Resource* resource,
		std::uint32_t first_element,
		std::uint32_t num_elements,
		D3D12_CPU_DESCRIPTOR_HANDLE handle
	) -> void;

	auto create_structured_buffer_uav(
		ID3D12Device* device,
		ID3D12Resource* resource,
		std::uint32_t first_element,
		std::uint32_t num_elements,
		std::uint32_t stride,
		D3D12_CPU_DESCRIPTOR_HANDLE handle
	) -> void;

	auto create_structured_buffer_srv(
		ID3D12Device* device,
		ID3D12Resource* resource,
		std::uint32_t first_element,
		std::uint32_t num_elements,
		std::uint32_t stride,
		D3D12_CPU_DESCRIPTOR_HANDLE handle
	) -> void;

	auto create_raw_buffer_srv(
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

	struct render_target_blend {
		bool blend_enable = false;
		D3D12_BLEND src_blend = blend_one;
		D3D12_BLEND dst_blend = blend_zero;
		D3D12_BLEND_OP blend_op = blend_op_add;
		D3D12_BLEND src_blend_alpha = blend_one;
		D3D12_BLEND dst_blend_alpha = blend_zero;
		D3D12_BLEND_OP blend_op_alpha = blend_op_add;
		std::uint8_t write_mask = 0x0f;
	};

	struct graphics_pipeline_desc {
		ID3D12RootSignature* root_signature = nullptr;
		const void* vs = nullptr;
		std::size_t vs_size = 0;
		const void* ps = nullptr;
		std::size_t ps_size = 0;
		const void* as = nullptr;
		std::size_t as_size = 0;
		const void* ms = nullptr;
		std::size_t ms_size = 0;
		D3D12_FILL_MODE fill_mode = fill_solid;
		D3D12_CULL_MODE cull_mode = cull_back;
		bool front_counter_clockwise = true;
		bool depth_clip_enable = true;
		std::int32_t depth_bias = 0;
		float depth_bias_clamp = 0.0f;
		float depth_bias_slope = 0.0f;
		bool depth_enable = true;
		bool depth_write = true;
		D3D12_COMPARISON_FUNC depth_func = compare_less;
		D3D12_PRIMITIVE_TOPOLOGY_TYPE topology_type = topology_type_triangle;
		std::uint32_t rtv_count = 0;
		DXGI_FORMAT rtv_formats[8]{};
		render_target_blend rtv_blends[8]{};
		DXGI_FORMAT dsv_format = format_unknown;
		std::uint32_t sample_count = 1;
		bool alpha_to_coverage = false;
	};

	[[nodiscard]] auto create_graphics_pipeline_state(
		ID3D12Device* device,
		const graphics_pipeline_desc& desc
	) -> com_ptr<ID3D12PipelineState>;

	[[nodiscard]] auto create_mesh_pipeline_state(
		ID3D12Device* device,
		const graphics_pipeline_desc& desc,
		long* out_hr = nullptr
	) -> com_ptr<ID3D12PipelineState>;

	auto dispatch_mesh(
		ID3D12GraphicsCommandList* list,
		std::uint32_t group_count_x,
		std::uint32_t group_count_y,
		std::uint32_t group_count_z
	) -> void;

	auto set_viewport(
		ID3D12GraphicsCommandList* list,
		float x,
		float y,
		float width,
		float height,
		float min_depth,
		float max_depth
	) -> void;

	auto set_scissor(
		ID3D12GraphicsCommandList* list,
		std::int32_t left,
		std::int32_t top,
		std::int32_t right,
		std::int32_t bottom
	) -> void;

	auto begin_event(
		ID3D12GraphicsCommandList* list,
		const char* label,
		std::size_t label_size
	) -> void;

	auto end_event(
		ID3D12GraphicsCommandList* list
	) -> void;

	auto set_index_buffer(
		ID3D12GraphicsCommandList* list,
		std::uint64_t gpu_address,
		std::uint32_t size_bytes,
		bool format_32bit
	) -> void;

	[[nodiscard]] auto resource_byte_width(
		ID3D12Resource* resource
	) -> std::uint64_t;

	[[nodiscard]] auto create_draw_indexed_command_signature(
		ID3D12Device* device,
		std::uint32_t byte_stride
	) -> com_ptr<ID3D12CommandSignature>;

	[[nodiscard]] auto create_dispatch_mesh_command_signature(
		ID3D12Device* device,
		std::uint32_t byte_stride
	) -> com_ptr<ID3D12CommandSignature>;

	auto execute_indirect(
		ID3D12GraphicsCommandList* list,
		ID3D12CommandSignature* signature,
		std::uint32_t max_command_count,
		ID3D12Resource* argument_buffer,
		std::uint64_t argument_offset
	) -> void;
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

auto gse::directx::enable_debug_layer(const bool gpu_based_validation) -> bool {
	ID3D12Debug* debug = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))) && debug) {
		debug->EnableDebugLayer();
		debug->Release();
	}
	if (!gpu_based_validation) {
		return false;
	}
	ID3D12Debug1* debug1 = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug1))) && debug1) {
		debug1->SetEnableGPUBasedValidation(TRUE);
		debug1->Release();
		return true;
	}
	return false;
}

auto gse::directx::set_resource_name(ID3D12Resource* resource, const char* name, const std::size_t length) -> void {
	set_object_name(resource, name, length);
}

auto gse::directx::set_object_name(ID3D12Object* object, const char* name, const std::size_t length) -> void {
	if (!object || !name || length == 0) {
		return;
	}
	wchar_t wide[256];
	const int written = MultiByteToWideChar(CP_UTF8, 0, name, static_cast<int>(length), wide, 255);
	wide[written > 0 ? written : 0] = L'\0';
	object->SetName(wide);
}

auto gse::directx::disable_debug_break(ID3D12Device* device) -> void {
	ID3D12InfoQueue* queue = nullptr;
	if (!device || FAILED(device->QueryInterface(IID_PPV_ARGS(&queue))) || !queue) {
		return;
	}
	queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, false);
	queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, false);
	queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
	queue->Release();
}

auto gse::directx::drain_debug_messages(ID3D12Device* device, void (*sink)(void* context, const char* message), void* context) -> void {
	ID3D12InfoQueue* queue = nullptr;
	if (!device || !sink || FAILED(device->QueryInterface(IID_PPV_ARGS(&queue))) || !queue) {
		return;
	}
	const auto count = queue->GetNumStoredMessages();
	alignas(16) char storage[4096];
	for (UINT64 i = 0; i < count; ++i) {
		SIZE_T len = 0;
		if (FAILED(queue->GetMessage(i, nullptr, &len)) || len == 0 || len > sizeof(storage)) {
			continue;
		}
		auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage);
		if (SUCCEEDED(queue->GetMessage(i, message, &len)) && message->pDescription && message->Severity <= D3D12_MESSAGE_SEVERITY_WARNING) {
			sink(context, message->pDescription);
		}
	}
	queue->ClearStoredMessages();
	queue->Release();
}

auto gse::directx::enable_dred() -> bool {
	ID3D12DeviceRemovedExtendedDataSettings* settings = nullptr;
	if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&settings))) || !settings) {
		return false;
	}
	settings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
	settings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
	settings->Release();
	return true;
}

auto gse::directx::dump_dred(ID3D12Device* device, void (*sink)(void* context, const char* message), void (*op_sink)(void* context, unsigned int index, unsigned int completed, D3D12_AUTO_BREADCRUMB_OP op), void* context) -> void {
	if (!device || !sink || !op_sink) {
		return;
	}
	ID3D12DeviceRemovedExtendedData* dred = nullptr;
	if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dred))) || !dred) {
		sink(context, "DRED: extended data interface unavailable (not enabled before device creation)");
		return;
	}

	char line[1024];

	D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumbs = {};
	if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput(&breadcrumbs))) {
		const D3D12_AUTO_BREADCRUMB_NODE* node = breadcrumbs.pHeadAutoBreadcrumbNode;
		int node_index = 0;
		int incomplete_count = 0;
		while (node && node_index < 65536) {
			const UINT32 count = node->BreadcrumbCount;
			const UINT32 completed = node->pLastBreadcrumbValue ? *node->pLastBreadcrumbValue : 0;
			if (count != 0 && completed < count) {
				++incomplete_count;
				const char* queue_name = node->pCommandQueueDebugNameA ? node->pCommandQueueDebugNameA : "?";
				const char* list_name = node->pCommandListDebugNameA ? node->pCommandListDebugNameA : "?";
				std::snprintf(line, sizeof(line), "DRED node[%d] queue='%s' list='%s' completed %u/%u ops -- GPU hung in this list", node_index, queue_name, list_name, completed, count);
				sink(context, line);
				const UINT32 first = completed > 4u ? completed - 4u : 0u;
				const UINT32 last = completed + 4u < count ? completed + 4u : count;
				for (UINT32 i = first; i < last; ++i) {
					op_sink(context, i, completed, node->pCommandHistory[i]);
				}
			}
			node = node->pNext;
			++node_index;
		}
		std::snprintf(line, sizeof(line), "DRED: %d breadcrumb nodes walked, %d incomplete", node_index, incomplete_count);
		sink(context, line);
	}
	else {
		sink(context, "DRED: no auto-breadcrumb output available");
	}

	D3D12_DRED_PAGE_FAULT_OUTPUT page_fault = {};
	if (SUCCEEDED(dred->GetPageFaultAllocationOutput(&page_fault)) && page_fault.PageFaultVA != 0) {
		std::snprintf(line, sizeof(line), "DRED page fault at GPU VA 0x%llx", static_cast<unsigned long long>(page_fault.PageFaultVA));
		sink(context, line);
		for (const D3D12_DRED_ALLOCATION_NODE* a = page_fault.pHeadExistingAllocationNode; a; a = a->pNext) {
			std::snprintf(line, sizeof(line), "    existing allocation '%s' type=%d", a->ObjectNameA ? a->ObjectNameA : "?", static_cast<int>(a->AllocationType));
			sink(context, line);
		}
		for (const D3D12_DRED_ALLOCATION_NODE* a = page_fault.pHeadRecentFreedAllocationNode; a; a = a->pNext) {
			std::snprintf(line, sizeof(line), "    recently freed allocation '%s' type=%d", a->ObjectNameA ? a->ObjectNameA : "?", static_cast<int>(a->AllocationType));
			sink(context, line);
		}
	}

	dred->Release();
}

auto gse::directx::create_factory() -> com_ptr<IDXGIFactory4> {
	com_ptr<IDXGIFactory4> factory;
	CreateDXGIFactory2(0, IID_PPV_ARGS(factory.put()));
	return factory;
}

auto gse::directx::create_device(IDXGIFactory4* factory, long* out_hr, long* out_nvidia_hr) -> com_ptr<ID3D12Device> {
	wchar_t exe_path[MAX_PATH] = {};
	wchar_t saved_cwd[MAX_PATH] = {};
	const DWORD path_len = GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
	const DWORD cwd_len = GetCurrentDirectoryW(MAX_PATH, saved_cwd);
	bool cwd_changed = false;
	if (path_len > 0 && path_len < MAX_PATH) {
		DWORD i = path_len;
		while (i > 0 && exe_path[i] != L'\\' && exe_path[i] != L'/') {
			--i;
		}
		if (i > 0) {
			exe_path[i] = L'\0';
			cwd_changed = SetCurrentDirectoryW(exe_path) != 0;
		}
	}

	com_ptr<ID3D12Device> device;
	HRESULT hr = DXGI_ERROR_NOT_FOUND;

	if (factory) {
		UINT best_index = 0;
		bool have_best = false;
		bool best_is_nvidia = false;
		SIZE_T best_vram = 0;
		for (UINT i = 0;; ++i) {
			com_ptr<IDXGIAdapter1> adapter;
			if (factory->EnumAdapters1(i, adapter.put()) == DXGI_ERROR_NOT_FOUND) {
				break;
			}
			DXGI_ADAPTER_DESC1 desc = {};
			adapter->GetDesc1(&desc);
			if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
				continue;
			}
			const bool is_nvidia = desc.VendorId == 0x10de;
			const HRESULT support = D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr);
			if (is_nvidia && out_nvidia_hr) {
				*out_nvidia_hr = static_cast<long>(support);
			}
			if (FAILED(support)) {
				continue;
			}
			const bool better = !have_best || (is_nvidia && !best_is_nvidia) ||
				(is_nvidia == best_is_nvidia && desc.DedicatedVideoMemory > best_vram);
			if (better) {
				best_vram = desc.DedicatedVideoMemory;
				best_index = i;
				best_is_nvidia = is_nvidia;
				have_best = true;
			}
		}
		if (have_best) {
			com_ptr<IDXGIAdapter1> best;
			if (factory->EnumAdapters1(best_index, best.put()) != DXGI_ERROR_NOT_FOUND) {
				hr = D3D12CreateDevice(best.get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.put()));
			}
		}
	}

	if (FAILED(hr) || !device) {
		device.reset();
		hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.put()));
	}

	if (cwd_changed && cwd_len > 0 && cwd_len < MAX_PATH) {
		SetCurrentDirectoryW(saved_cwd);
	}
	if (out_hr) {
		*out_hr = static_cast<long>(hr);
	}
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

auto gse::directx::create_compute_queue(ID3D12Device* device) -> com_ptr<ID3D12CommandQueue> {
	const D3D12_COMMAND_QUEUE_DESC desc = {
		.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE,
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

auto gse::directx::create_command_allocator(ID3D12Device* device, const bool compute) -> com_ptr<ID3D12CommandAllocator> {
	const auto type = compute ? D3D12_COMMAND_LIST_TYPE_COMPUTE : D3D12_COMMAND_LIST_TYPE_DIRECT;
	com_ptr<ID3D12CommandAllocator> allocator;
	device->CreateCommandAllocator(type, IID_PPV_ARGS(allocator.put()));
	return allocator;
}

auto gse::directx::create_command_list(ID3D12Device* device, ID3D12CommandAllocator* allocator, const bool compute) -> com_ptr<ID3D12GraphicsCommandList> {
	const auto type = compute ? D3D12_COMMAND_LIST_TYPE_COMPUTE : D3D12_COMMAND_LIST_TYPE_DIRECT;
	com_ptr<ID3D12GraphicsCommandList> list;
	device->CreateCommandList(0, type, allocator, nullptr, IID_PPV_ARGS(list.put()));
	if (list) {
		list->Close();
	}
	return list;
}

auto gse::directx::is_compute_command_list(ID3D12GraphicsCommandList* list) -> bool {
	return list && list->GetType() == D3D12_COMMAND_LIST_TYPE_COMPUTE;
}

auto gse::directx::strip_graphics_only_states(const D3D12_RESOURCE_STATES state) -> D3D12_RESOURCE_STATES {
	constexpr int graphics_only = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	return static_cast<D3D12_RESOURCE_STATES>(static_cast<int>(state) & ~graphics_only);
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

auto gse::directx::upload_texture(ID3D12Device* device, ID3D12CommandQueue* queue, ID3D12Fence* fence, void* wait_event, ID3D12Resource* texture, const void* const* layer_pointers, const std::uint32_t layer_count) -> void {
	if (layer_count == 0) {
		return;
	}

	D3D12_RESOURCE_DESC desc;
	texture->GetDesc(&desc);

	std::uint64_t total_bytes = 0;
	device->GetCopyableFootprints(&desc, 0, layer_count, 0, nullptr, nullptr, nullptr, &total_bytes);

	auto upload = create_upload_buffer(device, total_bytes);
	if (!upload) {
		return;
	}
	auto* mapped = static_cast<unsigned char*>(map_buffer(upload.get()));
	if (!mapped) {
		return;
	}

	auto allocator = create_command_allocator(device);
	auto list = create_command_list(device, allocator.get());
	list->Reset(allocator.get(), nullptr);

	const D3D12_RESOURCE_BARRIER to_copy = {
		.Type = barrier_type_transition,
		.Transition = {
			.pResource = texture,
			.Subresource = resource_barrier_all_subresources,
			.StateBefore = resource_state_common,
			.StateAfter = resource_state_copy_dest,
		},
	};
	list->ResourceBarrier(1, &to_copy);

	std::uint64_t base = 0;
	for (std::uint32_t layer = 0; layer < layer_count; ++layer) {
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
		std::uint32_t row_count = 0;
		std::uint64_t row_size = 0;
		std::uint64_t layer_bytes = 0;
		device->GetCopyableFootprints(&desc, layer, 1, base, &footprint, &row_count, &row_size, &layer_bytes);

		const auto* src = static_cast<const unsigned char*>(layer_pointers[layer]);
		for (std::uint32_t row = 0; row < row_count; ++row) {
			auto* dst_row = mapped + footprint.Offset + static_cast<std::uint64_t>(row) * footprint.Footprint.RowPitch;
			const auto* src_row = src + static_cast<std::uint64_t>(row) * row_size;
			for (std::uint64_t i = 0; i < row_size; ++i) {
				dst_row[i] = src_row[i];
			}
		}

		const D3D12_TEXTURE_COPY_LOCATION destination = {
			.pResource = texture,
			.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
			.SubresourceIndex = layer,
		};
		const D3D12_TEXTURE_COPY_LOCATION source = {
			.pResource = upload.get(),
			.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
			.PlacedFootprint = footprint,
		};
		list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);

		base = (footprint.Offset + layer_bytes + 511) & ~static_cast<std::uint64_t>(511);
	}

	upload->Unmap(0, nullptr);

	const D3D12_RESOURCE_BARRIER to_common = {
		.Type = barrier_type_transition,
		.Transition = {
			.pResource = texture,
			.Subresource = resource_barrier_all_subresources,
			.StateBefore = resource_state_copy_dest,
			.StateAfter = resource_state_common,
		},
	};
	list->ResourceBarrier(1, &to_common);

	list->Close();
	ID3D12CommandList* lists[] = { list.get() };
	queue->ExecuteCommandLists(1, lists);
	const auto target = fence->GetCompletedValue() + 1;
	queue->Signal(fence, target);
	wait_fence(fence, target, wait_event);
}

auto gse::directx::create_default_buffer(ID3D12Device* device, const std::uint64_t size, const D3D12_RESOURCE_STATES initial_state) -> com_ptr<ID3D12Resource> {
	const D3D12_HEAP_PROPERTIES heap = {
		.Type = D3D12_HEAP_TYPE_DEFAULT,
	};
	const D3D12_RESOURCE_DESC desc = {
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Width = size == 0 ? 1 : size,
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_UNKNOWN,
		.SampleDesc = { .Count = 1 },
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
	};
	com_ptr<ID3D12Resource> resource;
	device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, initial_state, nullptr, IID_PPV_ARGS(resource.put()));
	return resource;
}

auto gse::directx::create_timestamp_query_heap(ID3D12Device* device, const std::uint32_t count) -> com_ptr<ID3D12QueryHeap> {
	const D3D12_QUERY_HEAP_DESC desc = {
		.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP,
		.Count = count,
		.NodeMask = 0,
	};
	com_ptr<ID3D12QueryHeap> heap;
	device->CreateQueryHeap(&desc, IID_PPV_ARGS(heap.put()));
	return heap;
}

auto gse::directx::create_readback_buffer(ID3D12Device* device, const std::uint64_t size) -> com_ptr<ID3D12Resource> {
	const D3D12_HEAP_PROPERTIES heap = {
		.Type = D3D12_HEAP_TYPE_READBACK,
	};
	const D3D12_RESOURCE_DESC desc = {
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Width = size == 0 ? 1 : size,
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_UNKNOWN,
		.SampleDesc = { .Count = 1 },
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
	};
	com_ptr<ID3D12Resource> resource;
	device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(resource.put()));
	return resource;
}

auto gse::directx::resolve_timestamp_query(ID3D12GraphicsCommandList* list, ID3D12QueryHeap* heap, ID3D12Resource* readback, const std::uint32_t index) -> void {
	list->EndQuery(heap, D3D12_QUERY_TYPE_TIMESTAMP, index);
	list->ResolveQueryData(heap, D3D12_QUERY_TYPE_TIMESTAMP, index, 1, readback, static_cast<std::uint64_t>(index) * sizeof(std::uint64_t));
}

auto gse::directx::timestamp_frequency(ID3D12CommandQueue* queue) -> std::uint64_t {
	std::uint64_t frequency = 0;
	if (FAILED(queue->GetTimestampFrequency(&frequency))) {
		return 0;
	}
	return frequency;
}

auto gse::directx::blas_prebuild_info(ID3D12Device* device, const blas_triangles& triangles, std::uint64_t* out_acceleration_structure_size, std::uint64_t* out_scratch_size) -> void {
	D3D12_RAYTRACING_GEOMETRY_DESC geometry = {};
	geometry.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geometry.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
	geometry.Triangles.IndexFormat = triangles.index_format;
	geometry.Triangles.VertexFormat = triangles.vertex_format;
	geometry.Triangles.IndexCount = triangles.prim_count * 3;
	geometry.Triangles.VertexCount = triangles.vertex_count;
	geometry.Triangles.IndexBuffer = triangles.index_address;
	geometry.Triangles.VertexBuffer.StartAddress = triangles.vertex_address;
	geometry.Triangles.VertexBuffer.StrideInBytes = triangles.vertex_stride;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
	inputs.NumDescs = 1;
	inputs.pGeometryDescs = &geometry;

	com_ptr<ID3D12Device5> device5;
	device->QueryInterface(IID_PPV_ARGS(device5.put()));
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
	if (device5) {
		device5->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);
	}
	*out_acceleration_structure_size = info.ResultDataMaxSizeInBytes;
	*out_scratch_size = info.ScratchDataSizeInBytes;
}

auto gse::directx::tlas_prebuild_info(ID3D12Device* device, const std::uint32_t max_instances, std::uint64_t* out_acceleration_structure_size, std::uint64_t* out_build_scratch_size, std::uint64_t* out_update_scratch_size) -> void {
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
	inputs.NumDescs = max_instances;

	com_ptr<ID3D12Device5> device5;
	device->QueryInterface(IID_PPV_ARGS(device5.put()));
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
	if (device5) {
		device5->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);
	}
	*out_acceleration_structure_size = info.ResultDataMaxSizeInBytes;
	*out_build_scratch_size = info.ScratchDataSizeInBytes;
	*out_update_scratch_size = info.UpdateScratchDataSizeInBytes;
}

auto gse::directx::build_blas(ID3D12GraphicsCommandList* list, const std::uint64_t dst_address, const std::uint64_t scratch_address, const blas_triangles& triangles) -> void {
	D3D12_RAYTRACING_GEOMETRY_DESC geometry = {};
	geometry.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geometry.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
	geometry.Triangles.IndexFormat = triangles.index_format;
	geometry.Triangles.VertexFormat = triangles.vertex_format;
	geometry.Triangles.IndexCount = triangles.prim_count * 3;
	geometry.Triangles.VertexCount = triangles.vertex_count;
	geometry.Triangles.IndexBuffer = triangles.index_address;
	geometry.Triangles.VertexBuffer.StartAddress = triangles.vertex_address;
	geometry.Triangles.VertexBuffer.StrideInBytes = triangles.vertex_stride;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
	desc.DestAccelerationStructureData = dst_address;
	desc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	desc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	desc.Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
	desc.Inputs.NumDescs = 1;
	desc.Inputs.pGeometryDescs = &geometry;
	desc.ScratchAccelerationStructureData = scratch_address;

	com_ptr<ID3D12GraphicsCommandList4> list4;
	list->QueryInterface(IID_PPV_ARGS(list4.put()));
	if (list4) {
		list4->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
	}
}

auto gse::directx::build_tlas(ID3D12GraphicsCommandList* list, const std::uint64_t dst_address, const std::uint64_t scratch_address, const std::uint64_t instance_address, const std::uint32_t instance_count, const bool allow_update) -> void {
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
	desc.DestAccelerationStructureData = dst_address;
	desc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	desc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	desc.Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD | (allow_update ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE);
	desc.Inputs.NumDescs = instance_count;
	desc.Inputs.InstanceDescs = instance_address;
	desc.ScratchAccelerationStructureData = scratch_address;

	com_ptr<ID3D12GraphicsCommandList4> list4;
	list->QueryInterface(IID_PPV_ARGS(list4.put()));
	if (list4) {
		list4->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
	}
}

auto gse::directx::create_acceleration_structure_srv(ID3D12Device* device, const std::uint64_t acceleration_structure_address, const D3D12_CPU_DESCRIPTOR_HANDLE handle) -> void {
	D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
	srv.Format = DXGI_FORMAT_UNKNOWN;
	srv.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv.RaytracingAccelerationStructure.Location = acceleration_structure_address;
	device->CreateShaderResourceView(nullptr, &srv, handle);
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
	D3D12_CPU_DESCRIPTOR_HANDLE handle = {};
	heap->GetCPUDescriptorHandleForHeapStart(&handle);
	return handle;
}

auto gse::directx::descriptor_heap_gpu_start(ID3D12DescriptorHeap* heap) -> D3D12_GPU_DESCRIPTOR_HANDLE {
	D3D12_GPU_DESCRIPTOR_HANDLE handle = {};
	heap->GetGPUDescriptorHandleForHeapStart(&handle);
	return handle;
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

auto gse::directx::create_texture_srv_3d(ID3D12Device* device, ID3D12Resource* resource, const DXGI_FORMAT format, const D3D12_CPU_DESCRIPTOR_HANDLE handle) -> void {
	const D3D12_SHADER_RESOURCE_VIEW_DESC desc = {
		.Format = format,
		.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Texture3D = {
			.MipLevels = 1,
		},
	};
	device->CreateShaderResourceView(resource, &desc, handle);
}

auto gse::directx::create_texture_uav_3d(ID3D12Device* device, ID3D12Resource* resource, const DXGI_FORMAT format, const std::uint32_t depth, const D3D12_CPU_DESCRIPTOR_HANDLE handle) -> void {
	const D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {
		.Format = format,
		.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D,
		.Texture3D = {
			.WSize = depth,
		},
	};
	device->CreateUnorderedAccessView(resource, nullptr, &desc, handle);
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
	const std::uint32_t push_constants = 32;
	const std::uint32_t binding_constants = num_root_constants > push_constants ? num_root_constants - push_constants : num_root_constants;
	const D3D12_ROOT_PARAMETER params[2] = {
		{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
			.Constants = {
				.ShaderRegister = 0,
				.RegisterSpace = 0,
				.Num32BitValues = binding_constants,
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
		},
		{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
			.Constants = {
				.ShaderRegister = 1,
				.RegisterSpace = 0,
				.Num32BitValues = push_constants,
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
		},
	};
	const D3D12_ROOT_SIGNATURE_DESC desc = {
		.NumParameters = 2,
		.pParameters = params,
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

auto gse::directx::gpu_upload_heap_supported(ID3D12Device* device) -> bool {
	D3D12_FEATURE_DATA_D3D12_OPTIONS16 options = {};
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS16, &options, sizeof(options)))) {
		return false;
	}
	return options.GPUUploadHeapSupported;
}

auto gse::directx::mesh_shader_tier(ID3D12Device* device) -> std::uint32_t {
	D3D12_FEATURE_DATA_D3D12_OPTIONS7 options = {};
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options, sizeof(options)))) {
		return 0;
	}
	return static_cast<std::uint32_t>(options.MeshShaderTier);
}

auto gse::directx::adapter_vendor_id(IDXGIFactory4* factory, ID3D12Device* device) -> std::uint32_t {
	if (!factory || !device) {
		return 0;
	}
	LUID luid = {};
	device->GetAdapterLuid(&luid);
	for (UINT i = 0;; ++i) {
		com_ptr<IDXGIAdapter1> adapter;
		if (factory->EnumAdapters1(i, adapter.put()) == DXGI_ERROR_NOT_FOUND) {
			break;
		}
		DXGI_ADAPTER_DESC1 desc = {};
		adapter->GetDesc1(&desc);
		if (desc.AdapterLuid.LowPart == luid.LowPart && desc.AdapterLuid.HighPart == luid.HighPart) {
			return desc.VendorId;
		}
	}
	return 0;
}

auto gse::directx::create_gpu_upload_buffer(ID3D12Device* device, const std::uint64_t size) -> com_ptr<ID3D12Resource> {
	const D3D12_HEAP_PROPERTIES heap = {
		.Type = D3D12_HEAP_TYPE_GPU_UPLOAD,
		.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
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

auto gse::directx::create_structured_buffer_uav(ID3D12Device* device, ID3D12Resource* resource, const std::uint32_t first_element, const std::uint32_t num_elements, const std::uint32_t stride, const D3D12_CPU_DESCRIPTOR_HANDLE handle) -> void {
	const D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {
		.Format = DXGI_FORMAT_UNKNOWN,
		.ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
		.Buffer = {
			.FirstElement = first_element,
			.NumElements = num_elements,
			.StructureByteStride = stride,
			.Flags = D3D12_BUFFER_UAV_FLAG_NONE,
		},
	};
	device->CreateUnorderedAccessView(resource, nullptr, &desc, handle);
}

auto gse::directx::create_structured_buffer_srv(ID3D12Device* device, ID3D12Resource* resource, const std::uint32_t first_element, const std::uint32_t num_elements, const std::uint32_t stride, const D3D12_CPU_DESCRIPTOR_HANDLE handle) -> void {
	const D3D12_SHADER_RESOURCE_VIEW_DESC desc = {
		.Format = DXGI_FORMAT_UNKNOWN,
		.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Buffer = {
			.FirstElement = first_element,
			.NumElements = num_elements,
			.StructureByteStride = stride,
			.Flags = D3D12_BUFFER_SRV_FLAG_NONE,
		},
	};
	device->CreateShaderResourceView(resource, &desc, handle);
}

auto gse::directx::create_raw_buffer_srv(ID3D12Device* device, ID3D12Resource* resource, const std::uint32_t first_element, const std::uint32_t num_elements, const D3D12_CPU_DESCRIPTOR_HANDLE handle) -> void {
	const D3D12_SHADER_RESOURCE_VIEW_DESC desc = {
		.Format = DXGI_FORMAT_R32_TYPELESS,
		.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Buffer = {
			.FirstElement = first_element,
			.NumElements = num_elements,
			.StructureByteStride = 0,
			.Flags = D3D12_BUFFER_SRV_FLAG_RAW,
		},
	};
	device->CreateShaderResourceView(resource, &desc, handle);
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

namespace gse::directx {
	auto build_blend_desc(const graphics_pipeline_desc& desc) -> D3D12_BLEND_DESC {
		D3D12_BLEND_DESC blend = {};
		blend.AlphaToCoverageEnable = desc.alpha_to_coverage;
		blend.IndependentBlendEnable = TRUE;
		for (std::size_t i = 0; i < 8; ++i) {
			const auto& b = desc.rtv_blends[i];
			blend.RenderTarget[i] = {
				.BlendEnable = b.blend_enable,
				.LogicOpEnable = FALSE,
				.SrcBlend = b.src_blend,
				.DestBlend = b.dst_blend,
				.BlendOp = b.blend_op,
				.SrcBlendAlpha = b.src_blend_alpha,
				.DestBlendAlpha = b.dst_blend_alpha,
				.BlendOpAlpha = b.blend_op_alpha,
				.LogicOp = D3D12_LOGIC_OP_NOOP,
				.RenderTargetWriteMask = b.write_mask,
			};
		}
		return blend;
	}

	auto build_rasterizer_desc(const graphics_pipeline_desc& desc) -> D3D12_RASTERIZER_DESC {
		return {
			.FillMode = desc.fill_mode,
			.CullMode = desc.cull_mode,
			.FrontCounterClockwise = desc.front_counter_clockwise,
			.DepthBias = desc.depth_bias,
			.DepthBiasClamp = desc.depth_bias_clamp,
			.SlopeScaledDepthBias = desc.depth_bias_slope,
			.DepthClipEnable = desc.depth_clip_enable,
		};
	}

	auto build_depth_stencil_desc(const graphics_pipeline_desc& desc) -> D3D12_DEPTH_STENCIL_DESC {
		return {
			.DepthEnable = desc.depth_enable,
			.DepthWriteMask = desc.depth_write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO,
			.DepthFunc = desc.depth_func,
			.StencilEnable = FALSE,
		};
	}
}

auto gse::directx::create_graphics_pipeline_state(ID3D12Device* device, const graphics_pipeline_desc& desc) -> com_ptr<ID3D12PipelineState> {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
	pso.pRootSignature = desc.root_signature;
	pso.VS = {
		.pShaderBytecode = desc.vs,
		.BytecodeLength = desc.vs_size,
	};
	pso.PS = {
		.pShaderBytecode = desc.ps,
		.BytecodeLength = desc.ps_size,
	};
	pso.BlendState = build_blend_desc(desc);
	pso.SampleMask = 0xffffffffu;
	pso.RasterizerState = build_rasterizer_desc(desc);
	pso.DepthStencilState = build_depth_stencil_desc(desc);
	pso.InputLayout = {
		.pInputElementDescs = nullptr,
		.NumElements = 0,
	};
	pso.PrimitiveTopologyType = desc.topology_type;
	pso.NumRenderTargets = desc.rtv_count;
	for (std::uint32_t i = 0; i < desc.rtv_count && i < 8; ++i) {
		pso.RTVFormats[i] = desc.rtv_formats[i];
	}
	pso.DSVFormat = desc.dsv_format;
	pso.SampleDesc = {
		.Count = desc.sample_count,
		.Quality = 0,
	};
	com_ptr<ID3D12PipelineState> out;
	device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(out.put()));
	return out;
}

auto gse::directx::create_mesh_pipeline_state(ID3D12Device* device, const graphics_pipeline_desc& desc, long* out_hr) -> com_ptr<ID3D12PipelineState> {
	com_ptr<ID3D12Device2> device2;
	const HRESULT qi_hr = device->QueryInterface(IID_PPV_ARGS(device2.put()));
	if (!device2) {
		if (out_hr) {
			*out_hr = qi_hr;
		}
		return {};
	}
	D3DX12_MESH_SHADER_PIPELINE_STATE_DESC mesh = {};
	mesh.pRootSignature = desc.root_signature;
	mesh.AS = {
		.pShaderBytecode = desc.as,
		.BytecodeLength = desc.as_size,
	};
	mesh.MS = {
		.pShaderBytecode = desc.ms,
		.BytecodeLength = desc.ms_size,
	};
	mesh.PS = {
		.pShaderBytecode = desc.ps,
		.BytecodeLength = desc.ps_size,
	};
	mesh.BlendState = build_blend_desc(desc);
	mesh.SampleMask = 0xffffffffu;
	mesh.RasterizerState = build_rasterizer_desc(desc);
	mesh.DepthStencilState = build_depth_stencil_desc(desc);
	mesh.PrimitiveTopologyType = desc.topology_type;
	mesh.NumRenderTargets = desc.rtv_count;
	for (std::uint32_t i = 0; i < desc.rtv_count && i < 8; ++i) {
		mesh.RTVFormats[i] = desc.rtv_formats[i];
	}
	mesh.DSVFormat = desc.dsv_format;
	mesh.SampleDesc = {
		.Count = desc.sample_count,
		.Quality = 0,
	};

	CD3DX12_PIPELINE_STATE_STREAM2 stream(mesh);
	const D3D12_PIPELINE_STATE_STREAM_DESC stream_desc = {
		.SizeInBytes = sizeof(stream),
		.pPipelineStateSubobjectStream = &stream,
	};
	com_ptr<ID3D12PipelineState> out;
	const HRESULT hr = device2->CreatePipelineState(&stream_desc, IID_PPV_ARGS(out.put()));
	if (out_hr) {
		*out_hr = hr;
	}
	return out;
}

auto gse::directx::dispatch_mesh(ID3D12GraphicsCommandList* list, const std::uint32_t group_count_x, const std::uint32_t group_count_y, const std::uint32_t group_count_z) -> void {
	com_ptr<ID3D12GraphicsCommandList6> list6;
	list->QueryInterface(IID_PPV_ARGS(list6.put()));
	if (list6) {
		list6->DispatchMesh(group_count_x, group_count_y, group_count_z);
	}
}

auto gse::directx::set_viewport(ID3D12GraphicsCommandList* list, const float x, const float y, const float width, const float height, const float min_depth, const float max_depth) -> void {
	const D3D12_VIEWPORT viewport = {
		.TopLeftX = x,
		.TopLeftY = y,
		.Width = width,
		.Height = height,
		.MinDepth = min_depth,
		.MaxDepth = max_depth,
	};
	list->RSSetViewports(1, &viewport);
}

auto gse::directx::set_scissor(ID3D12GraphicsCommandList* list, const std::int32_t left, const std::int32_t top, const std::int32_t right, const std::int32_t bottom) -> void {
	const D3D12_RECT rect = {
		.left = left,
		.top = top,
		.right = right,
		.bottom = bottom,
	};
	list->RSSetScissorRects(1, &rect);
}

auto gse::directx::begin_event(ID3D12GraphicsCommandList* list, const char* label, const std::size_t label_size) -> void {
	constexpr std::uint32_t pix3blob_version = 2;
	constexpr std::uint64_t pix_event_begin_event_no_args = 0x002;
	constexpr std::uint64_t pix_event_type_bit_shift = 10;
	constexpr std::uint64_t pix_string_copy_chunk_size_bit_shift = 55;
	constexpr std::uint64_t pix_string_is_ansi_bit_shift = 54;

	constexpr std::size_t header_qwords = 3;
	constexpr std::size_t max_blob_qwords = 64;
	constexpr std::size_t max_label_size = (max_blob_qwords - header_qwords - 1) * sizeof(std::uint64_t);
	const std::size_t clamped_size = label_size < max_label_size ? label_size : max_label_size;

	std::uint64_t blob[max_blob_qwords] = {};
	blob[0] = pix_event_begin_event_no_args << pix_event_type_bit_shift;
	blob[1] = 0;
	blob[2] = (static_cast<std::uint64_t>(8) << pix_string_copy_chunk_size_bit_shift) | (static_cast<std::uint64_t>(1) << pix_string_is_ansi_bit_shift);

	const std::size_t string_qwords = (clamped_size + sizeof(std::uint64_t)) / sizeof(std::uint64_t);
	std::memcpy(blob + header_qwords, label, clamped_size);

	const std::size_t total_qwords = header_qwords + string_qwords;
	list->BeginEvent(pix3blob_version, blob, static_cast<std::uint32_t>(total_qwords * sizeof(std::uint64_t)));
}

auto gse::directx::end_event(ID3D12GraphicsCommandList* list) -> void {
	list->EndEvent();
}

auto gse::directx::set_index_buffer(ID3D12GraphicsCommandList* list, const std::uint64_t gpu_address, const std::uint32_t size_bytes, const bool format_32bit) -> void {
	const D3D12_INDEX_BUFFER_VIEW view = {
		.BufferLocation = gpu_address,
		.SizeInBytes = size_bytes,
		.Format = format_32bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT,
	};
	list->IASetIndexBuffer(&view);
}

auto gse::directx::resource_byte_width(ID3D12Resource* resource) -> std::uint64_t {
	D3D12_RESOURCE_DESC desc = {};
	resource->GetDesc(&desc);
	return desc.Width;
}

auto gse::directx::create_draw_indexed_command_signature(ID3D12Device* device, const std::uint32_t byte_stride) -> com_ptr<ID3D12CommandSignature> {
	const D3D12_INDIRECT_ARGUMENT_DESC argument = {
		.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED,
	};
	const D3D12_COMMAND_SIGNATURE_DESC desc = {
		.ByteStride = byte_stride,
		.NumArgumentDescs = 1,
		.pArgumentDescs = &argument,
	};
	com_ptr<ID3D12CommandSignature> signature;
	device->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(signature.put()));
	return signature;
}

auto gse::directx::create_dispatch_mesh_command_signature(ID3D12Device* device, const std::uint32_t byte_stride) -> com_ptr<ID3D12CommandSignature> {
	const D3D12_INDIRECT_ARGUMENT_DESC argument = {
		.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH,
	};
	const D3D12_COMMAND_SIGNATURE_DESC desc = {
		.ByteStride = byte_stride,
		.NumArgumentDescs = 1,
		.pArgumentDescs = &argument,
	};
	com_ptr<ID3D12CommandSignature> signature;
	device->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(signature.put()));
	return signature;
}

auto gse::directx::execute_indirect(ID3D12GraphicsCommandList* list, ID3D12CommandSignature* signature, const std::uint32_t max_command_count, ID3D12Resource* argument_buffer, const std::uint64_t argument_offset) -> void {
	list->ExecuteIndirect(signature, max_command_count, argument_buffer, argument_offset, nullptr, 0);
}

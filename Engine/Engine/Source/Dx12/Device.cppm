export module gse.dx12:device;

import std;

import gse.gpu_backend;
import gse.core;
import gse.os;
import gse.ecs;
import gse.math;
import gse.meta;
import gse.win32;
import gse.directx;
import gse.log;

import :conversions;
import :pipeline;

namespace gse::dx12 {
	struct sync_point {
		directx::com_ptr<directx::ID3D12Fence> fence;
		std::uint64_t value = 0;
		bool is_timeline = false;
	};

	enum class queue_op_kind : std::uint8_t {
		wait,
		signal,
		execute,
		cpu_signal,
		cpu_wait,
	};

	struct queue_op_record {
		std::uint64_t seq = 0;
		queue_op_kind kind = queue_op_kind::wait;
		gpu::queue_type queue = gpu::queue_type::graphics;
		const void* fence = nullptr;
		std::uint64_t value = 0;
		std::uint32_t list_count = 0;
	};

	inline constexpr std::size_t queue_op_ring_size = 256;

	struct frame_target {
		directx::com_ptr<directx::ID3D12CommandAllocator> allocator;
		directx::com_ptr<directx::ID3D12GraphicsCommandList> list;
	};

	struct gfx_template {
		std::vector<std::byte> vs;
		std::vector<std::byte> ps;
		std::vector<std::byte> mesh;
		std::vector<std::byte> task;
		gpu::dynamic_pipeline_state state;
		bool is_mesh = false;
		std::uint32_t push_size = 0;
	};

	struct graphics_pass_state {
		std::uint32_t rtv_count = 0;
		std::array<directx::DXGI_FORMAT, 8> rtv_formats{};
		directx::DXGI_FORMAT dsv_format = directx::format_unknown;
		const gfx_template* pending = nullptr;
		std::uint32_t push_size = 0;
		bool compute_pso_bound = false;
		directx::ID3D12PipelineState* resolved_pso = nullptr;
	};

	struct graphics_pso_entry {
		const gfx_template* tmpl = nullptr;
		std::uint32_t rtv_count = 0;
		std::array<directx::DXGI_FORMAT, 8> rtv_formats{};
		directx::DXGI_FORMAT dsv_format = directx::format_unknown;
		directx::com_ptr<directx::ID3D12PipelineState> pso;
	};

	struct timestamp_query_pool {
		directx::com_ptr<directx::ID3D12QueryHeap> heap;
		directx::com_ptr<directx::ID3D12Resource> readback;
		std::uint32_t capacity = 0;
	};

	struct bindless_layout {
		gpu::device_size image_range_offset = 0;
		gpu::device_size image_stride = 0;
		gpu::device_size texture_image_offset = 0;
		gpu::device_size buffer_range_offset = 0;
		gpu::device_size buffer_stride = 0;
		gpu::device_size texture_sampler_offset = 0;
		gpu::device_size sampler_range_offset = 0;
		gpu::device_size sampler_stride = 0;
	};

	[[nodiscard]] auto build_graphics_pipeline_desc(
		const gfx_template& tmpl,
		const graphics_pass_state& pass,
		directx::ID3D12RootSignature* root_signature
	) -> directx::graphics_pipeline_desc;
}

export namespace gse::dx12 {
	class device final : public non_copyable {
	public:
		device(
			shared_view<window::data> win,
			bool enable_validation,
			gpu::device_settings& cfg
		);

		[[nodiscard]] auto handle() const -> gpu::device_handle;

		[[nodiscard]] auto queue_family(
			gpu::queue_type queue_type
		) const -> std::uint32_t;

		auto wait_idle() const -> void;

		[[nodiscard]] auto timestamp_period() const -> float;

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

		auto cmd_transition_acceleration_structure_inputs(
			gpu::command_buffer_handle cmd,
			std::span<const gpu::device_address> addresses
		) -> void;

		auto cmd_write_timestamp(
			gpu::command_buffer_handle cmd,
			gpu::handle<gpu::query_pool> pool,
			std::uint32_t index
		) -> void;

		auto cmd_release_swapchain_to_present(
			gpu::command_buffer_handle cmd,
			gpu::handle<gpu::image> img,
			gpu::pipeline_stage_flags src_stages,
			gpu::access_flags src_access
		) -> void;

		auto begin_debug_event(
			gpu::command_buffer_handle cmd,
			std::string_view label
		) -> void;

		auto end_debug_event(
			gpu::command_buffer_handle cmd
		) -> void;

		auto cmd_begin_rendering(
			gpu::command_buffer_handle cmd,
			const gpu::rendering_info& info
		) -> void;

		auto cmd_bind_graphics_shaders(
			gpu::command_buffer_handle cmd,
			gpu::handle<gpu::shader_object> shader
		) -> void;

		auto note_compute_push_size(
			gpu::command_buffer_handle cmd,
			gpu::handle<gpu::shader_object> shader
		) -> void;

		[[nodiscard]] auto list_push_size(
			gpu::command_buffer_handle cmd
		) -> std::uint32_t;

		[[nodiscard]] auto compute_pso_bound(
			gpu::command_buffer_handle cmd
		) -> bool;

		auto cmd_set_viewport(
			gpu::command_buffer_handle cmd,
			const gpu::viewport& viewport
		) -> void;

		auto cmd_set_scissor(
			gpu::command_buffer_handle cmd,
			const gse::rect_t<vec2i>& scissor
		) -> void;

		auto cmd_bind_index_buffer(
			gpu::command_buffer_handle cmd,
			gpu::handle<gpu::buffer> buffer,
			gpu::device_size offset,
			gpu::device_size size,
			gpu::index_type type
		) -> void;

		auto cmd_draw(
			gpu::command_buffer_handle cmd,
			std::uint32_t vertex_count,
			std::uint32_t instance_count,
			std::uint32_t first_vertex,
			std::uint32_t first_instance
		) -> void;

		auto cmd_draw_indexed(
			gpu::command_buffer_handle cmd,
			std::uint32_t index_count,
			std::uint32_t instance_count,
			std::uint32_t first_index,
			std::int32_t vertex_offset,
			std::uint32_t first_instance
		) -> void;

		auto cmd_draw_indexed_indirect(
			gpu::command_buffer_handle cmd,
			gpu::handle<gpu::buffer> buffer,
			gpu::device_size offset,
			std::uint32_t draw_count,
			std::uint32_t stride
		) -> void;

		auto cmd_draw_mesh_tasks(
			gpu::command_buffer_handle cmd,
			std::uint32_t group_count_x,
			std::uint32_t group_count_y,
			std::uint32_t group_count_z
		) -> void;

		auto cmd_draw_mesh_tasks_indirect(
			gpu::command_buffer_handle cmd,
			gpu::handle<gpu::buffer> buffer,
			gpu::device_size offset,
			std::uint32_t draw_count,
			std::uint32_t stride
		) -> void;

		[[nodiscard]] auto frame_command_buffer(
			gpu::queue_type queue_type,
			std::uint32_t frame_index
		) const -> gpu::command_buffer_handle;

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

		[[nodiscard]] auto create_shared_surface(
			const gpu::shared_surface_desc& desc
		) const -> std::expected<gpu::shared_surface, std::string>;

		[[nodiscard]] auto import_shared_surface(
			const gpu::shared_surface_desc& desc,
			void* handle
		) const -> std::expected<gpu::shared_surface, std::string>;

		auto destroy_shared_surface(
			const gpu::shared_surface& surface
		) const -> void;

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

		[[nodiscard]] auto create_shader_program(
			const gpu::shader_program_create_info& info
		) -> gpu::shader_program;

		[[nodiscard]] auto create_semaphore() -> gpu::handle<gpu::semaphore>;

		[[nodiscard]] auto create_timeline_semaphore(
			std::uint64_t initial_value
		) -> gpu::handle<gpu::semaphore>;

		[[nodiscard]] auto create_exportable_semaphore() -> gpu::handle<gpu::semaphore>;

		[[nodiscard]] auto export_semaphore_handle(
			gpu::handle<gpu::semaphore> semaphore
		) const -> std::expected<void*, std::string>;

		[[nodiscard]] auto import_semaphore_handle(
			void* handle
		) -> std::expected<gpu::handle<gpu::semaphore>, std::string>;

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

		[[nodiscard]] auto buffer_slot(
			gpu::handle<gpu::buffer> buffer
		) const -> gpu::bindless_slot;

		[[nodiscard]] auto buffer_address(
			gpu::handle<gpu::buffer> buffer
		) const -> gpu::device_address;

		[[nodiscard]] auto buffer_size(
			gpu::handle<gpu::buffer> buffer
		) const -> gpu::device_size;

		[[nodiscard]] auto buffer_mapped(
			gpu::handle<gpu::buffer> buffer
		) const -> std::byte*;

		[[nodiscard]] auto image_sampled_slot(
			gpu::handle<gpu::image> image
		) const -> gpu::bindless_slot;

		[[nodiscard]] auto image_storage_slot(
			gpu::handle<gpu::image> image
		) const -> gpu::bindless_slot;

		[[nodiscard]] auto image_format_of(
			gpu::handle<gpu::image> image
		) const -> gpu::image_format;

		[[nodiscard]] auto image_extent(
			gpu::handle<gpu::image> image
		) const -> vec3u;

		[[nodiscard]] auto image_view(
			gpu::handle<gpu::image> image
		) const -> gpu::handle<gpu::image_view>;

		[[nodiscard]] auto allocate_buffer_slot() -> gpu::bindless_handle;

		[[nodiscard]] auto allocate_image_slot() -> gpu::bindless_handle;

		[[nodiscard]] auto allocate_acceleration_structure_slot() -> gpu::bindless_handle;

		auto write_storage_buffer(
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

		[[nodiscard]] auto bindless_resource_heap_binding() const -> gpu::bindless_heap_binding;

		[[nodiscard]] auto bindless_sampler_heap_binding() const -> gpu::bindless_heap_binding;

		[[nodiscard]] auto create_sampler(
			const gpu::sampler_desc& desc
		) -> gpu::handle<gpu::sampler>;

		auto collect_garbage() -> void;

		[[nodiscard]] auto root_signature() const -> directx::ID3D12RootSignature*;

		[[nodiscard]] auto resource_heap() const -> directx::ID3D12DescriptorHeap*;

		[[nodiscard]] auto sampler_heap() const -> directx::ID3D12DescriptorHeap*;

		[[nodiscard]] auto raw_device() const -> directx::ID3D12Device*;

		auto reset_acquired_list(
			directx::ID3D12GraphicsCommandList* list
		) -> void;

		[[nodiscard]] auto graphics_queue() const -> directx::ID3D12CommandQueue*;

		[[nodiscard]] auto command_queue(
			gpu::queue_type queue_type
		) const -> directx::ID3D12CommandQueue*;

		[[nodiscard]] auto validation_enabled() const -> bool;

		auto dump_dred_once() -> void;

		auto record_queue_op(
			queue_op_kind kind,
			gpu::queue_type queue,
			const void* fence,
			std::uint64_t value,
			std::uint32_t list_count
		) const -> void;

		[[nodiscard]] auto idle_event() const -> void*;

		[[nodiscard]] auto factory() const -> directx::IDXGIFactory4*;

		[[nodiscard]] auto hwnd() const -> void*;

		auto register_view_format(
			std::size_t descriptor_ptr,
			directx::DXGI_FORMAT format
		) -> void;

	private:
		auto init_bindless() -> void;

		auto register_sync_point(
			const sync_point* sp
		) -> void;

		auto dump_queue_ops() -> void;

		auto write_sampler_at(
			gpu::device_size byte_offset,
			const gpu::sampler_desc& desc
		) const -> void;

		auto find_buffer(
			gpu::device_address address
		) const -> std::pair<directx::ID3D12Resource*, gpu::device_address>;

		auto graphics_state(
			directx::ID3D12GraphicsCommandList* list
		) -> graphics_pass_state&;

		auto resolve_graphics_pso(
			graphics_pass_state& pass
		) -> directx::ID3D12PipelineState*;

		auto resolve_graphics_pso_locked(
			graphics_pass_state& pass
		) -> directx::ID3D12PipelineState*;

		auto prewarm_graphics_pso(
			const gpu::shader_program_create_info& info,
			const gfx_template* tmpl
		) -> void;
		
		[[nodiscard]] auto view_format(
			std::size_t descriptor_ptr
		) const -> directx::DXGI_FORMAT;

		directx::com_ptr<directx::IDXGIFactory4> m_factory;
		directx::com_ptr<directx::ID3D12Device> m_device;
		directx::com_ptr<directx::ID3D12CommandQueue> m_graphics_queue;
		directx::com_ptr<directx::ID3D12CommandQueue> m_compute_queue;
		directx::com_ptr<directx::ID3D12Fence> m_idle_fence;
		directx::com_ptr<directx::ID3D12DescriptorHeap> m_rtv_view_heap;
		directx::com_ptr<directx::ID3D12DescriptorHeap> m_dsv_view_heap;
		std::array<std::vector<frame_target>, gpu::queue_type_count> m_frames;
		std::deque<sync_point> m_sync_points;
		mutable std::mutex m_mutex;
		mutable std::vector<directx::com_ptr<directx::ID3D12Resource>> m_owned_buffers;
		mutable std::vector<directx::com_ptr<directx::ID3D12Resource>> m_owned_images;
		std::vector<directx::com_ptr<directx::ID3D12PipelineState>> m_owned_psos;
		std::deque<gfx_template> m_gfx_templates;
		std::vector<graphics_pso_entry> m_graphics_psos;
		mutable std::map<std::size_t, directx::DXGI_FORMAT> m_view_format;
		std::unordered_map<directx::ID3D12PipelineState*, std::uint32_t> m_pso_push_size;
		std::unordered_map<directx::ID3D12Resource*, directx::D3D12_RESOURCE_STATES> m_resource_states;
		std::unordered_map<directx::ID3D12Resource*, directx::D3D12_RESOURCE_STATES> m_buffer_states;

		struct live_buffer {
			gpu::bindless_slot slot;
			gpu::device_size size = 0;
			gpu::device_address address = 0;
			std::byte* mapped = nullptr;
		};

		struct live_image {
			gpu::handle<gpu::image_view> view;
			gpu::bindless_slot storage_slot;
			gpu::bindless_slot sampled_slot;
			gpu::image_format format = gpu::image_format::undefined;
			vec3u extent;
			gpu::image_view_create_info view_info;
		};

		std::unordered_map<std::uint64_t, live_buffer> m_live_buffers;
		std::unordered_map<std::uint64_t, live_image> m_live_images;
		directx::com_ptr<directx::ID3D12CommandSignature> m_draw_indexed_signature;
		directx::com_ptr<directx::ID3D12CommandSignature> m_dispatch_mesh_signature;
		std::vector<std::unique_ptr<timestamp_query_pool>> m_query_pools;
		gpu::bindless_slot_pool m_image_pool;
		gpu::bindless_slot_pool m_buffer_pool;
		gpu::bindless_slot_pool m_texture_pool;
		gpu::bindless_slot_pool m_sampler_pool;
		directx::com_ptr<directx::ID3D12DescriptorHeap> m_resource_heap;
		directx::com_ptr<directx::ID3D12DescriptorHeap> m_sampler_heap;
		bindless_layout m_bindless_layout;
		gpu::bindless_heap_binding m_resource_binding;
		gpu::bindless_heap_binding m_sampler_binding;
		std::uint32_t m_cbv_srv_uav_size = 0;
		std::uint32_t m_sampler_size = 0;
		pipeline_layout m_pipeline_layout;
		bool m_gpu_upload_supported = false;
		bool m_validation_enabled = false;
		std::atomic<bool> m_dred_dumped{ false };
		mutable std::array<queue_op_record, queue_op_ring_size> m_queue_op_ring{};
		mutable std::uint64_t m_queue_op_seq = 0;
		mutable std::vector<const sync_point*> m_sync_point_registry;
		mutable std::mutex m_queue_op_mutex;
		mutable std::map<gpu::device_address, std::pair<directx::ID3D12Resource*, gpu::device_size>> m_buffer_by_address;
		mutable std::uint64_t m_aliased_counter = 0;
		void* m_hwnd = nullptr;
		void* m_idle_event = nullptr;
		std::uint32_t m_rtv_size = 0;
		std::uint32_t m_dsv_size = 0;
		mutable std::uint32_t m_rtv_view_next = 0;
		mutable std::uint32_t m_dsv_view_next = 0;
	};

	inline device* active_device = nullptr;
}

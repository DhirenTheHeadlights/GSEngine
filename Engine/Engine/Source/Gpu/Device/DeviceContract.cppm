export module gse.gpu:device_contract;

import std;

import :video_encoder;

import gse.gpu_backend;
import gse.ecs;
import gse.math;
import gse.meta;
import gse.os;

export namespace gse::gpu {
	struct gpu_dispatch;

	struct device_backend_contract {
		[[nodiscard]] auto handle() const -> device_handle;

		[[nodiscard]] auto queue_family(
			queue_type queue_type
		) const -> std::uint32_t;

		auto wait_idle() const -> void;

		[[nodiscard]] auto timestamp_period() const -> float;

		[[nodiscard]] auto calibrated_timestamp(
			queue_type queue
		) const -> std::optional<timestamp_calibration>;

		auto wait_for_crash_dump() -> void;

		[[nodiscard]] auto fault_enabled() const -> bool;

		[[nodiscard]] auto vendor_binary_fault_enabled() const -> bool;

		[[nodiscard]] auto query_fault_counts(
			device_fault_counts& counts
		) const -> result;

		[[nodiscard]] auto query_fault_info(
			device_fault_counts& counts,
			device_fault_info& info
		) const -> result;

		auto record_buffer_fill_u32(
			command_buffer_handle cmd,
			gpu::handle<buffer> buf,
			device_size offset,
			std::uint32_t value
		) -> void;

		auto cmd_reset(
			command_buffer_handle cmd
		) -> void;

		auto cmd_begin(
			command_buffer_handle cmd
		) -> void;

		auto cmd_end(
			command_buffer_handle cmd
		) -> void;

		auto cmd_pipeline_barrier(
			command_buffer_handle cmd,
			const dependency_info& dep
		) -> void;

		auto cmd_release_swapchain_to_present(
			command_buffer_handle cmd,
			gpu::handle<image> img,
			pipeline_stage_flags src_stages,
			access_flags src_access
		) -> void;

		auto begin_debug_event(
			command_buffer_handle cmd,
			std::string_view label
		) -> void;

		auto end_debug_event(
			command_buffer_handle cmd
		) -> void;

		[[nodiscard]] auto frame_command_buffer(
			queue_type queue_type,
			std::uint32_t frame_index
		) const -> command_buffer_handle;

		[[nodiscard]] auto submit(
			queue_type queue_type,
			const submit_info& info,
			gpu::handle<fence> signal_fence
		) -> result;

		[[nodiscard]] auto present(
			const present_info& info
		) -> result;

		[[nodiscard]] auto wait_for_fence(
			gpu::handle<fence> f,
			std::uint64_t timeout_ns
		) const -> result;

		auto reset_fence(
			gpu::handle<fence> f
		) const -> void;

		auto reset_worker_command_pools(
			std::uint32_t frame_index
		) -> void;

		[[nodiscard]] auto acquire_worker_command_buffer(
			queue_type queue_type,
			std::size_t worker_index,
			std::uint32_t frame_index
		) -> command_buffer_handle;

		[[nodiscard]] auto create_image_unbound(
			const image_create_info& info
		) const -> std::pair<gpu::handle<image>, memory_requirements>;

		[[nodiscard]] auto create_buffer_unbound(
			const buffer_desc& info
		) const -> std::pair<gpu::handle<buffer>, memory_requirements>;

		[[nodiscard]] auto create_shared_surface(
			const shared_surface_desc& desc
		) const -> std::expected<shared_surface, std::string>;

		[[nodiscard]] auto import_shared_surface(
			const shared_surface_desc& desc,
			void* handle
		) const -> std::expected<shared_surface, std::string>;

		auto destroy_shared_surface(
			const shared_surface& surface
		) const -> void;

		[[nodiscard]] auto readback_layout(
			image_format format,
			vec2u extent
		) const -> image_readback_layout;

		auto bind_image_memory(
			gpu::handle<image> img,
			device_memory mem,
			device_size offset
		) const -> void;

		auto bind_buffer_memory(
			gpu::handle<buffer> buf,
			device_memory mem,
			device_size offset
		) const -> void;

		[[nodiscard]] auto create_image_view(
			gpu::handle<image> img,
			const image_view_create_info& info
		) const -> gpu::handle<image_view>;

		[[nodiscard]] auto allocate_aliased_memory(
			device_size size,
			std::uint32_t memory_type_index
		) const -> device_memory;

		auto free_aliased_memory(
			device_memory mem
		) const -> void;

		[[nodiscard]] auto find_memory_type_index(
			std::uint32_t type_bits,
			memory_property_flags required
		) const -> std::uint32_t;

		auto host_upload_image_layers(
			gpu::handle<image> img,
			std::span<const void* const> layer_pointers,
			vec2u extent
		) const -> void;

		auto begin_one_time_commands(
			command_buffer_handle cmd
		) -> void;

		auto end_commands(
			command_buffer_handle cmd
		) -> void;

		[[nodiscard]] auto create_transient_command_pool(
			std::uint32_t family
		) -> transient_pool_handle;

		[[nodiscard]] auto allocate_transient_primary(
			transient_pool_handle pool
		) -> command_buffer_handle;

		auto transient_pool_try_reset(
			transient_pool_handle pool,
			std::uint64_t queue_progress
		) -> void;

		auto transient_pool_mark_in_use(
			transient_pool_handle pool,
			std::uint64_t value
		) -> void;

		auto transient_pool_reset_all(
			transient_pool_handle pool
		) -> void;

		[[nodiscard]] auto create_shader_program(
			const shader_program_create_info& info
		) -> shader_program;

		[[nodiscard]] auto create_semaphore() -> gpu::handle<semaphore>;

		[[nodiscard]] auto create_timeline_semaphore(
			std::uint64_t initial_value
		) -> gpu::handle<semaphore>;

		[[nodiscard]] auto create_exportable_semaphore() -> gpu::handle<semaphore>;

		[[nodiscard]] auto export_semaphore_handle(
			gpu::handle<semaphore> semaphore
		) const -> std::expected<void*, std::string>;

		[[nodiscard]] auto import_semaphore_handle(
			void* handle
		) -> std::expected<gpu::handle<semaphore>, std::string>;

		[[nodiscard]] auto create_fence(
			bool signaled
		) -> gpu::handle<fence>;

		auto retire_semaphore(
			gpu::handle<semaphore> semaphore
		) -> void;

		auto retire_fence(
			gpu::handle<fence> fence
		) -> void;

		auto retire_query_pool(
			gpu::handle<query_pool> pool
		) -> void;

		[[nodiscard]] auto semaphore_counter_value(
			gpu::handle<semaphore> semaphore
		) const -> std::uint64_t;

		auto wait_semaphore(
			gpu::handle<semaphore> semaphore,
			std::uint64_t value
		) const -> void;

		[[nodiscard]] auto wait_semaphore_for(
			gpu::handle<semaphore> semaphore,
			std::uint64_t value,
			time timeout
		) const -> bool;

		[[nodiscard]] auto create_timestamp_query_pool(
			std::uint32_t capacity,
			std::string_view label
		) -> gpu::handle<query_pool>;

		[[nodiscard]] auto create_pipeline_stats_query_pool(
			std::uint32_t capacity,
			pipeline_statistic_flags statistics,
			std::string_view label
		) -> gpu::handle<query_pool>;

		[[nodiscard]] auto query_pool_results(
			gpu::handle<query_pool> pool,
			std::uint32_t first_query,
			std::uint32_t query_count,
			std::uint64_t stride
		) const -> std::pair<query_status, std::vector<std::uint64_t>>;

		[[nodiscard]] auto create_swapchain(
			surface surface,
			vec2i framebuffer_size,
			present_mode mode,
			swap_chain_handle old_handle
		) -> expected<swap_chain_info>;

		[[nodiscard]] auto boot_surface() const -> surface;

		[[nodiscard]] auto recreate_surface(
			const window::window_surface& win,
			swap_chain_handle current_swapchain
		) -> surface;

		[[nodiscard]] auto create_surface(
			native_window_handle handle
		) -> surface;

		auto destroy_surface(
			surface surface
		) -> void;

		[[nodiscard]] auto acquire_swapchain_image(
			swap_chain_handle swapchain,
			gpu::handle<semaphore> wait_semaphore,
			std::uint64_t timeout_ns
		) const -> acquire_next_image_result;

		auto wait_swapchain_release_fences(
			swap_chain_handle swapchain
		) const -> void;

		auto reset_swapchain_release_fence(
			swap_chain_handle swapchain,
			std::uint32_t image_index
		) const -> void;

		[[nodiscard]] auto swapchain_release_fence(
			swap_chain_handle swapchain,
			std::uint32_t image_index
		) const -> gpu::handle<fence>;

		[[nodiscard]] auto swapchain_past_presentation_timing(
			swap_chain_handle swapchain
		) const -> std::vector<past_present_timing>;

		[[nodiscard]] auto create_blas(
			const acceleration_structure_geometry& geometry,
			std::uint32_t prim_count
		) -> blas;

		[[nodiscard]] auto create_tlas(
			std::uint32_t max_instances
		) -> tlas;

		[[nodiscard]] auto query_blas_build_sizes(
			const acceleration_structure_geometry& geometry,
			std::uint32_t prim_count
		) const -> acceleration_structure_build_sizes;

		[[nodiscard]] auto acceleration_structure_scratch_alignment() const -> device_size;

		[[nodiscard]] auto create_buffer(
			const buffer_desc& desc,
			std::string_view tag,
			const std::source_location& loc
		) -> buffer;

		[[nodiscard]] auto create_image(
			const image_desc& desc,
			std::string_view tag
		) -> image;

		[[nodiscard]] auto buffer_slot(
			gpu::handle<buffer> buffer
		) const -> bindless_slot;

		[[nodiscard]] auto buffer_address(
			gpu::handle<buffer> buffer
		) const -> device_address;

		[[nodiscard]] auto buffer_size(
			gpu::handle<buffer> buffer
		) const -> device_size;

		[[nodiscard]] auto buffer_mapped(
			gpu::handle<buffer> buffer
		) const -> std::byte*;

		[[nodiscard]] auto image_sampled_slot(
			gpu::handle<image> image
		) const -> bindless_slot;

		[[nodiscard]] auto image_storage_slot(
			gpu::handle<image> image
		) const -> bindless_slot;

		[[nodiscard]] auto image_format_of(
			gpu::handle<image> image
		) const -> image_format;

		[[nodiscard]] auto image_extent(
			gpu::handle<image> image
		) const -> vec3u;

		[[nodiscard]] auto image_view_of(
			gpu::handle<image> image
		) const -> gpu::handle<image_view>;

		[[nodiscard]] auto allocate_buffer_slot() -> bindless_handle;

		[[nodiscard]] auto allocate_image_slot() -> bindless_handle;

		[[nodiscard]] auto allocate_acceleration_structure_slot() -> bindless_handle;

		auto write_storage_buffer(
			bindless_slot slot,
			device_address address,
			device_size size
		) -> void;

		auto write_acceleration_structure(
			bindless_slot slot,
			device_address as_address
		) -> void;

		auto write_sampled_image(
			bindless_slot slot,
			const image& img
		) -> void;

		[[nodiscard]] auto register_sampler(
			const sampler_desc& desc
		) -> bindless_handle;

		[[nodiscard]] auto register_texture(
			const image& img,
			const sampler_desc& desc
		) -> bindless_handle;

		[[nodiscard]] auto bindless_resource_heap_binding() const -> bindless_heap_binding;

		[[nodiscard]] auto bindless_sampler_heap_binding() const -> bindless_heap_binding;

		[[nodiscard]] auto max_push_data_size() const -> std::uint32_t;

		auto collect_garbage() -> void;

		[[nodiscard]] auto make_video_encoder_backend(
			const encode_desc& desc
		) -> std::unique_ptr<video_encoder_backend>;
	};
}

namespace gse::gpu {
	consteval {
		std::meta::define_aggregate(
			^^gpu_dispatch,
			meta::dispatch_specs(meta::pointer_receiver<device_backend_contract>::self_type(), ^^device_backend_contract, meta::dispatch_no_exclude)
		);
	}
}

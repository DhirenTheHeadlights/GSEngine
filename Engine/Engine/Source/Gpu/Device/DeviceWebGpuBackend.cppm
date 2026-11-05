export module gse.gpu:device_webgpu_backend;

import std;

import :video_encoder;
import :command_dispatch;

import gse.assert;
import gse.gpu_backend;
import gse.webgpu;
import gse.os;
import gse.ecs;
import gse.math;

export namespace gse::gpu {
	struct webgpu_device_backend {
		std::unique_ptr<webgpu::device> device;

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

		[[nodiscard]] auto resize_swapchain_in_place(
			swap_chain_handle swapchain,
			vec2i framebuffer_size
		) -> bool;

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

		[[nodiscard]] auto capabilities() const -> backend_capabilities;

		auto collect_garbage() -> void;

		[[nodiscard]] auto make_video_encoder_backend(
			const encode_desc& desc
		) -> std::unique_ptr<video_encoder_backend>;
	};

	struct webgpu_backend_creation {
		std::unique_ptr<webgpu_device_backend> backend;
		const command_dispatch* commands = nullptr;
		image_format surface_format = image_format::b8g8r8a8_unorm;
	};

	[[nodiscard]] auto create_webgpu_device_backend(
		std::optional<shared_view<window::data>> win
	) -> webgpu_backend_creation;
}

namespace gse::gpu {
	auto webgpu_unsupported(
		std::string_view operation
	) -> void;
}

auto gse::gpu::webgpu_unsupported(const std::string_view operation) -> void {
	assert(false, "webgpu: {} is not implemented on this backend yet", operation);
}

auto gse::gpu::webgpu_device_backend::handle() const -> device_handle {
	return webgpu::handle_of<device_handle>(device->raw());
}

auto gse::gpu::webgpu_device_backend::queue_family(queue_type) const -> std::uint32_t {
	return 0;
}

auto gse::gpu::webgpu_device_backend::wait_idle() const -> void {
	device->wait_idle();
}

auto gse::gpu::webgpu_device_backend::timestamp_period() const -> float {
	return 1.f;
}

auto gse::gpu::webgpu_device_backend::calibrated_timestamp(queue_type) const -> std::optional<timestamp_calibration> {
	return std::nullopt;
}

auto gse::gpu::webgpu_device_backend::wait_for_crash_dump() -> void {
}

auto gse::gpu::webgpu_device_backend::fault_enabled() const -> bool {
	return false;
}

auto gse::gpu::webgpu_device_backend::vendor_binary_fault_enabled() const -> bool {
	return false;
}

auto gse::gpu::webgpu_device_backend::query_fault_counts(device_fault_counts&) const -> result {
	return result::error_unknown;
}

auto gse::gpu::webgpu_device_backend::query_fault_info(device_fault_counts&, device_fault_info&) const -> result {
	return result::error_unknown;
}

auto gse::gpu::webgpu_device_backend::record_buffer_fill_u32(command_buffer_handle, gpu::handle<buffer>, device_size, std::uint32_t) -> void {
	webgpu_unsupported("record_buffer_fill_u32");
}

auto gse::gpu::webgpu_device_backend::cmd_reset(const command_buffer_handle cmd) -> void {
	device->cmd_reset(cmd);
}

auto gse::gpu::webgpu_device_backend::cmd_begin(const command_buffer_handle cmd) -> void {
	device->cmd_begin(cmd);
}

auto gse::gpu::webgpu_device_backend::cmd_end(const command_buffer_handle cmd) -> void {
	device->cmd_end(cmd);
}

auto gse::gpu::webgpu_device_backend::cmd_pipeline_barrier(command_buffer_handle, const dependency_info&) -> void {
}

auto gse::gpu::webgpu_device_backend::cmd_release_swapchain_to_present(command_buffer_handle, gpu::handle<image>, pipeline_stage_flags, access_flags) -> void {
}

auto gse::gpu::webgpu_device_backend::begin_debug_event(const command_buffer_handle cmd, const std::string_view label) -> void {
	device->begin_debug_event(cmd, label);
}

auto gse::gpu::webgpu_device_backend::end_debug_event(const command_buffer_handle cmd) -> void {
	device->end_debug_event(cmd);
}

auto gse::gpu::webgpu_device_backend::frame_command_buffer(const queue_type queue_type, const std::uint32_t frame_index) const -> command_buffer_handle {
	return device->frame_command_buffer(queue_type, frame_index);
}

auto gse::gpu::webgpu_device_backend::submit(queue_type, const submit_info& info, const gpu::handle<fence> signal_fence) -> result {
	return device->submit(info, signal_fence);
}

auto gse::gpu::webgpu_device_backend::present(const present_info& info) -> result {
	return device->present(info);
}

auto gse::gpu::webgpu_device_backend::wait_for_fence(const gpu::handle<fence> f, const std::uint64_t timeout_ns) const -> result {
	return device->wait_for_fence(f, timeout_ns);
}

auto gse::gpu::webgpu_device_backend::reset_fence(const gpu::handle<fence> f) const -> void {
	device->reset_fence(f);
}

auto gse::gpu::webgpu_device_backend::reset_worker_command_pools(const std::uint32_t frame_index) -> void {
	device->reset_worker_command_pools(frame_index);
}

auto gse::gpu::webgpu_device_backend::acquire_worker_command_buffer(queue_type, std::size_t, const std::uint32_t frame_index) -> command_buffer_handle {
	return device->acquire_worker_command_buffer(frame_index);
}

auto gse::gpu::webgpu_device_backend::create_image_unbound(const image_create_info&) const -> std::pair<gpu::handle<image>, memory_requirements> {
	webgpu_unsupported("create_image_unbound");
	return {};
}

auto gse::gpu::webgpu_device_backend::create_buffer_unbound(const buffer_desc&) const -> std::pair<gpu::handle<buffer>, memory_requirements> {
	webgpu_unsupported("create_buffer_unbound");
	return {};
}

auto gse::gpu::webgpu_device_backend::create_shared_surface(const shared_surface_desc&) const -> std::expected<shared_surface, std::string> {
	return std::unexpected(std::string("webgpu cannot export shared surfaces"));
}

auto gse::gpu::webgpu_device_backend::import_shared_surface(const shared_surface_desc&, void*) const -> std::expected<shared_surface, std::string> {
	return std::unexpected(std::string("webgpu cannot import shared surfaces"));
}

auto gse::gpu::webgpu_device_backend::destroy_shared_surface(const shared_surface&) const -> void {
}

auto gse::gpu::webgpu_device_backend::readback_layout(const image_format format, const vec2u extent) const -> image_readback_layout {
	const device_size row_pitch = (static_cast<device_size>(extent.x()) * bytes_per_pixel(format) + 255) & ~device_size{ 255 };
	return {
		.row_pitch = row_pitch,
		.size = row_pitch * extent.y(),
	};
}

auto gse::gpu::webgpu_device_backend::bind_image_memory(gpu::handle<image>, device_memory, device_size) const -> void {
	webgpu_unsupported("bind_image_memory");
}

auto gse::gpu::webgpu_device_backend::bind_buffer_memory(gpu::handle<buffer>, device_memory, device_size) const -> void {
	webgpu_unsupported("bind_buffer_memory");
}

auto gse::gpu::webgpu_device_backend::create_image_view(const gpu::handle<image> img, const image_view_create_info& info) const -> gpu::handle<image_view> {
	return device->create_image_view(img, info);
}

auto gse::gpu::webgpu_device_backend::allocate_aliased_memory(device_size, std::uint32_t) const -> device_memory {
	webgpu_unsupported("allocate_aliased_memory");
	return {};
}

auto gse::gpu::webgpu_device_backend::free_aliased_memory(device_memory) const -> void {
	webgpu_unsupported("free_aliased_memory");
}

auto gse::gpu::webgpu_device_backend::find_memory_type_index(std::uint32_t, memory_property_flags) const -> std::uint32_t {
	webgpu_unsupported("find_memory_type_index");
	return 0;
}

auto gse::gpu::webgpu_device_backend::host_upload_image_layers(const gpu::handle<image> img, const std::span<const void* const> layer_pointers, const vec2u extent) const -> void {
	device->host_upload_image_layers(img, layer_pointers, extent);
}

auto gse::gpu::webgpu_device_backend::begin_one_time_commands(const command_buffer_handle cmd) -> void {
	device->cmd_begin(cmd);
}

auto gse::gpu::webgpu_device_backend::end_commands(const command_buffer_handle cmd) -> void {
	device->cmd_end(cmd);
}

auto gse::gpu::webgpu_device_backend::create_transient_command_pool(std::uint32_t) -> transient_pool_handle {
	return device->create_transient_command_pool();
}

auto gse::gpu::webgpu_device_backend::allocate_transient_primary(const transient_pool_handle pool) -> command_buffer_handle {
	return device->allocate_transient_primary(pool);
}

auto gse::gpu::webgpu_device_backend::transient_pool_try_reset(const transient_pool_handle pool, const std::uint64_t queue_progress) -> void {
	device->transient_pool_try_reset(pool, queue_progress);
}

auto gse::gpu::webgpu_device_backend::transient_pool_mark_in_use(const transient_pool_handle pool, const std::uint64_t value) -> void {
	device->transient_pool_mark_in_use(pool, value);
}

auto gse::gpu::webgpu_device_backend::transient_pool_reset_all(const transient_pool_handle pool) -> void {
	device->transient_pool_reset_all(pool);
}

auto gse::gpu::webgpu_device_backend::create_shader_program(const shader_program_create_info& info) -> shader_program {
	return device->create_program(info);
}

auto gse::gpu::webgpu_device_backend::create_semaphore() -> gpu::handle<semaphore> {
	return device->create_semaphore(0);
}

auto gse::gpu::webgpu_device_backend::create_timeline_semaphore(const std::uint64_t initial_value) -> gpu::handle<semaphore> {
	return device->create_semaphore(initial_value);
}

auto gse::gpu::webgpu_device_backend::create_exportable_semaphore() -> gpu::handle<semaphore> {
	webgpu_unsupported("create_exportable_semaphore");
	return {};
}

auto gse::gpu::webgpu_device_backend::export_semaphore_handle(gpu::handle<semaphore>) const -> std::expected<void*, std::string> {
	return std::unexpected(std::string("webgpu cannot export semaphores"));
}

auto gse::gpu::webgpu_device_backend::import_semaphore_handle(void*) -> std::expected<gpu::handle<semaphore>, std::string> {
	return std::unexpected(std::string("webgpu cannot import semaphores"));
}

auto gse::gpu::webgpu_device_backend::create_fence(const bool signaled) -> gpu::handle<fence> {
	return device->create_fence(signaled);
}

auto gse::gpu::webgpu_device_backend::retire_semaphore(const gpu::handle<semaphore> semaphore) -> void {
	device->retire_semaphore(semaphore);
}

auto gse::gpu::webgpu_device_backend::retire_fence(const gpu::handle<fence> fence) -> void {
	device->retire_fence(fence);
}

auto gse::gpu::webgpu_device_backend::retire_query_pool(const gpu::handle<query_pool> pool) -> void {
	assert(!pool, "webgpu: query pools are never created on this backend");
}

auto gse::gpu::webgpu_device_backend::semaphore_counter_value(const gpu::handle<semaphore> semaphore) const -> std::uint64_t {
	return device->semaphore_counter_value(semaphore);
}

auto gse::gpu::webgpu_device_backend::wait_semaphore(const gpu::handle<semaphore> semaphore, const std::uint64_t value) const -> void {
	(void)device->wait_semaphore_for(semaphore, value, std::nullopt);
}

auto gse::gpu::webgpu_device_backend::wait_semaphore_for(const gpu::handle<semaphore> semaphore, const std::uint64_t value, const time timeout) const -> bool {
	return device->wait_semaphore_for(semaphore, value, timeout);
}

auto gse::gpu::webgpu_device_backend::create_timestamp_query_pool(std::uint32_t, std::string_view) -> gpu::handle<query_pool> {
	return {};
}

auto gse::gpu::webgpu_device_backend::create_pipeline_stats_query_pool(std::uint32_t, pipeline_statistic_flags, std::string_view) -> gpu::handle<query_pool> {
	return {};
}

auto gse::gpu::webgpu_device_backend::query_pool_results(gpu::handle<query_pool>, std::uint32_t, std::uint32_t, std::uint64_t) const -> std::pair<query_status, std::vector<std::uint64_t>> {
	return { query_status::error, {} };
}

auto gse::gpu::webgpu_device_backend::create_swapchain(const surface surface, const vec2i framebuffer_size, const present_mode mode, swap_chain_handle) -> expected<swap_chain_info> {
	return device->configure_surface(surface, framebuffer_size, mode);
}

auto gse::gpu::webgpu_device_backend::resize_swapchain_in_place(swap_chain_handle, vec2i) -> bool {
	return false;
}

auto gse::gpu::webgpu_device_backend::boot_surface() const -> surface {
	return device->boot_surface();
}

auto gse::gpu::webgpu_device_backend::recreate_surface(const window::window_surface& win, const swap_chain_handle current_swapchain) -> surface {
	device->destroy_surface(std::bit_cast<surface>(current_swapchain));
	return device->create_surface(win.handle);
}

auto gse::gpu::webgpu_device_backend::create_surface(const native_window_handle handle) -> surface {
	return device->create_surface(handle);
}

auto gse::gpu::webgpu_device_backend::destroy_surface(const surface surface) -> void {
	device->destroy_surface(surface);
}

auto gse::gpu::webgpu_device_backend::acquire_swapchain_image(const swap_chain_handle swapchain, gpu::handle<semaphore>, std::uint64_t) const -> acquire_next_image_result {
	return device->acquire_surface_image(swapchain);
}

auto gse::gpu::webgpu_device_backend::wait_swapchain_release_fences(swap_chain_handle) const -> void {
}

auto gse::gpu::webgpu_device_backend::reset_swapchain_release_fence(swap_chain_handle, std::uint32_t) const -> void {
}

auto gse::gpu::webgpu_device_backend::swapchain_release_fence(swap_chain_handle, std::uint32_t) const -> gpu::handle<fence> {
	return {};
}

auto gse::gpu::webgpu_device_backend::swapchain_past_presentation_timing(swap_chain_handle) const -> std::vector<past_present_timing> {
	return {};
}

auto gse::gpu::webgpu_device_backend::create_blas(const acceleration_structure_geometry&, std::uint32_t) -> blas {
	webgpu_unsupported("create_blas");
	return {};
}

auto gse::gpu::webgpu_device_backend::create_tlas(std::uint32_t) -> tlas {
	webgpu_unsupported("create_tlas");
	return {};
}

auto gse::gpu::webgpu_device_backend::query_blas_build_sizes(const acceleration_structure_geometry&, std::uint32_t) const -> acceleration_structure_build_sizes {
	webgpu_unsupported("query_blas_build_sizes");
	return {};
}

auto gse::gpu::webgpu_device_backend::acceleration_structure_scratch_alignment() const -> device_size {
	webgpu_unsupported("acceleration_structure_scratch_alignment");
	return 0;
}

auto gse::gpu::webgpu_device_backend::create_buffer(const buffer_desc& desc, const std::string_view tag, const std::source_location&) -> buffer {
	return device->create_buffer(desc, tag);
}

auto gse::gpu::webgpu_device_backend::create_image(const image_desc& desc, const std::string_view tag) -> image {
	return device->create_image(desc, tag);
}

auto gse::gpu::webgpu_device_backend::buffer_slot(const gpu::handle<buffer> buffer) const -> bindless_slot {
	return webgpu::record_of<webgpu::buffer_record>(buffer)->slot;
}

auto gse::gpu::webgpu_device_backend::buffer_address(const gpu::handle<buffer> buffer) const -> device_address {
	return webgpu::record_of<webgpu::buffer_record>(buffer)->address;
}

auto gse::gpu::webgpu_device_backend::buffer_size(const gpu::handle<buffer> buffer) const -> device_size {
	return webgpu::record_of<webgpu::buffer_record>(buffer)->size;
}

auto gse::gpu::webgpu_device_backend::buffer_mapped(const gpu::handle<buffer> buffer) const -> std::byte* {
	return webgpu::record_of<webgpu::buffer_record>(buffer)->shadow.get();
}

auto gse::gpu::webgpu_device_backend::image_sampled_slot(const gpu::handle<image> image) const -> bindless_slot {
	return webgpu::record_of<webgpu::texture_record>(image)->sampled_slot;
}

auto gse::gpu::webgpu_device_backend::image_storage_slot(const gpu::handle<image> image) const -> bindless_slot {
	return webgpu::record_of<webgpu::texture_record>(image)->storage_slot;
}

auto gse::gpu::webgpu_device_backend::image_format_of(const gpu::handle<image> image) const -> image_format {
	return webgpu::record_of<webgpu::texture_record>(image)->format;
}

auto gse::gpu::webgpu_device_backend::image_extent(const gpu::handle<image> image) const -> vec3u {
	return webgpu::record_of<webgpu::texture_record>(image)->extent;
}

auto gse::gpu::webgpu_device_backend::image_view_of(const gpu::handle<image> image) const -> gpu::handle<image_view> {
	return webgpu::handle_of<gpu::handle<image_view>>(webgpu::record_of<webgpu::texture_record>(image)->default_record);
}

auto gse::gpu::webgpu_device_backend::allocate_buffer_slot() -> bindless_handle {
	return device->allocate_buffer_slot();
}

auto gse::gpu::webgpu_device_backend::allocate_image_slot() -> bindless_handle {
	return device->allocate_image_slot();
}

auto gse::gpu::webgpu_device_backend::allocate_acceleration_structure_slot() -> bindless_handle {
	webgpu_unsupported("allocate_acceleration_structure_slot");
	return {};
}

auto gse::gpu::webgpu_device_backend::write_storage_buffer(const bindless_slot slot, const device_address address, const device_size size) -> void {
	device->write_storage_buffer(slot, address, size);
}

auto gse::gpu::webgpu_device_backend::write_acceleration_structure(bindless_slot, device_address) -> void {
	webgpu_unsupported("write_acceleration_structure");
}

auto gse::gpu::webgpu_device_backend::write_sampled_image(const bindless_slot slot, const image& img) -> void {
	device->write_sampled_image(slot, img);
}

auto gse::gpu::webgpu_device_backend::register_sampler(const sampler_desc& desc) -> bindless_handle {
	return device->register_sampler(desc);
}

auto gse::gpu::webgpu_device_backend::register_texture(const image& img, const sampler_desc&) -> bindless_handle {
	return device->register_texture(img);
}

auto gse::gpu::webgpu_device_backend::bindless_resource_heap_binding() const -> bindless_heap_binding {
	return {};
}

auto gse::gpu::webgpu_device_backend::bindless_sampler_heap_binding() const -> bindless_heap_binding {
	return {};
}

auto gse::gpu::webgpu_device_backend::max_push_data_size() const -> std::uint32_t {
	return 0;
}

auto gse::gpu::webgpu_device_backend::capabilities() const -> backend_capabilities {
	return {
		.ray_queries = false,
		.shader_target = true,
		.mesh_shaders = false,
		.bindless_texture_table = false,
		.shader_int64 = false,
		.untyped_atomics = false,
		.shared_surface_export = false,
	};
}

auto gse::gpu::webgpu_device_backend::collect_garbage() -> void {
	device->collect_garbage();
}

auto gse::gpu::create_webgpu_device_backend(const std::optional<shared_view<window::data>> win) -> webgpu_backend_creation {
	return {
		.backend = std::make_unique<webgpu_device_backend>(std::make_unique<webgpu::device>(win)),
		.commands = command_dispatch_for<webgpu::commands>(),
		.surface_format = image_format::b8g8r8a8_unorm,
	};
}

export module gse.gpu:device_dx12_backend;

import std;

import :video_backend;
import :command_dispatch;

import gse.gpu_backend;
import gse.dx12;
import gse.os;
import gse.ecs;
import gse.math;

export namespace gse::gpu {
	struct dx12_device_backend {
		std::unique_ptr<dx12::device> device;
		dx12::fault fault;
		dx12::command_pools pools;
		dx12::queue queue;
		dx12::swapchain swapchain;

		[[nodiscard]] auto handle() const -> device_handle;

		[[nodiscard]] auto queue_family(
			queue_type queue_type
		) const -> std::uint32_t;

		auto wait_idle() const -> void;

		[[nodiscard]] auto timestamp_period() const -> float;

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

		auto submit(
			queue_type queue_type,
			const submit_info& info,
			gpu::handle<fence> signal_fence
		) -> void;

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

		[[nodiscard]] auto semaphore_counter_value(
			gpu::handle<semaphore> semaphore
		) const -> std::uint64_t;

		auto wait_semaphore(
			gpu::handle<semaphore> semaphore,
			std::uint64_t value
		) const -> void;

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

		[[nodiscard]] auto create_sampler(
			const sampler_desc& desc
		) -> gpu::handle<sampler>;

		auto collect_garbage() -> void;

		[[nodiscard]] auto make_video_encoder_backend(
			const encode_desc& desc
		) -> std::unique_ptr<video_encoder_backend>;
	};

	struct dx12_backend_creation {
		std::unique_ptr<dx12_device_backend> backend;
		const command_dispatch* commands = nullptr;
		image_format surface_format = image_format::b8g8r8a8_unorm;
		bool video_encode_enabled = false;
	};

	[[nodiscard]] auto create_dx12_device_backend(
		std::optional<shared_view<window::data>> win,
		bool validation_layers_enabled,
		device_settings& cfg
	) -> dx12_backend_creation;
}

auto gse::gpu::dx12_device_backend::handle() const -> device_handle {
	return device->handle();
}

auto gse::gpu::dx12_device_backend::queue_family(const queue_type queue_type) const -> std::uint32_t {
	return device->queue_family(queue_type);
}

auto gse::gpu::dx12_device_backend::wait_idle() const -> void {
	device->wait_idle();
}

auto gse::gpu::dx12_device_backend::timestamp_period() const -> float {
	return device->timestamp_period();
}

auto gse::gpu::dx12_device_backend::wait_for_crash_dump() -> void {
	fault.wait_for_crash_dump();
}

auto gse::gpu::dx12_device_backend::fault_enabled() const -> bool {
	return fault.enabled();
}

auto gse::gpu::dx12_device_backend::vendor_binary_fault_enabled() const -> bool {
	return fault.vendor_binary_enabled();
}

auto gse::gpu::dx12_device_backend::query_fault_counts(device_fault_counts& counts) const -> result {
	return fault.query_counts(counts);
}

auto gse::gpu::dx12_device_backend::query_fault_info(device_fault_counts& counts, device_fault_info& info) const -> result {
	return fault.query_info(counts, info);
}

auto gse::gpu::dx12_device_backend::record_buffer_fill_u32(const command_buffer_handle cmd, const gpu::handle<buffer> buf, const device_size offset, const std::uint32_t value) -> void {
	device->record_buffer_fill_u32(cmd, buf, offset, value);
}

auto gse::gpu::dx12_device_backend::cmd_reset(const command_buffer_handle cmd) -> void {
	device->cmd_reset(cmd);
}

auto gse::gpu::dx12_device_backend::cmd_begin(const command_buffer_handle cmd) -> void {
	device->cmd_begin(cmd);
}

auto gse::gpu::dx12_device_backend::cmd_end(const command_buffer_handle cmd) -> void {
	device->cmd_end(cmd);
}

auto gse::gpu::dx12_device_backend::cmd_pipeline_barrier(const command_buffer_handle cmd, const dependency_info& dep) -> void {
	device->cmd_pipeline_barrier(cmd, dep);
}

auto gse::gpu::dx12_device_backend::cmd_release_swapchain_to_present(const command_buffer_handle cmd, const gpu::handle<image> img, const pipeline_stage_flags src_stages, const access_flags src_access) -> void {
	device->cmd_release_swapchain_to_present(cmd, img, src_stages, src_access);
}

auto gse::gpu::dx12_device_backend::begin_debug_event(const command_buffer_handle cmd, const std::string_view label) -> void {
	device->begin_debug_event(cmd, label);
}

auto gse::gpu::dx12_device_backend::end_debug_event(const command_buffer_handle cmd) -> void {
	device->end_debug_event(cmd);
}

auto gse::gpu::dx12_device_backend::frame_command_buffer(const queue_type queue_type, const std::uint32_t frame_index) const -> command_buffer_handle {
	return device->frame_command_buffer(queue_type, frame_index);
}

auto gse::gpu::dx12_device_backend::submit(const queue_type queue_type, const submit_info& info, const gpu::handle<fence> signal_fence) -> void {
	queue.submit(queue_type, info, signal_fence);
}

auto gse::gpu::dx12_device_backend::present(const present_info& info) -> result {
	return swapchain.present(info);
}

auto gse::gpu::dx12_device_backend::wait_for_fence(const gpu::handle<fence> f, std::uint64_t) const -> result {
	return queue.wait_for_fence(f);
}

auto gse::gpu::dx12_device_backend::reset_fence(const gpu::handle<fence> f) const -> void {
	queue.reset_fence(f);
}

auto gse::gpu::dx12_device_backend::reset_worker_command_pools(const std::uint32_t frame_index) -> void {
	pools.reset_worker_command_pools(frame_index);
}

auto gse::gpu::dx12_device_backend::acquire_worker_command_buffer(const queue_type queue_type, const std::size_t worker_index, const std::uint32_t frame_index) -> command_buffer_handle {
	return pools.acquire_worker_command_buffer(queue_type, worker_index, frame_index);
}

auto gse::gpu::dx12_device_backend::create_image_unbound(const image_create_info& info) const -> std::pair<gpu::handle<image>, memory_requirements> {
	return device->create_image_unbound(info);
}

auto gse::gpu::dx12_device_backend::create_buffer_unbound(const buffer_desc& info) const -> std::pair<gpu::handle<buffer>, memory_requirements> {
	return device->create_buffer_unbound(info);
}

auto gse::gpu::dx12_device_backend::create_shared_surface(const shared_surface_desc& desc) const -> std::expected<shared_surface, std::string> {
	return device->create_shared_surface(desc);
}

auto gse::gpu::dx12_device_backend::import_shared_surface(const shared_surface_desc& desc, void* handle) const -> std::expected<shared_surface, std::string> {
	return device->import_shared_surface(desc, handle);
}

auto gse::gpu::dx12_device_backend::destroy_shared_surface(const shared_surface& surface) const -> void {
	device->destroy_shared_surface(surface);
}

auto gse::gpu::dx12_device_backend::bind_image_memory(const gpu::handle<image> img, const device_memory mem, const device_size offset) const -> void {
	device->bind_image_memory(img, mem, offset);
}

auto gse::gpu::dx12_device_backend::bind_buffer_memory(const gpu::handle<buffer> buf, const device_memory mem, const device_size offset) const -> void {
	device->bind_buffer_memory(buf, mem, offset);
}

auto gse::gpu::dx12_device_backend::create_image_view(const gpu::handle<image> img, const image_view_create_info& info) const -> gpu::handle<image_view> {
	return device->create_image_view(img, info);
}

auto gse::gpu::dx12_device_backend::allocate_aliased_memory(const device_size size, const std::uint32_t memory_type_index) const -> device_memory {
	return device->allocate_aliased_memory(size, memory_type_index);
}

auto gse::gpu::dx12_device_backend::free_aliased_memory(const device_memory mem) const -> void {
	device->free_aliased_memory(mem);
}

auto gse::gpu::dx12_device_backend::find_memory_type_index(const std::uint32_t type_bits, const memory_property_flags required) const -> std::uint32_t {
	return device->find_memory_type_index(type_bits, required);
}

auto gse::gpu::dx12_device_backend::host_upload_image_layers(const gpu::handle<image> img, const std::span<const void* const> layer_pointers, const vec2u extent) const -> void {
	device->host_upload_image_layers(img, layer_pointers, extent);
}

auto gse::gpu::dx12_device_backend::begin_one_time_commands(const command_buffer_handle cmd) -> void {
	pools.begin_one_time_commands(cmd);
}

auto gse::gpu::dx12_device_backend::end_commands(const command_buffer_handle cmd) -> void {
	pools.end_commands(cmd);
}

auto gse::gpu::dx12_device_backend::create_transient_command_pool(const std::uint32_t family) -> transient_pool_handle {
	return pools.create_transient_command_pool(family == device->queue_family(queue_type::compute));
}

auto gse::gpu::dx12_device_backend::allocate_transient_primary(const transient_pool_handle pool) -> command_buffer_handle {
	return pools.allocate_transient_primary(pool);
}

auto gse::gpu::dx12_device_backend::transient_pool_try_reset(const transient_pool_handle pool, const std::uint64_t queue_progress) -> void {
	pools.transient_pool_try_reset(pool, queue_progress);
}

auto gse::gpu::dx12_device_backend::transient_pool_mark_in_use(const transient_pool_handle pool, const std::uint64_t value) -> void {
	pools.transient_pool_mark_in_use(pool, value);
}

auto gse::gpu::dx12_device_backend::transient_pool_reset_all(const transient_pool_handle pool) -> void {
	pools.transient_pool_reset_all(pool);
}

auto gse::gpu::dx12_device_backend::create_shader_program(const shader_program_create_info& info) -> shader_program {
	return device->create_shader_program(info);
}

auto gse::gpu::dx12_device_backend::create_semaphore() -> gpu::handle<semaphore> {
	return device->create_semaphore();
}

auto gse::gpu::dx12_device_backend::create_timeline_semaphore(const std::uint64_t initial_value) -> gpu::handle<semaphore> {
	return device->create_timeline_semaphore(initial_value);
}

auto gse::gpu::dx12_device_backend::create_exportable_semaphore() -> gpu::handle<semaphore> {
	return device->create_exportable_semaphore();
}

auto gse::gpu::dx12_device_backend::export_semaphore_handle(const gpu::handle<semaphore> semaphore) const -> std::expected<void*, std::string> {
	return device->export_semaphore_handle(semaphore);
}

auto gse::gpu::dx12_device_backend::import_semaphore_handle(void* handle) -> std::expected<gpu::handle<semaphore>, std::string> {
	return device->import_semaphore_handle(handle);
}

auto gse::gpu::dx12_device_backend::create_fence(const bool signaled) -> gpu::handle<fence> {
	return device->create_fence(signaled);
}

auto gse::gpu::dx12_device_backend::retire_semaphore(const gpu::handle<semaphore> semaphore) -> void {
	device->retire_semaphore(semaphore);
}

auto gse::gpu::dx12_device_backend::retire_fence(const gpu::handle<fence> fence) -> void {
	device->retire_fence(fence);
}

auto gse::gpu::dx12_device_backend::semaphore_counter_value(const gpu::handle<semaphore> semaphore) const -> std::uint64_t {
	return device->semaphore_counter_value(semaphore);
}

auto gse::gpu::dx12_device_backend::wait_semaphore(const gpu::handle<semaphore> semaphore, const std::uint64_t value) const -> void {
	device->wait_semaphore(semaphore, value);
}

auto gse::gpu::dx12_device_backend::create_timestamp_query_pool(const std::uint32_t capacity, const std::string_view label) -> gpu::handle<query_pool> {
	return device->create_timestamp_query_pool(capacity, label);
}

auto gse::gpu::dx12_device_backend::create_pipeline_stats_query_pool(const std::uint32_t capacity, const pipeline_statistic_flags statistics, const std::string_view label) -> gpu::handle<query_pool> {
	return device->create_pipeline_stats_query_pool(capacity, statistics, label);
}

auto gse::gpu::dx12_device_backend::query_pool_results(const gpu::handle<query_pool> pool, const std::uint32_t first_query, const std::uint32_t query_count, const std::uint64_t stride) const -> std::pair<query_status, std::vector<std::uint64_t>> {
	return device->query_pool_results(pool, first_query, query_count, stride);
}

auto gse::gpu::dx12_device_backend::create_swapchain(surface, const vec2i framebuffer_size, const present_mode mode, const swap_chain_handle old_handle) -> expected<swap_chain_info> {
	return swapchain.create(framebuffer_size, mode, old_handle);
}

auto gse::gpu::dx12_device_backend::boot_surface() const -> surface {
	return {};
}

auto gse::gpu::dx12_device_backend::recreate_surface(const window::window_surface&, swap_chain_handle) -> surface {
	return {};
}

auto gse::gpu::dx12_device_backend::create_surface(native_window_handle) -> surface {
	return {};
}

auto gse::gpu::dx12_device_backend::destroy_surface(surface) -> void {
}

auto gse::gpu::dx12_device_backend::acquire_swapchain_image(swap_chain_handle, const gpu::handle<semaphore> wait_semaphore, std::uint64_t) const -> acquire_next_image_result {
	return swapchain.acquire_image(wait_semaphore);
}

auto gse::gpu::dx12_device_backend::wait_swapchain_release_fences(swap_chain_handle) const -> void {
	swapchain.wait_release_fences();
}

auto gse::gpu::dx12_device_backend::reset_swapchain_release_fence(swap_chain_handle, const std::uint32_t image_index) const -> void {
	swapchain.reset_release_fence(image_index);
}

auto gse::gpu::dx12_device_backend::swapchain_release_fence(swap_chain_handle, const std::uint32_t image_index) const -> gpu::handle<fence> {
	return swapchain.release_fence(image_index);
}

auto gse::gpu::dx12_device_backend::swapchain_past_presentation_timing(swap_chain_handle) const -> std::vector<past_present_timing> {
	return swapchain.past_presentation_timing();
}

auto gse::gpu::dx12_device_backend::create_blas(const acceleration_structure_geometry& geometry, const std::uint32_t prim_count) -> blas {
	return device->create_blas(geometry, prim_count);
}

auto gse::gpu::dx12_device_backend::create_tlas(const std::uint32_t max_instances) -> tlas {
	return device->create_tlas(max_instances);
}

auto gse::gpu::dx12_device_backend::query_blas_build_sizes(const acceleration_structure_geometry& geometry, const std::uint32_t prim_count) const -> acceleration_structure_build_sizes {
	return device->query_blas_build_sizes(geometry, prim_count);
}

auto gse::gpu::dx12_device_backend::acceleration_structure_scratch_alignment() const -> device_size {
	return device->acceleration_structure_scratch_alignment();
}

auto gse::gpu::dx12_device_backend::create_buffer(const buffer_desc& desc, const std::string_view tag, const std::source_location& loc) -> buffer {
	return device->create_buffer(desc, tag, loc);
}

auto gse::gpu::dx12_device_backend::create_image(const image_desc& desc, const std::string_view tag) -> image {
	return device->create_image(desc, tag);
}

auto gse::gpu::dx12_device_backend::buffer_slot(const gpu::handle<buffer> buffer) const -> bindless_slot {
	return device->buffer_slot(buffer);
}

auto gse::gpu::dx12_device_backend::buffer_address(const gpu::handle<buffer> buffer) const -> device_address {
	return device->buffer_address(buffer);
}

auto gse::gpu::dx12_device_backend::buffer_size(const gpu::handle<buffer> buffer) const -> device_size {
	return device->buffer_size(buffer);
}

auto gse::gpu::dx12_device_backend::buffer_mapped(const gpu::handle<buffer> buffer) const -> std::byte* {
	return device->buffer_mapped(buffer);
}

auto gse::gpu::dx12_device_backend::image_sampled_slot(const gpu::handle<image> image) const -> bindless_slot {
	return device->image_sampled_slot(image);
}

auto gse::gpu::dx12_device_backend::image_storage_slot(const gpu::handle<image> image) const -> bindless_slot {
	return device->image_storage_slot(image);
}

auto gse::gpu::dx12_device_backend::image_format_of(const gpu::handle<image> image) const -> image_format {
	return device->image_format_of(image);
}

auto gse::gpu::dx12_device_backend::image_extent(const gpu::handle<image> image) const -> vec3u {
	return device->image_extent(image);
}

auto gse::gpu::dx12_device_backend::image_view_of(const gpu::handle<image> image) const -> gpu::handle<image_view> {
	return device->image_view(image);
}

auto gse::gpu::dx12_device_backend::allocate_buffer_slot() -> bindless_handle {
	return device->allocate_buffer_slot();
}

auto gse::gpu::dx12_device_backend::allocate_image_slot() -> bindless_handle {
	return device->allocate_image_slot();
}

auto gse::gpu::dx12_device_backend::allocate_acceleration_structure_slot() -> bindless_handle {
	return device->allocate_acceleration_structure_slot();
}


auto gse::gpu::dx12_device_backend::write_storage_buffer(const bindless_slot slot, const device_address address, const device_size size) -> void {
	device->write_storage_buffer(slot, address, size);
}

auto gse::gpu::dx12_device_backend::write_acceleration_structure(const bindless_slot slot, const device_address as_address) -> void {
	device->write_acceleration_structure(slot, as_address);
}

auto gse::gpu::dx12_device_backend::write_sampled_image(const bindless_slot slot, const image& img) -> void {
	device->write_sampled_image(slot, img);
}

auto gse::gpu::dx12_device_backend::register_sampler(const sampler_desc& desc) -> bindless_handle {
	return device->register_sampler(desc);
}

auto gse::gpu::dx12_device_backend::register_texture(const image& img, const sampler_desc& desc) -> bindless_handle {
	return device->register_texture(img, desc);
}

auto gse::gpu::dx12_device_backend::bindless_resource_heap_binding() const -> bindless_heap_binding {
	return device->bindless_resource_heap_binding();
}

auto gse::gpu::dx12_device_backend::bindless_sampler_heap_binding() const -> bindless_heap_binding {
	return device->bindless_sampler_heap_binding();
}

auto gse::gpu::dx12_device_backend::create_sampler(const sampler_desc& desc) -> gpu::handle<sampler> {
	return device->create_sampler(desc);
}

auto gse::gpu::dx12_device_backend::collect_garbage() -> void {
	device->collect_garbage();
}

auto gse::gpu::dx12_device_backend::make_video_encoder_backend(const encode_desc&) -> std::unique_ptr<video_encoder_backend> {
	return nullptr;
}

auto gse::gpu::create_dx12_device_backend(const std::optional<shared_view<window::data>> win, const bool validation_layers_enabled, device_settings& cfg) -> dx12_backend_creation {
	auto backend = std::make_unique<dx12_device_backend>(std::make_unique<dx12::device>(*win, validation_layers_enabled, cfg));
	backend->pools.bind(backend->device.get());
	backend->queue.bind(backend->device.get());
	backend->swapchain.bind(backend->device.get());
	return {
		.backend = std::move(backend),
		.commands = command_dispatch_for<dx12::commands>(),
		.surface_format = image_format::b8g8r8a8_unorm,
		.video_encode_enabled = false,
	};
}
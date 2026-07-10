export module gse.gpu:device_vulkan_backend;

import std;

import :video_backend;

import gse.gpu_backend;
import gse.vulkan;
import gse.os;
import gse.ecs;
import gse.concurrency;
import gse.math;

export namespace gse::gpu {
	struct vulkan_device_backend {
		vulkan::aftermath aftermath;
		vulkan::instance instance;
		vulkan::device device_config;
		vulkan::queue queue;
		vulkan::command command;
		vulkan::worker_command_pools worker_pools;

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

		auto begin_debug_event(
			gpu::command_buffer_handle cmd,
			std::string_view label
		) -> void;

		auto end_debug_event(
			gpu::command_buffer_handle cmd
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

		[[nodiscard]] auto allocate_acceleration_structure_slot() -> gpu::bindless_handle;

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

		[[nodiscard]] auto bindless_resource_heap_binding() const -> gpu::bindless_heap_binding;

		[[nodiscard]] auto bindless_sampler_heap_binding() const -> gpu::bindless_heap_binding;

		[[nodiscard]] auto create_sampler(
			const gpu::sampler_desc& desc
		) -> gpu::handle<gpu::sampler>;

		auto collect_garbage() -> void;

		[[nodiscard]] auto make_video_encoder_backend(
			vec2u extent
		) -> std::unique_ptr<gpu::video_encoder_backend>;
	};

	struct vulkan_backend_creation {
		std::unique_ptr<vulkan_device_backend> backend;
		image_format surface_format = image_format::b8g8r8a8_unorm;
		bool video_encode_enabled = false;
	};

	[[nodiscard]] auto create_vulkan_device_backend(
		std::optional<shared_view<window::data>> win,
		bool validation_layers_enabled,
		device_settings& cfg
	) -> gpu::expected<vulkan_backend_creation>;
}

auto gse::gpu::vulkan_device_backend::handle() const -> gpu::device_handle {
	return device_config.device_handle();
}

auto gse::gpu::vulkan_device_backend::queue_family(const gpu::queue_type queue_type) const -> std::uint32_t {
	return device_config.queue_family(queue_type);
}

auto gse::gpu::vulkan_device_backend::wait_idle() const -> void {
	device_config.wait_idle();
}

auto gse::gpu::vulkan_device_backend::timestamp_period() const -> float {
	return device_config.timestamp_period();
}

auto gse::gpu::vulkan_device_backend::wait_for_crash_dump() -> void {
	aftermath.wait_for_crash_dump();
}

auto gse::gpu::vulkan_device_backend::fault_enabled() const -> bool {
	return device_config.fault_enabled();
}

auto gse::gpu::vulkan_device_backend::vendor_binary_fault_enabled() const -> bool {
	return device_config.vendor_binary_fault_enabled();
}

auto gse::gpu::vulkan_device_backend::query_fault_counts(gpu::device_fault_counts& counts) const -> gpu::result {
	return device_config.query_fault_counts(counts);
}

auto gse::gpu::vulkan_device_backend::query_fault_info(gpu::device_fault_counts& counts, gpu::device_fault_info& info) const -> gpu::result {
	return device_config.query_fault_info(counts, info);
}

auto gse::gpu::vulkan_device_backend::record_buffer_fill_u32(const gpu::command_buffer_handle cmd, const gpu::handle<gpu::buffer> buf, const gpu::device_size offset, const std::uint32_t value) -> void {
	vulkan::commands(cmd).fill_buffer(buf, offset, sizeof(std::uint32_t), value);
}

auto gse::gpu::vulkan_device_backend::cmd_reset(const gpu::command_buffer_handle cmd) -> void {
	vulkan::commands(cmd).reset();
}

auto gse::gpu::vulkan_device_backend::cmd_begin(const gpu::command_buffer_handle cmd) -> void {
	vulkan::commands(cmd).begin();
}

auto gse::gpu::vulkan_device_backend::cmd_end(const gpu::command_buffer_handle cmd) -> void {
	vulkan::commands(cmd).end();
}

auto gse::gpu::vulkan_device_backend::cmd_pipeline_barrier(const gpu::command_buffer_handle cmd, const gpu::dependency_info& dep) -> void {
	vulkan::commands(cmd).pipeline_barrier(dep);
}

auto gse::gpu::vulkan_device_backend::cmd_release_swapchain_to_present(const gpu::command_buffer_handle cmd, const gpu::handle<gpu::image> img, const gpu::pipeline_stage_flags src_stages, const gpu::access_flags src_access) -> void {
	vulkan::commands(cmd).release_swapchain_image_to_present(img, src_stages, src_access);
}

auto gse::gpu::vulkan_device_backend::begin_debug_event(const gpu::command_buffer_handle cmd, const std::string_view label) -> void {
	vulkan::commands(cmd).begin_debug_label(label);
}

auto gse::gpu::vulkan_device_backend::end_debug_event(const gpu::command_buffer_handle cmd) -> void {
	vulkan::commands(cmd).end_debug_label();
}

auto gse::gpu::vulkan_device_backend::frame_command_buffer(const gpu::queue_type queue_type, const std::uint32_t frame_index) const -> gpu::command_buffer_handle {
	return command.frame_command_buffer(queue_type, frame_index);
}

auto gse::gpu::vulkan_device_backend::submit(const gpu::queue_type queue_type, const gpu::submit_info& info, const gpu::handle<gpu::fence> signal_fence) -> void {
	queue.submit(queue_type, info, signal_fence);
}

auto gse::gpu::vulkan_device_backend::present(const gpu::present_info& info) -> gpu::result {
	return queue.present(info);
}

auto gse::gpu::vulkan_device_backend::wait_for_fence(const gpu::handle<gpu::fence> f, const std::uint64_t timeout_ns) const -> gpu::result {
	return device_config.wait_for_fence(f, timeout_ns);
}

auto gse::gpu::vulkan_device_backend::reset_fence(const gpu::handle<gpu::fence> f) const -> void {
	device_config.reset_fence(f);
}

auto gse::gpu::vulkan_device_backend::reset_worker_command_pools(const std::uint32_t frame_index) -> void {
	worker_pools.reset_frame(frame_index);
}

auto gse::gpu::vulkan_device_backend::acquire_worker_command_buffer(const gpu::queue_type queue_type, const std::size_t worker_index, const std::uint32_t frame_index) -> gpu::command_buffer_handle {
	return worker_pools.acquire_command_buffer(queue_type, worker_index, frame_index);
}

auto gse::gpu::vulkan_device_backend::create_image_unbound(const gpu::image_create_info& info) const -> std::pair<gpu::handle<gpu::image>, gpu::memory_requirements> {
	return device_config.create_image_unbound(info);
}

auto gse::gpu::vulkan_device_backend::create_buffer_unbound(const gpu::buffer_desc& info) const -> std::pair<gpu::handle<gpu::buffer>, gpu::memory_requirements> {
	return device_config.create_buffer_unbound(info);
}

auto gse::gpu::vulkan_device_backend::create_shared_surface(const gpu::shared_surface_desc& desc) const -> std::expected<gpu::shared_surface, std::string> {
	return device_config.create_shared_surface(desc);
}

auto gse::gpu::vulkan_device_backend::import_shared_surface(const gpu::shared_surface_desc& desc, void* handle) const -> std::expected<gpu::shared_surface, std::string> {
	return device_config.import_shared_surface(desc, handle);
}

auto gse::gpu::vulkan_device_backend::destroy_shared_surface(const gpu::shared_surface& surface) const -> void {
	device_config.destroy_shared_surface(surface);
}

auto gse::gpu::vulkan_device_backend::bind_image_memory(const gpu::handle<gpu::image> img, const gpu::device_memory mem, const gpu::device_size offset) const -> void {
	device_config.bind_image_memory(img, mem, offset);
}

auto gse::gpu::vulkan_device_backend::bind_buffer_memory(const gpu::handle<gpu::buffer> buf, const gpu::device_memory mem, const gpu::device_size offset) const -> void {
	device_config.bind_buffer_memory(buf, mem, offset);
}

auto gse::gpu::vulkan_device_backend::create_image_view(const gpu::handle<gpu::image> img, const gpu::image_view_create_info& info) const -> gpu::handle<gpu::image_view> {
	return device_config.create_image_view(img, info);
}

auto gse::gpu::vulkan_device_backend::allocate_aliased_memory(const gpu::device_size size, const std::uint32_t memory_type_index) const -> gpu::device_memory {
	return device_config.allocate_aliased_memory(size, memory_type_index);
}

auto gse::gpu::vulkan_device_backend::free_aliased_memory(const gpu::device_memory mem) const -> void {
	device_config.free_aliased_memory(mem);
}

auto gse::gpu::vulkan_device_backend::find_memory_type_index(const std::uint32_t type_bits, const gpu::memory_property_flags required) const -> std::uint32_t {
	return device_config.find_memory_type_index(type_bits, required);
}

auto gse::gpu::vulkan_device_backend::host_upload_image_layers(const gpu::handle<gpu::image> img, const std::span<const void* const> layer_pointers, const vec2u extent) const -> void {
	device_config.host_upload_image_layers(img, layer_pointers, extent);
}

auto gse::gpu::vulkan_device_backend::begin_one_time_commands(const gpu::command_buffer_handle cmd) -> void {
	device_config.begin_one_time_commands(cmd);
}

auto gse::gpu::vulkan_device_backend::end_commands(const gpu::command_buffer_handle cmd) -> void {
	device_config.end_commands(cmd);
}

auto gse::gpu::vulkan_device_backend::create_transient_command_pool(const std::uint32_t family) -> gpu::transient_pool_handle {
	return device_config.create_transient_command_pool(family);
}

auto gse::gpu::vulkan_device_backend::allocate_transient_primary(const gpu::transient_pool_handle pool) -> gpu::command_buffer_handle {
	return device_config.allocate_transient_primary(pool);
}

auto gse::gpu::vulkan_device_backend::transient_pool_try_reset(const gpu::transient_pool_handle pool, const std::uint64_t queue_progress) -> void {
	device_config.transient_pool_try_reset(pool, queue_progress);
}

auto gse::gpu::vulkan_device_backend::transient_pool_mark_in_use(const gpu::transient_pool_handle pool, const std::uint64_t value) -> void {
	device_config.transient_pool_mark_in_use(pool, value);
}

auto gse::gpu::vulkan_device_backend::transient_pool_reset_all(const gpu::transient_pool_handle pool) -> void {
	device_config.transient_pool_reset_all(pool);
}

auto gse::gpu::vulkan_device_backend::create_shader_program(const gpu::shader_program_create_info& info) -> gpu::shader_program {
	return device_config.create_shader_program(info);
}

auto gse::gpu::vulkan_device_backend::create_semaphore() -> gpu::handle<gpu::semaphore> {
	return device_config.create_semaphore();
}

auto gse::gpu::vulkan_device_backend::create_timeline_semaphore(const std::uint64_t initial_value) -> gpu::handle<gpu::semaphore> {
	return device_config.create_timeline_semaphore(initial_value);
}

auto gse::gpu::vulkan_device_backend::create_exportable_semaphore() -> gpu::handle<gpu::semaphore> {
	return device_config.create_exportable_semaphore();
}

auto gse::gpu::vulkan_device_backend::export_semaphore_handle(const gpu::handle<gpu::semaphore> semaphore) const -> std::expected<void*, std::string> {
	return device_config.export_semaphore_handle(semaphore);
}

auto gse::gpu::vulkan_device_backend::import_semaphore_handle(void* handle) -> std::expected<gpu::handle<gpu::semaphore>, std::string> {
	return device_config.import_semaphore_handle(handle);
}

auto gse::gpu::vulkan_device_backend::create_fence(const bool signaled) -> gpu::handle<gpu::fence> {
	return device_config.create_fence(signaled);
}

auto gse::gpu::vulkan_device_backend::retire_semaphore(const gpu::handle<gpu::semaphore> semaphore) -> void {
	device_config.retire(semaphore);
}

auto gse::gpu::vulkan_device_backend::retire_fence(const gpu::handle<gpu::fence> fence) -> void {
	device_config.retire(fence);
}

auto gse::gpu::vulkan_device_backend::semaphore_counter_value(const gpu::handle<gpu::semaphore> semaphore) const -> std::uint64_t {
	return device_config.semaphore_counter_value(semaphore);
}

auto gse::gpu::vulkan_device_backend::wait_semaphore(const gpu::handle<gpu::semaphore> semaphore, const std::uint64_t value) const -> void {
	device_config.wait_semaphore(semaphore, value);
}

auto gse::gpu::vulkan_device_backend::create_timestamp_query_pool(const std::uint32_t capacity, const std::string_view label) -> gpu::handle<gpu::query_pool> {
	return device_config.create_timestamp_query_pool(capacity, label);
}

auto gse::gpu::vulkan_device_backend::create_pipeline_stats_query_pool(const std::uint32_t capacity, const gpu::pipeline_statistic_flags statistics, const std::string_view label) -> gpu::handle<gpu::query_pool> {
	return device_config.create_pipeline_stats_query_pool(capacity, statistics, label);
}

auto gse::gpu::vulkan_device_backend::query_pool_results(const gpu::handle<gpu::query_pool> pool, const std::uint32_t first_query, const std::uint32_t query_count, const std::uint64_t stride) const -> std::pair<gpu::query_status, std::vector<std::uint64_t>> {
	return device_config.query_pool_results(pool, first_query, query_count, stride);
}

auto gse::gpu::vulkan_device_backend::create_swapchain(const vec2i framebuffer_size, const gpu::present_mode mode, const gpu::swap_chain_handle old_handle) -> gpu::swap_chain_info {
	return device_config.create_swap_chain(framebuffer_size, mode, old_handle);
}

auto gse::gpu::vulkan_device_backend::acquire_swapchain_image(const gpu::swap_chain_handle swapchain, const gpu::handle<gpu::semaphore> wait_semaphore, const std::uint64_t timeout_ns) const -> gpu::acquire_next_image_result {
	return device_config.acquire_next_image(swapchain, wait_semaphore, timeout_ns);
}

auto gse::gpu::vulkan_device_backend::wait_swapchain_release_fences(const gpu::swap_chain_handle swapchain) const -> void {
	device_config.wait_swapchain_release_fences(swapchain);
}

auto gse::gpu::vulkan_device_backend::reset_swapchain_release_fence(const gpu::swap_chain_handle swapchain, const std::uint32_t image_index) const -> void {
	device_config.reset_swapchain_release_fence(swapchain, image_index);
}

auto gse::gpu::vulkan_device_backend::swapchain_release_fence(const gpu::swap_chain_handle swapchain, const std::uint32_t image_index) const -> gpu::handle<gpu::fence> {
	return device_config.swapchain_release_fence(swapchain, image_index);
}

auto gse::gpu::vulkan_device_backend::swapchain_past_presentation_timing(const gpu::swap_chain_handle swapchain) const -> std::vector<gpu::past_present_timing> {
	return device_config.swapchain_past_presentation_timing(swapchain);
}

auto gse::gpu::vulkan_device_backend::create_blas(const gpu::acceleration_structure_geometry& geometry, const std::uint32_t prim_count) -> gpu::blas {
	return device_config.create_blas(geometry, prim_count);
}

auto gse::gpu::vulkan_device_backend::create_tlas(const std::uint32_t max_instances) -> gpu::tlas {
	return device_config.create_tlas(max_instances);
}

auto gse::gpu::vulkan_device_backend::query_blas_build_sizes(const gpu::acceleration_structure_geometry& geometry, const std::uint32_t prim_count) const -> gpu::acceleration_structure_build_sizes {
	return device_config.query_blas_build_sizes(geometry, prim_count);
}

auto gse::gpu::vulkan_device_backend::acceleration_structure_scratch_alignment() const -> gpu::device_size {
	return device_config.acceleration_structure_scratch_alignment();
}

auto gse::gpu::vulkan_device_backend::create_buffer(const gpu::buffer_desc& desc, const std::string_view tag, const std::source_location& loc) -> gpu::buffer {
	return device_config.create_buffer(desc, tag, loc);
}

auto gse::gpu::vulkan_device_backend::create_image(const gpu::image_desc& desc, const std::string_view tag) -> gpu::image {
	return device_config.create_image(desc, tag);
}

auto gse::gpu::vulkan_device_backend::allocate_buffer_slot() -> gpu::bindless_handle {
	return device_config.allocate_buffer_slot();
}

auto gse::gpu::vulkan_device_backend::allocate_image_slot() -> gpu::bindless_handle {
	return device_config.allocate_image_slot();
}

auto gse::gpu::vulkan_device_backend::allocate_acceleration_structure_slot() -> gpu::bindless_handle {
	return device_config.allocate_acceleration_structure_slot();
}

auto gse::gpu::vulkan_device_backend::write_storage_buffer(const gpu::bindless_slot slot, const gpu::device_address address, const gpu::device_size size) -> void {
	device_config.write_storage_buffer(slot, address, size);
}

auto gse::gpu::vulkan_device_backend::write_uniform_buffer(const gpu::bindless_slot slot, const gpu::device_address address, const gpu::device_size size) -> void {
	device_config.write_uniform_buffer(slot, address, size);
}

auto gse::gpu::vulkan_device_backend::write_acceleration_structure(const gpu::bindless_slot slot, const gpu::device_address as_address) -> void {
	device_config.write_acceleration_structure(slot, as_address);
}

auto gse::gpu::vulkan_device_backend::write_sampled_image(const gpu::bindless_slot slot, const gpu::image& img) -> void {
	device_config.write_sampled_image(slot, img);
}

auto gse::gpu::vulkan_device_backend::register_sampler(const gpu::sampler_desc& desc) -> gpu::bindless_handle {
	return device_config.register_sampler(desc);
}

auto gse::gpu::vulkan_device_backend::register_texture(const gpu::image& img, const gpu::sampler_desc& desc) -> gpu::bindless_handle {
	return device_config.register_texture(img, desc);
}

auto gse::gpu::vulkan_device_backend::bindless_resource_heap_binding() const -> gpu::bindless_heap_binding {
	return device_config.bindless_resource_heap_binding();
}

auto gse::gpu::vulkan_device_backend::bindless_sampler_heap_binding() const -> gpu::bindless_heap_binding {
	return device_config.bindless_sampler_heap_binding();
}

auto gse::gpu::vulkan_device_backend::create_sampler(const gpu::sampler_desc& desc) -> gpu::handle<gpu::sampler> {
	return device_config.create_sampler(desc);
}

auto gse::gpu::vulkan_device_backend::collect_garbage() -> void {
	device_config.collect_garbage();
}

auto gse::gpu::vulkan_device_backend::make_video_encoder_backend(const vec2u extent) -> std::unique_ptr<gpu::video_encoder_backend> {
	const auto caps = vulkan::video_encoder::probe(device_config, queue);
	if (!caps.available) {
		return nullptr;
	}
	return std::make_unique<gpu::video_encoder_backend>(vulkan::video_encoder::create(device_config, queue, extent, caps));
}

auto gse::gpu::create_vulkan_device_backend(const std::optional<shared_view<window::data>> win, const bool validation_layers_enabled, device_settings& cfg) -> gpu::expected<vulkan_backend_creation> {
	auto aftermath_tracker = vulkan::aftermath::create({});

	auto instance = vulkan::instance::create(win ? vulkan::instance::required_window_extensions() : std::span<const char* const>{}, validation_layers_enabled);
	if (win) {
		instance.create_surface(*win);
	}

	auto created = vulkan::device::create(instance, cfg, aftermath_tracker);
	if (!created) {
		return std::unexpected{ created.error() };
	}
	auto& creation = *created;
	std::array<std::uint32_t, gpu::queue_type_count> queue_families{};
	queue_families[static_cast<std::size_t>(gpu::queue_type::graphics)] = creation.families.graphics_family.value();
	queue_families[static_cast<std::size_t>(gpu::queue_type::compute)] = creation.families.compute_family.value();

	auto command = vulkan::command::create(creation.device, queue_families);

	auto worker_pools = vulkan::worker_command_pools::create(creation.device, queue_families, task::thread_count());

	const auto surface_format = win ? vulkan::pick_surface_format(creation.device, instance) : gpu::image_format::b8g8r8a8_srgb;

	auto backend = std::make_unique<vulkan_device_backend>(
		std::move(aftermath_tracker),
		std::move(instance),
		std::move(creation.device),
		std::move(creation.queue),
		std::move(command),
		std::move(worker_pools)
	);

	backend->device_config.init_bindless();

	return vulkan_backend_creation{
		.backend = std::move(backend),
		.surface_format = surface_format,
		.video_encode_enabled = creation.video_encode_enabled,
	};
}

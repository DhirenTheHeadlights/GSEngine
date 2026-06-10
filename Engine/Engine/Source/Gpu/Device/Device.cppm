export module gse.gpu:device;

import std;

import :aliases;
import :sync_token;

import gse.vulkan;
import gse.assert;
import gse.os;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.save;
import gse.math;

export namespace gse::gpu {
	class device final : public non_copyable {
	public:
		[[nodiscard]]
		static auto create(
			const window::data& win,
			bool validation_layers_enabled,
			gpu::device_settings& device_cfg
		) -> std::unique_ptr<device>;

		~device();

		[[nodiscard]] auto handle() const -> gpu::device_handle;

		[[nodiscard]] auto surface_format() const -> image_format;

		[[nodiscard]] auto queue_family(
			gpu::queue_type queue
		) const -> std::uint32_t;

		auto wait_idle() const -> void;

		[[nodiscard]] auto timestamp_period() const -> float;

		auto report_device_lost(
			std::string_view operation
		) -> void;

		struct pass_marker {
			std::uint64_t frame_counter = 0;
			std::uint32_t pass_index = 0;
			id pass_type{};
		};

		enum class pass_marker_domain : std::uint8_t {
			graphics_queue = 0,
			compute_queue = 1,
			transient = 2,
		};

		static constexpr std::size_t pass_marker_domain_count = 3;

		struct pass_marker_handle {
			std::uint64_t seq = 0;
			pass_marker_domain domain = pass_marker_domain::graphics_queue;
		};

		auto begin_pass_marker(
			gpu::command_buffer_handle cmd,
			pass_marker_domain domain,
			pass_marker marker
		) -> pass_marker_handle;

		auto checkpoint_pass_marker(
			gpu::command_buffer_handle cmd,
			pass_marker_handle handle
		) -> void;

		auto post_renderpass_pass_marker(
			gpu::command_buffer_handle cmd,
			pass_marker_handle handle
		) -> void;

		auto end_pass_marker(
			gpu::command_buffer_handle cmd,
			pass_marker_handle handle
		) -> void;

		[[nodiscard]]
		auto create_shader_program(
			const shader_program_create_info& info
		) -> shader_program;

		[[nodiscard]]
		auto create_sync(
			std::uint32_t image_count,
			std::uint32_t frames_in_flight = max_frames_in_flight
		) -> sync;

		[[nodiscard]]
		auto create_timeline_semaphore(
			std::uint64_t initial_value
		) -> gpu::handle<gpu::semaphore>;

		auto retire(
			gpu::handle<gpu::semaphore> semaphore
		) -> void;

		[[nodiscard]]
		auto semaphore_counter_value(
			gpu::handle<gpu::semaphore> semaphore
		) const -> std::uint64_t;

		auto wait_semaphore(
			gpu::handle<gpu::semaphore> semaphore,
			std::uint64_t value
		) const -> void;

		[[nodiscard]]
		auto create_timestamp_query_pool(
			std::uint32_t capacity,
			std::string_view label = {}
		) -> gpu::handle<gpu::query_pool>;

		[[nodiscard]]
		auto create_pipeline_stats_query_pool(
			std::uint32_t capacity,
			pipeline_statistic_flags statistics,
			std::string_view label = {}
		) -> gpu::handle<gpu::query_pool>;

		[[nodiscard]]
		auto query_pool_results(
			gpu::handle<gpu::query_pool> pool,
			std::uint32_t first_query,
			std::uint32_t query_count,
			std::uint64_t stride
		) const -> std::pair<query_status, std::vector<std::uint64_t>>;

		[[nodiscard]]
		auto create_swapchain(
			vec2i framebuffer_size,
			present_mode mode,
			gpu::swap_chain_handle old_handle = {}
		) -> swap_chain_info;

		[[nodiscard]]
		auto acquire_swapchain_image(
			gpu::swap_chain_handle swapchain,
			gpu::handle<gpu::semaphore> wait_semaphore,
			std::uint64_t timeout_ns = std::numeric_limits<std::uint64_t>::max()
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

		[[nodiscard]]
		auto swapchain_past_presentation_timing(
			gpu::swap_chain_handle swapchain
		) const -> std::vector<past_present_timing>;

		[[nodiscard]]
		auto create_blas(
			const acceleration_structure_geometry& geometry,
			std::uint32_t prim_count
		) -> blas;

		[[nodiscard]]
		auto create_tlas(
			std::uint32_t max_instances
		) -> tlas;

		[[nodiscard]]
		auto query_blas_build_sizes(
			const acceleration_structure_geometry& geometry,
			std::uint32_t prim_count
		) const -> acceleration_structure_build_sizes;

		[[nodiscard]]
		auto acceleration_structure_scratch_alignment() const -> device_size;

		auto host_upload_image_layers(
			gpu::handle<gpu::image> img,
			std::span<const void* const> layer_pointers,
			vec2u extent
		) const -> void;

		[[nodiscard]]
		auto upload_image_2d(
			image& img,
			const void* pixel_data
		) -> sync_token;

		[[nodiscard]]
		auto create_buffer(
			const buffer_desc& desc,
			std::string_view tag = "",
			const std::source_location& loc = std::source_location::current()
		) -> buffer;

		[[nodiscard]]
		auto create_image(
			const image_desc& desc,
			std::string_view tag = ""
		) -> image;

		[[nodiscard]]
		auto allocate_buffer_slot() -> gpu::bindless_handle;

		[[nodiscard]]
		auto allocate_image_slot() -> gpu::bindless_handle;

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
			const image& img
		) -> void;

		[[nodiscard]]
		auto register_sampler(
			const sampler_desc& desc
		) -> gpu::bindless_handle;

		[[nodiscard]]
		auto register_texture(
			const image& img,
			const sampler_desc& desc
		) -> gpu::bindless_handle;

		[[nodiscard]]
		auto bindless_layout() const -> gpu::bindless_layout;

		[[nodiscard]]
		auto bindless_resource_heap_binding() const -> gpu::bindless_heap_binding;

		[[nodiscard]]
		auto bindless_sampler_heap_binding() const -> gpu::bindless_heap_binding;

		[[nodiscard]]
		auto create_sampler(
			const sampler_desc& desc
		) -> gpu::handle<gpu::sampler>;

		auto collect_garbage() -> void;

		[[nodiscard]]
		auto frame_command_buffer(
			queue_type queue,
			std::uint32_t frame_index
		) const -> gpu::command_buffer_handle;

		auto submit(
			queue_type queue,
			const submit_info& info,
			gpu::handle<gpu::fence> signal_fence = {}
		) -> void;

		[[nodiscard]] auto present(
			const present_info& info
		) -> result;

		[[nodiscard]]
		auto wait_for_fence(
			gpu::handle<gpu::fence> f,
			std::uint64_t timeout_ns = std::numeric_limits<std::uint64_t>::max()
		) const -> result;

		auto reset_fence(
			gpu::handle<gpu::fence> f
		) const -> void;

		auto reset_worker_command_pools(
			std::uint32_t frame_index
		) -> void;

		[[nodiscard]]
		auto acquire_worker_command_buffer(
			queue_type queue,
			std::size_t worker_index,
			std::uint32_t frame_index
		) -> gpu::command_buffer_handle;

		[[nodiscard]] auto make_video_encoder(
			vec2u extent
		) -> std::optional<video_encoder>;

		[[nodiscard]]
		auto create_image_unbound(
			const image_create_info& info
		) const -> std::pair<gpu::handle<gpu::image>, memory_requirements>;

		[[nodiscard]]
		auto create_buffer_unbound(
			const buffer_desc& info
		) const -> std::pair<gpu::handle<gpu::buffer>, memory_requirements>;

		auto bind_image_memory(
			gpu::handle<gpu::image> img,
			device_memory mem,
			device_size offset
		) const -> void;

		auto bind_buffer_memory(
			gpu::handle<gpu::buffer> buf,
			device_memory mem,
			device_size offset
		) const -> void;

		[[nodiscard]]
		auto create_image_view(
			gpu::handle<gpu::image> img,
			const image_view_create_info& info
		) const -> gpu::handle<gpu::image_view>;

		[[nodiscard]]
		auto allocate_aliased_memory(
			device_size size,
			std::uint32_t memory_type_index
		) const -> device_memory;

		auto free_aliased_memory(
			device_memory mem
		) const -> void;

		[[nodiscard]]
		auto find_memory_type_index(
			std::uint32_t type_bits,
			memory_property_flags required
		) const -> std::uint32_t;

		[[nodiscard]]
		auto make_aliased_image(
			gpu::handle<gpu::image> img_handle,
			gpu::handle<gpu::image_view> view_handle,
			image_format format,
			vec3u extent,
			const image_view_create_info& view_info,
			std::string_view tag
		) -> std::unique_ptr<image>;

		[[nodiscard]]
		auto make_aliased_buffer(
			gpu::handle<gpu::buffer> buf_handle,
			device_size size,
			std::string_view tag
		) -> std::unique_ptr<buffer>;

		[[nodiscard]] auto transient() -> transient_executor&;

		[[nodiscard]] auto video_encode_enabled() const -> bool;

	private:
		device(
			vulkan::aftermath&& aftermath_tracker,
			vulkan::instance&& instance,
			vulkan::device&& device,
			vulkan::queue&& queue,
			vulkan::command&& command,
			vulkan::worker_command_pools&& worker_pools,
			image_format surface_format,
			bool video_encode_enabled
		);

		vulkan::aftermath m_aftermath;
		vulkan::instance m_instance;
		vulkan::device m_device_config;
		vulkan::queue m_queue;
		vulkan::command m_command;
		vulkan::worker_command_pools m_worker_pools;
		std::unique_ptr<transient_executor> m_transient;
		image_format m_surface_format;
		std::atomic<bool> m_device_lost_reported = false;
		bool m_video_encode_enabled = false;

		static constexpr std::size_t pass_marker_ring_size = 128;

		struct pass_marker_ring {
			std::array<pass_marker, pass_marker_ring_size> entries{};
			std::atomic<std::uint64_t> seq{ 1 };
			buffer checkpoint_buffer;
			const std::uint32_t* checkpoint_slots = nullptr;
		};

		std::array<pass_marker_ring, pass_marker_domain_count> m_pass_marker_rings;
	};
}

auto gse::gpu::device::create_shader_program(const shader_program_create_info& info) -> shader_program {
	return m_device_config.create_shader_program(info);
}

auto gse::gpu::device::create_sync(const std::uint32_t image_count, const std::uint32_t frames_in_flight) -> sync {
	return sync::create(m_device_config, image_count, frames_in_flight);
}

auto gse::gpu::device::create_timeline_semaphore(const std::uint64_t initial_value) -> gpu::handle<gpu::semaphore> {
	return m_device_config.create_timeline_semaphore(initial_value);
}

auto gse::gpu::device::retire(const gpu::handle<gpu::semaphore> semaphore) -> void {
	m_device_config.retire(semaphore);
}

auto gse::gpu::device::semaphore_counter_value(const gpu::handle<gpu::semaphore> semaphore) const -> std::uint64_t {
	return m_device_config.semaphore_counter_value(semaphore);
}

auto gse::gpu::device::wait_semaphore(const gpu::handle<gpu::semaphore> semaphore, const std::uint64_t value) const -> void {
	m_device_config.wait_semaphore(semaphore, value);
}

auto gse::gpu::device::create_timestamp_query_pool(const std::uint32_t capacity, const std::string_view label) -> gpu::handle<gpu::query_pool> {
	return m_device_config.create_timestamp_query_pool(capacity, label);
}

auto gse::gpu::device::create_pipeline_stats_query_pool(const std::uint32_t capacity, const pipeline_statistic_flags statistics, const std::string_view label) -> gpu::handle<gpu::query_pool> {
	return m_device_config.create_pipeline_stats_query_pool(capacity, statistics, label);
}

auto gse::gpu::device::query_pool_results(const gpu::handle<gpu::query_pool> pool, const std::uint32_t first_query, const std::uint32_t query_count, const std::uint64_t stride) const -> std::pair<query_status, std::vector<std::uint64_t>> {
	return m_device_config.query_pool_results(pool, first_query, query_count, stride);
}

auto gse::gpu::device::create_swapchain(const vec2i framebuffer_size, const present_mode mode, const gpu::swap_chain_handle old_handle) -> swap_chain_info {
	return m_device_config.create_swap_chain(framebuffer_size, mode, old_handle);
}

auto gse::gpu::device::acquire_swapchain_image(const gpu::swap_chain_handle swapchain, const gpu::handle<gpu::semaphore> wait_semaphore, const std::uint64_t timeout_ns) const -> gpu::acquire_next_image_result {
	return m_device_config.acquire_next_image(swapchain, wait_semaphore, timeout_ns);
}

auto gse::gpu::device::wait_swapchain_release_fences(const gpu::swap_chain_handle swapchain) const -> void {
	m_device_config.wait_swapchain_release_fences(swapchain);
}

auto gse::gpu::device::reset_swapchain_release_fence(const gpu::swap_chain_handle swapchain, const std::uint32_t image_index) const -> void {
	m_device_config.reset_swapchain_release_fence(swapchain, image_index);
}

auto gse::gpu::device::swapchain_release_fence(const gpu::swap_chain_handle swapchain, const std::uint32_t image_index) const -> gpu::handle<gpu::fence> {
	return m_device_config.swapchain_release_fence(swapchain, image_index);
}

auto gse::gpu::device::swapchain_past_presentation_timing(const gpu::swap_chain_handle swapchain) const -> std::vector<past_present_timing> {
	return m_device_config.swapchain_past_presentation_timing(swapchain);
}

auto gse::gpu::device::create_blas(const acceleration_structure_geometry& geometry, const std::uint32_t prim_count) -> blas {
	return m_device_config.create_blas(geometry, prim_count);
}

auto gse::gpu::device::create_tlas(const std::uint32_t max_instances) -> tlas {
	return m_device_config.create_tlas(max_instances);
}

auto gse::gpu::device::query_blas_build_sizes(const acceleration_structure_geometry& geometry, const std::uint32_t prim_count) const -> acceleration_structure_build_sizes {
	return m_device_config.query_blas_build_sizes(geometry, prim_count);
}

auto gse::gpu::device::acceleration_structure_scratch_alignment() const -> device_size {
	return m_device_config.acceleration_structure_scratch_alignment();
}

auto gse::gpu::device::host_upload_image_layers(const gpu::handle<gpu::image> img, const std::span<const void* const> layer_pointers, const vec2u extent) const -> void {
	m_device_config.host_upload_image_layers(img, layer_pointers, extent);
}

auto gse::gpu::device::create_buffer(const buffer_desc& desc, const std::string_view tag, const std::source_location& loc) -> buffer {
	return m_device_config.create_buffer(desc, tag, loc);
}

auto gse::gpu::device::create_image(const image_desc& desc, const std::string_view tag) -> image {
	return m_device_config.create_image(desc, tag);
}

auto gse::gpu::device::allocate_buffer_slot() -> gpu::bindless_handle {
	return m_device_config.allocate_buffer_slot();
}

auto gse::gpu::device::allocate_image_slot() -> gpu::bindless_handle {
	return m_device_config.allocate_image_slot();
}

auto gse::gpu::device::write_storage_buffer(const gpu::bindless_slot slot, const gpu::device_address address, const gpu::device_size size) -> void {
	m_device_config.write_storage_buffer(slot, address, size);
}

auto gse::gpu::device::write_uniform_buffer(const gpu::bindless_slot slot, const gpu::device_address address, const gpu::device_size size) -> void {
	m_device_config.write_uniform_buffer(slot, address, size);
}

auto gse::gpu::device::write_acceleration_structure(const gpu::bindless_slot slot, const gpu::device_address as_address) -> void {
	m_device_config.write_acceleration_structure(slot, as_address);
}

auto gse::gpu::device::write_sampled_image(const gpu::bindless_slot slot, const image& img) -> void {
	m_device_config.write_sampled_image(slot, img);
}

auto gse::gpu::device::register_sampler(const sampler_desc& desc) -> gpu::bindless_handle {
	return m_device_config.register_sampler(desc);
}

auto gse::gpu::device::register_texture(const image& img, const sampler_desc& desc) -> gpu::bindless_handle {
	return m_device_config.register_texture(img, desc);
}

auto gse::gpu::device::bindless_layout() const -> gpu::bindless_layout {
	return m_device_config.bindless_layout();
}

auto gse::gpu::device::bindless_resource_heap_binding() const -> gpu::bindless_heap_binding {
	return m_device_config.bindless_resource_heap_binding();
}

auto gse::gpu::device::bindless_sampler_heap_binding() const -> gpu::bindless_heap_binding {
	return m_device_config.bindless_sampler_heap_binding();
}

auto gse::gpu::device::create_sampler(const sampler_desc& desc) -> gpu::handle<gpu::sampler> {
	return m_device_config.create_sampler(desc);
}

auto gse::gpu::device::collect_garbage() -> void {
	m_device_config.collect_garbage();
}

auto gse::gpu::device::upload_image_2d(image& img, const void* pixel_data) -> sync_token {
	const auto extent3 = img.extent();
	const vec2u extent2{ extent3.x(), extent3.y() };
	const void* ptrs[] = { pixel_data };
	host_upload_image_layers(img.handle(), ptrs, extent2);
	return {};
}

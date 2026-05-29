export module gse.gpu:device;

import std;

import :aliases;

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

		~device() override;

		[[nodiscard]] auto handle() const -> gpu::device_handle;

		[[nodiscard]] auto allocator(
			this auto& self
		) -> auto&;

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
		) const -> shader_program;

		[[nodiscard]]
		auto create_sync(
			std::uint32_t image_count,
			std::uint32_t frames_in_flight = max_frames_in_flight
		) -> sync;

		[[nodiscard]]
		auto create_timeline_semaphore(
			std::uint64_t initial_value
		) -> semaphore;

		[[nodiscard]]
		auto create_timestamp_query_pool(
			std::uint32_t capacity,
			std::string_view label = {}
		) const -> query_pool;

		[[nodiscard]]
		auto create_pipeline_stats_query_pool(
			std::uint32_t capacity,
			pipeline_statistic_flags statistics,
			std::string_view label = {}
		) const -> query_pool;

		[[nodiscard]]
		auto create_bindless_heaps(
			std::uint32_t texture_capacity = 2048,
			std::uint32_t image_capacity = 65536,
			std::uint32_t buffer_capacity = 16384,
			std::uint32_t sampler_capacity = 512
		) const -> std::unique_ptr<bindless_heaps>;

		[[nodiscard]]
		auto create_swapchain_backend(
			vec2i framebuffer_size,
			present_mode mode,
			gpu::swap_chain_handle old_handle = {}
		) -> vulkan::swap_chain;

		[[nodiscard]]
		auto acquire_swapchain_image(
			gpu::swap_chain_handle swapchain,
			gpu::semaphore_handle wait_semaphore,
			std::uint64_t timeout_ns = std::numeric_limits<std::uint64_t>::max()
		) const -> gpu::acquire_next_image_result;

		auto wait_swapchain_release_fences(
			const vulkan::swap_chain& sc
		) const -> void;

		auto reset_swapchain_release_fence(
			vulkan::swap_chain& sc,
			std::uint32_t image_index
		) const -> void;

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
			gpu::image_handle img,
			std::span<const void* const> layer_pointers,
			vec2u extent
		) const -> void;

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
		auto frame_command_buffer(
			queue_type queue,
			std::uint32_t frame_index
		) const -> gpu::command_buffer_handle;

		auto submit(
			queue_type queue,
			const submit_info& info,
			gpu::fence_handle signal_fence = {}
		) -> void;

		[[nodiscard]] auto present(
			const present_info& info
		) -> result;

		[[nodiscard]]
		auto wait_for_fence(
			gpu::fence_handle f,
			std::uint64_t timeout_ns = std::numeric_limits<std::uint64_t>::max()
		) const -> result;

		auto reset_fence(
			gpu::fence_handle f
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
		) const -> std::pair<gpu::image_handle, memory_requirements>;

		[[nodiscard]]
		auto create_buffer_unbound(
			const buffer_desc& info
		) const -> std::pair<gpu::buffer_handle, memory_requirements>;

		auto bind_image_memory(
			gpu::image_handle img,
			device_memory mem,
			device_size offset
		) const -> void;

		auto bind_buffer_memory(
			gpu::buffer_handle buf,
			device_memory mem,
			device_size offset
		) const -> void;

		[[nodiscard]]
		auto create_image_view(
			gpu::image_handle img,
			const image_view_create_info& info
		) const -> gpu::image_view_handle;

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
			gpu::image_handle img_handle,
			gpu::image_view_handle view_handle,
			image_format format,
			vec3u extent,
			const image_view_create_info& view_info,
			std::string_view tag
		) -> std::unique_ptr<image>;

		[[nodiscard]]
		auto make_aliased_buffer(
			gpu::buffer_handle buf_handle,
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

auto gse::gpu::device::allocator(this auto& self) -> auto& {
	return self.m_device_config;
}

auto gse::gpu::device::create_shader_program(const shader_program_create_info& info) const -> shader_program {
	return shader_program::create(m_device_config, info);
}

auto gse::gpu::device::create_sync(const std::uint32_t image_count, const std::uint32_t frames_in_flight) -> sync {
	return sync::create(m_device_config, image_count, frames_in_flight);
}

auto gse::gpu::device::create_timeline_semaphore(const std::uint64_t initial_value) -> semaphore {
	return semaphore::create_timeline(m_device_config, initial_value);
}

auto gse::gpu::device::create_timestamp_query_pool(const std::uint32_t capacity, const std::string_view label) const -> query_pool {
	return query_pool::create_timestamp(m_device_config, capacity, label);
}

auto gse::gpu::device::create_pipeline_stats_query_pool(const std::uint32_t capacity, const pipeline_statistic_flags statistics, const std::string_view label) const -> query_pool {
	return query_pool::create_pipeline_stats(m_device_config, capacity, statistics, label);
}

auto gse::gpu::device::create_bindless_heaps(const std::uint32_t texture_capacity, const std::uint32_t image_capacity, const std::uint32_t buffer_capacity, const std::uint32_t sampler_capacity) const -> std::unique_ptr<bindless_heaps> {
	return std::make_unique<bindless_heaps>(m_device_config, texture_capacity, image_capacity, buffer_capacity, sampler_capacity);
}

auto gse::gpu::device::create_swapchain_backend(const vec2i framebuffer_size, const present_mode mode, const gpu::swap_chain_handle old_handle) -> vulkan::swap_chain {
	return vulkan::swap_chain::create(framebuffer_size, mode, m_instance, m_device_config, old_handle);
}

auto gse::gpu::device::acquire_swapchain_image(const gpu::swap_chain_handle swapchain, const gpu::semaphore_handle wait_semaphore, const std::uint64_t timeout_ns) const -> gpu::acquire_next_image_result {
	return m_device_config.acquire_next_image(swapchain, wait_semaphore, timeout_ns);
}

auto gse::gpu::device::wait_swapchain_release_fences(const vulkan::swap_chain& sc) const -> void {
	sc.wait_release_fences(m_device_config);
}

auto gse::gpu::device::reset_swapchain_release_fence(vulkan::swap_chain& sc, const std::uint32_t image_index) const -> void {
	sc.reset_release_fence(m_device_config, image_index);
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

auto gse::gpu::device::host_upload_image_layers(const gpu::image_handle img, const std::span<const void* const> layer_pointers, const vec2u extent) const -> void {
	m_device_config.host_upload_image_layers(img, layer_pointers, extent);
}

auto gse::gpu::device::create_buffer(const buffer_desc& desc, const std::string_view tag, const std::source_location& loc) -> buffer {
	return m_device_config.create_buffer(desc, tag, loc);
}

auto gse::gpu::device::create_image(const image_desc& desc, const std::string_view tag) -> image {
	return m_device_config.create_image(desc, tag);
}

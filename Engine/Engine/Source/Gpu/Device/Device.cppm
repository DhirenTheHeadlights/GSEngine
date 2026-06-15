export module gse.gpu:device;

import std;

import :sync_token;
import :video_encoder;

import gse.gpu_backend;
import gse.assert;
import gse.ecs;
import gse.os;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.save;
import gse.math;

export namespace gse::gpu {
	struct device_backend;

	class device final : public non_copyable {
	public:
		[[nodiscard]]
		static auto create(
			shared_view<window::data> win,
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

		[[nodiscard]] auto create_semaphore() -> gpu::handle<gpu::semaphore>;

		[[nodiscard]] auto create_fence(
			bool signaled
		) -> gpu::handle<gpu::fence>;

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

		[[nodiscard]]
		auto create_timeline_semaphore(
			std::uint64_t initial_value
		) -> gpu::handle<gpu::semaphore>;

		auto retire(
			gpu::handle<gpu::semaphore> semaphore
		) -> void;

		auto retire(
			gpu::handle<gpu::fence> fence
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

		[[nodiscard]] auto transient() -> transient_executor<device>&;

		[[nodiscard]] auto video_encode_enabled() const -> bool;

	private:
		device(
			std::unique_ptr<device_backend> backend,
			image_format surface_format,
			bool video_encode_enabled
		);

		std::unique_ptr<device_backend> m_backend;
		std::unique_ptr<transient_executor<device>> m_transient;
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

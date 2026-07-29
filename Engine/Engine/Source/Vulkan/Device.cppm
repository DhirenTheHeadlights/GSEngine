export module gse.vulkan:device;

import std;
import vulkan;

import :aftermath;
import gse.gpu_backend;
import :commands;
import :instance;
import :physical_device;
import :queues;
import :types;
import :shader_object;
import :bindless_mapping;

import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.meta;
import gse.log;
import gse.win32;

namespace gse::vulkan {
	class transient_command_pool final : public non_copyable {
	public:
		transient_command_pool() {}
		~transient_command_pool() = default;

		transient_command_pool(
			transient_command_pool&&
		) noexcept = default;

		auto operator=(
			transient_command_pool&&
		) noexcept -> transient_command_pool& = default;

		[[nodiscard]]
		static auto create(
			const vk::raii::Device& vk_device,
			std::uint32_t family
		) -> gpu::expected<transient_command_pool>;

		[[nodiscard]] auto allocate_primary(
			const vk::raii::Device& vk_device
		) -> gpu::command_buffer_handle;

		auto try_reset(
			std::uint64_t queue_progress
		) -> void;

		auto mark_in_use_until(
			std::uint64_t value
		) -> void;

		auto reset_all() -> void;

	private:
		explicit transient_command_pool(
			vk::raii::CommandPool&& pool
		);

		static constexpr std::uint32_t allocation_batch_size = 8;

		vk::raii::CommandPool m_pool{ nullptr };
		std::vector<vk::raii::CommandBuffer> m_owned_cbs;
		std::size_t m_used = 0;
		std::uint64_t m_high_water_mark = 0;
	};

	struct swap_chain_resources {
		vk::raii::SwapchainKHR swapchain = nullptr;
		std::vector<vk::raii::ImageView> image_views;
		std::vector<vk::raii::Fence> release_fences;
	};

	struct descriptor_heap_resources {
		vk::raii::Buffer buffer = nullptr;
		vk::raii::DeviceMemory memory = nullptr;
		gpu::device_address address = 0;
		std::byte* mapped = nullptr;
	};

	struct bindless_state {
		gpu::handle<gpu::descriptor_heap> resource_heap;
		gpu::handle<gpu::descriptor_heap> sampler_heap;
		gpu::bindless_heap_binding resource_binding;
		gpu::bindless_heap_binding sampler_binding;
		gpu::bindless_slot_pool image_pool;
		gpu::bindless_slot_pool buffer_pool;
		gpu::bindless_slot_pool acceleration_structure_pool;
		gpu::bindless_slot_pool texture_pool;
		gpu::bindless_slot_pool sampler_pool;
	};

	struct vk_resource_manifest {
		static consteval auto entries() -> std::vector<gpu::manifest_row> {
			return {
				{ ^^gpu::handle<gpu::semaphore>, ^^vk::raii::Semaphore },
				{ ^^gpu::handle<gpu::fence>, ^^vk::raii::Fence },
				{ ^^gpu::acceleration_structure, ^^vk::raii::AccelerationStructureKHR },
				{ ^^gpu::swap_chain_handle, ^^swap_chain_resources },
				{ ^^gpu::handle<gpu::query_pool>, ^^vk::raii::QueryPool },
				{ ^^gpu::handle<gpu::sampler>, ^^vk::raii::Sampler },
				{ ^^gpu::handle<gpu::shader_object>, ^^vk::raii::ShaderEXT },
				{ ^^gpu::handle<gpu::pipeline_layout>, ^^vk::raii::PipelineLayout },
				{ ^^gpu::handle<gpu::descriptor_heap>, ^^descriptor_heap_resources },
			};
		}
	};
}

export namespace gse::vulkan {
	class blas;
	class tlas;

	struct device_creation_result;

	class device : public non_copyable {
	public:
		~device();

		device(
			device&&
		) noexcept;

		auto operator=(
			device&&
		) noexcept -> device&;

		[[nodiscard]]
		static auto create(
			const instance& instance_data,
			gpu::device_settings& cfg,
			aftermath& aftermath_tracker
		) -> gpu::expected<device_creation_result>;

		[[nodiscard]] auto physical_device(
			this auto&& self
		) -> auto&;

		[[nodiscard]] auto raii_device(
			this auto&& self
		) -> auto&;

		[[nodiscard]] auto device_handle() const -> gpu::device_handle;

		[[nodiscard]] auto fault_enabled() const -> bool;

		[[nodiscard]] auto vendor_binary_fault_enabled() const -> bool;

		auto wait_idle() const -> void;

		[[nodiscard]] auto timestamp_period() const -> float;

		[[nodiscard]] auto queue_family(
			gpu::queue_type queue
		) const -> std::uint32_t;

		[[nodiscard]] auto graphics_family() const -> std::uint32_t;

		[[nodiscard]] auto compute_family() const -> std::uint32_t;

		[[nodiscard]] auto families_distinct() const -> bool;

		[[nodiscard]] auto query_fault_counts(
			gpu::device_fault_counts& counts
		) const -> gpu::result;

		[[nodiscard]]
		auto query_fault_info(
			gpu::device_fault_counts& counts,
			gpu::device_fault_info& info
		) const -> gpu::result;

		auto create_buffer(
			const gpu::buffer_desc& desc,
			std::string_view tag = "",
			const std::source_location& loc = std::source_location::current()
		) -> gpu::buffer;

		auto create_image(
			const gpu::image_create_info& info,
			gpu::memory_property_flags properties = gpu::memory_property_flag::device_local,
			const gpu::image_view_create_info& view_info = {},
			const void* data = nullptr,
			std::string_view tag = "",
			std::source_location loc = std::source_location::current()
		) -> gpu::image;

		auto create_image(
			const gpu::image_desc& desc,
			std::string_view tag = "",
			const std::source_location& loc = std::source_location::current()
		) -> gpu::image;

		[[nodiscard]]
		auto buffer_slot(
			gpu::handle<gpu::buffer> buffer
		) const -> gpu::bindless_slot;

		[[nodiscard]]
		auto buffer_address(
			gpu::handle<gpu::buffer> buffer
		) const -> gpu::device_address;

		[[nodiscard]]
		auto buffer_size(
			gpu::handle<gpu::buffer> buffer
		) const -> gpu::device_size;

		[[nodiscard]]
		auto buffer_mapped(
			gpu::handle<gpu::buffer> buffer
		) const -> std::byte*;

		[[nodiscard]]
		auto image_sampled_slot(
			gpu::handle<gpu::image> image
		) const -> gpu::bindless_slot;

		[[nodiscard]]
		auto image_storage_slot(
			gpu::handle<gpu::image> image
		) const -> gpu::bindless_slot;

		[[nodiscard]]
		auto image_format_of(
			gpu::handle<gpu::image> image
		) const -> gpu::image_format_value;

		[[nodiscard]]
		auto image_extent(
			gpu::handle<gpu::image> image
		) const -> vec3u;

		[[nodiscard]]
		auto image_view(
			gpu::handle<gpu::image> image
		) const -> gpu::handle<gpu::image_view>;

		[[nodiscard]]
		auto create_sampler(
			const gpu::sampler_desc& desc
		) -> gpu::handle<gpu::sampler>;

		[[nodiscard]]
		auto create_shader_program(
			const gpu::shader_program_create_info& info
		) -> gpu::shader_program;

		[[nodiscard]]
		auto descriptor_heap_properties() const -> gpu::descriptor_heap_properties;

		[[nodiscard]]
		auto create_descriptor_heap(
			gpu::device_size size
		) -> gpu::handle<gpu::descriptor_heap>;

		[[nodiscard]]
		auto descriptor_heap_address(
			gpu::handle<gpu::descriptor_heap> heap
		) const -> gpu::device_address;

		auto write_image_descriptor(
			gpu::handle<gpu::descriptor_heap> heap,
			gpu::device_size byte_offset,
			gpu::image_descriptor_kind kind,
			const gpu::image& img
		) const -> void;

		auto write_buffer_descriptor(
			gpu::handle<gpu::descriptor_heap> heap,
			gpu::device_size byte_offset,
			gpu::device_address address,
			gpu::device_size range
		) const -> void;

		auto write_sampler_descriptor(
			gpu::handle<gpu::descriptor_heap> heap,
			gpu::device_size byte_offset,
			const gpu::sampler_desc& desc
		) const -> void;

		[[nodiscard]]
		auto allocate_buffer_slot() -> gpu::bindless_handle;

		[[nodiscard]]
		auto allocate_image_slot() -> gpu::bindless_handle;

		[[nodiscard]]
		auto allocate_acceleration_structure_slot() -> gpu::bindless_handle;

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

		[[nodiscard]]
		auto register_sampler(
			const gpu::sampler_desc& desc
		) -> gpu::bindless_handle;

		[[nodiscard]]
		auto register_texture(
			const gpu::image& img,
			const gpu::sampler_desc& desc
		) -> gpu::bindless_handle;

		[[nodiscard]]
		auto bindless_resource_heap_binding() const -> gpu::bindless_heap_binding;

		[[nodiscard]]
		auto bindless_sampler_heap_binding() const -> gpu::bindless_heap_binding;

		auto init_bindless() -> void;

		[[nodiscard]] auto live_allocation_count() const -> std::uint32_t;

		[[nodiscard]] auto tracking_enabled() const -> bool;

		auto destroy_buffer(
			gpu::handle<gpu::buffer> buffer
		) -> void;

		auto retire(
			gpu::handle<gpu::buffer> buffer
		) -> void;

		auto retire(
			gpu::handle<gpu::image> image
		) -> void;

		auto retire(
			gpu::acceleration_structure acceleration_structure
		) -> void;

		auto retire(
			gpu::handle<gpu::semaphore> semaphore
		) -> void;

		auto retire(
			gpu::handle<gpu::fence> fence
		) -> void;

		auto collect_garbage() -> void;

		[[nodiscard]]
		auto buffer_device_address(
			gpu::handle<gpu::buffer> buffer
		) const -> gpu::device_address;

		auto destroy_image(
			gpu::handle<gpu::image> image
		) -> void;

		auto destroy_image_view(
			gpu::handle<gpu::image_view> view
		) const -> void;

		auto free_allocation(
			const gpu::allocation& alloc
		) -> void;

		[[nodiscard]]
		auto create_image_unbound(
			const gpu::image_create_info& info
		) const -> std::pair<gpu::handle<gpu::image>, gpu::memory_requirements>;

		[[nodiscard]]
		auto create_buffer_unbound(
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

		[[nodiscard]]
		auto create_image_view(
			gpu::handle<gpu::image> img,
			const gpu::image_view_create_info& info
		) const -> gpu::handle<gpu::image_view>;

		[[nodiscard]]
		auto allocate_aliased_memory(
			gpu::device_size size,
			std::uint32_t memory_type_index
		) const -> gpu::device_memory;

		auto free_aliased_memory(
			gpu::device_memory mem
		) const -> void;

		[[nodiscard]]
		auto create_exportable_image_unbound(
			const gpu::image_create_info& info
		) const -> std::pair<gpu::handle<gpu::image>, gpu::memory_requirements>;

		[[nodiscard]]
		auto allocate_exportable_memory(
			gpu::device_size size,
			std::uint32_t memory_type_index,
			gpu::handle<gpu::image> img
		) const -> gpu::device_memory;

		[[nodiscard]]
		auto export_memory_handle(
			gpu::device_memory mem
		) const -> std::expected<void*, std::string>;

		[[nodiscard]]
		auto import_memory_handle(
			gpu::device_size size,
			std::uint32_t memory_type_index,
			gpu::handle<gpu::image> img,
			void* handle
		) const -> std::expected<gpu::device_memory, std::string>;

		[[nodiscard]]
		auto create_exportable_semaphore() -> gpu::handle<gpu::semaphore>;

		[[nodiscard]]
		auto export_semaphore_handle(
			gpu::handle<gpu::semaphore> semaphore
		) const -> std::expected<void*, std::string>;

		[[nodiscard]]
		auto import_semaphore_handle(
			void* handle
		) -> std::expected<gpu::handle<gpu::semaphore>, std::string>;

		[[nodiscard]]
		auto create_shared_surface(
			const gpu::shared_surface_desc& desc
		) const -> std::expected<gpu::shared_surface, std::string>;

		[[nodiscard]]
		auto import_shared_surface(
			const gpu::shared_surface_desc& desc,
			void* handle
		) const -> std::expected<gpu::shared_surface, std::string>;

		auto destroy_shared_surface(
			const gpu::shared_surface& surface
		) const -> void;

		auto run_exportable_self_test() const -> void;

		[[nodiscard]]
		auto find_memory_type_index(
			std::uint32_t type_bits,
			gpu::memory_property_flags required
		) const -> std::uint32_t;

		auto host_upload_image_layers(
			gpu::handle<gpu::image> img,
			std::span<const void* const> layer_pointers,
			vec2u extent
		) const -> void;

		[[nodiscard]]
		auto wait_for_fence(
			gpu::handle<gpu::fence> fence,
			std::uint64_t timeout_ns = std::numeric_limits<std::uint64_t>::max()
		) const -> gpu::result;

		auto reset_fence(
			gpu::handle<gpu::fence> fence
		) const -> void;

		[[nodiscard]]
		auto acquire_next_image(
			gpu::swap_chain_handle swapchain,
			gpu::handle<gpu::semaphore> wait_semaphore,
			std::uint64_t timeout_ns = std::numeric_limits<std::uint64_t>::max()
		) const -> gpu::acquire_next_image_result;

		[[nodiscard]]
		auto create_swap_chain(
			vec2i framebuffer_size,
			gpu::present_mode preferred_present_mode,
			gpu::swap_chain_handle old_swapchain = {}
		) -> gpu::swap_chain_info;

		auto wait_swapchain_release_fences(
			gpu::swap_chain_handle swapchain
		) const -> void;

		auto reset_swapchain_release_fence(
			gpu::swap_chain_handle swapchain,
			std::uint32_t image_index
		) const -> void;

		[[nodiscard]]
		auto swapchain_release_fence(
			gpu::swap_chain_handle swapchain,
			std::uint32_t image_index
		) const -> gpu::handle<gpu::fence>;

		[[nodiscard]]
		auto swapchain_past_presentation_timing(
			gpu::swap_chain_handle swapchain
		) const -> std::vector<gpu::past_present_timing>;

		[[nodiscard]]
		auto create_blas(
			const gpu::acceleration_structure_geometry& geometry,
			std::uint32_t prim_count
		) -> gpu::blas;

		[[nodiscard]]
		auto create_tlas(
			std::uint32_t max_instances
		) -> gpu::tlas;

		[[nodiscard]]
		auto query_blas_build_sizes(
			const gpu::acceleration_structure_geometry& geometry,
			std::uint32_t prim_count
		) const -> gpu::acceleration_structure_build_sizes;

		[[nodiscard]]
		auto query_tlas_build_sizes(
			std::uint32_t max_instances
		) const -> gpu::acceleration_structure_build_sizes;

		[[nodiscard]] auto acceleration_structure_scratch_alignment() const -> gpu::device_size;

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
			gpu::pipeline_statistic_flags statistics,
			std::string_view label = {}
		) -> gpu::handle<gpu::query_pool>;

		[[nodiscard]]
		auto query_pool_results(
			gpu::handle<gpu::query_pool> pool,
			std::uint32_t first_query,
			std::uint32_t query_count,
			std::uint64_t stride
		) const -> std::pair<gpu::query_status, std::vector<std::uint64_t>>;

	private:
		device(
			class physical_device&& physical_device,
			vk::raii::Device&& device,
			gpu::device_settings& cfg,
			bool device_fault_enabled,
			bool device_fault_vendor_binary_enabled,
			std::uint32_t graphics_family,
			std::uint32_t compute_family,
			gpu::surface surface
		);

		auto create_buffer(
			const vk::BufferCreateInfo& buffer_info,
			const void* data,
			std::string_view tag,
			const std::source_location& loc
		) -> gpu::buffer;

		auto create_image(
			const vk::ImageCreateInfo& info,
			vk::MemoryPropertyFlags properties,
			const vk::ImageViewCreateInfo& view_info,
			const void* data,
			std::string_view tag,
			std::source_location loc,
			gpu::image_view_create_info engine_view_info = {}
		) -> gpu::image;

		auto allocate(
			const vk::MemoryRequirements& requirements,
			vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
			std::string_view tag = "",
			std::source_location loc = std::source_location::current(),
			bool device_address = false
		) -> std::expected<gpu::allocation, std::string>;

		auto clean_up() -> void;

		static auto memory_flag_preferences(
			vk::BufferUsageFlags usage
		) -> std::vector<vk::MemoryPropertyFlags>;

		auto host_transition_image_to_general(
			gpu::handle<gpu::image> img,
			gpu::image_aspect_flag aspect,
			std::uint32_t layer_count
		) const -> void;

		[[nodiscard]]
		auto create_acceleration_structure(
			gpu::handle<gpu::buffer> storage_buffer,
			gpu::device_size size,
			gpu::acceleration_structure_type type
		) -> gpu::acceleration_structure;

		[[nodiscard]]
		auto acceleration_structure_address(
			gpu::acceleration_structure acceleration_structure
		) const -> gpu::device_address;

		template <typename Frontend, typename Raii>
		auto adopt(
			Raii&& object
		) -> Frontend;

		struct memory_block {
			vk::DeviceMemory memory;
			vk::DeviceSize size;
			vk::MemoryPropertyFlags properties;
			std::list<gpu::sub_allocation> allocations;
			void* mapped = nullptr;
		};

		struct pool {
			std::uint32_t memory_type_index;
			std::list<memory_block> blocks;
		};

		struct pool_key {
			std::uint32_t memory_type_index;
			vk::MemoryPropertyFlags properties;
			bool device_address = false;

			auto operator==(
				const pool_key& other
			) const -> bool;
		};

		struct pool_key_hash {
			auto operator()(
				const pool_key& key
			) const noexcept -> std::size_t;
		};

		static constexpr vk::DeviceSize k_default_block_size = 1024 * 1024 * 64;

		class physical_device m_physical_device;
		vk::raii::Device m_device;
		bool m_fault_enabled = false;
		bool m_vendor_binary_fault_enabled = false;
		std::array<std::uint32_t, gpu::queue_type_count> m_queue_families{};
		gpu::surface m_surface;

		std::unordered_map<pool_key, pool, pool_key_hash> m_pools;
		mutable std::mutex m_mutex;

		std::atomic<std::uint32_t> m_live_allocation_count = 0;
		std::atomic<std::uint64_t> m_next_allocation_id = 1;
		bool m_cleaned_up = false;

		gpu::device_settings* m_settings = nullptr;
		std::unordered_map<std::uint64_t, gpu::allocation_debug_info> m_live_allocations;

		struct live_buffer {
			gpu::allocation alloc;
			gpu::bindless_slot slot;
			gpu::device_size size = 0;
			gpu::device_address address = 0;
			std::byte* mapped = nullptr;
		};

		struct live_image {
			gpu::allocation alloc;
			gpu::handle<gpu::image_view> view;
			gpu::bindless_slot storage_slot;
			gpu::bindless_slot sampled_slot;
			gpu::image_format_value format = 0;
			vec3u extent;
			gpu::image_view_create_info view_info;
		};

		std::unordered_map<std::uint64_t, live_buffer> m_live_buffers;
		std::unordered_map<std::uint64_t, live_image> m_live_images;
		std::vector<gpu::retired_resource> m_retired_buffers;
		std::vector<gpu::retired_resource> m_retired_images;
		gpu::resource_arena<vk_resource_manifest> m_owned;
		std::vector<transient_command_pool> m_transient_pools;
		std::uint64_t m_resource_frame = 0;
		gpu::descriptor_heap_properties m_descriptor_heap_props;
		std::unique_ptr<bindless_state> m_bindless;
	};

	struct device_creation_result {
		device device;
		queue queue;
		queue_family families;
		bool video_encode_enabled = false;
	};

	[[nodiscard]] auto pick_surface_format(
		const physical_device& physical_device,
		gpu::surface surface
	) -> gpu::image_format;

	[[nodiscard]] auto pick_surface_format(
		const device& dev,
		const instance& inst
	) -> gpu::image_format;
}

auto gse::vulkan::device::physical_device(this auto&& self) -> auto& {
	return self.m_physical_device;
}

auto gse::vulkan::device::raii_device(this auto&& self) -> auto& {
	return self.m_device;
}

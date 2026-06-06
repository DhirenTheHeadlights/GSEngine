export module gse.vulkan:device;

import std;
import vulkan;

import :aftermath;
import :allocation;
import :buffer;
import :image;
import :commands;
import :instance;
import :physical_device;
import :queues;
import :types;

import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.meta;
import gse.log;

namespace gse::vulkan {
	struct retired_resource {
		std::uint64_t value = 0;
		std::uint64_t retire_after = 0;
	};

	template <typename T>
	struct retiring_pool {
		std::unordered_map<std::uint64_t, T> live;
		std::vector<retired_resource> retired;

		auto retire(std::uint64_t key, std::uint64_t retire_after) -> void;

		auto collect(std::uint64_t frame) -> void;
	};
}

export namespace gse::vulkan {
	class blas;
	class tlas;

	struct device_creation_result;

	class device : public non_copyable {
	public:
		struct settings {
			[[
				= gse::settings::describe<"Track GPU resource lifetimes for leak detection and faulting diagnostics.">{}
			]]
			bool tracking_enabled = false;

			[[
				= gse::settings::describe<"Attach debug names to GPU resources so they appear by name in RenderDoc, "
										  "NSight, and validation messages.">{}
			]]
			bool name_resources = false;
		};

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
			settings& cfg,
			aftermath& aftermath_tracker
		) -> device_creation_result;

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
		) -> buffer;

		auto create_image(
			const gpu::image_create_info& info,
			gpu::memory_property_flags properties = gpu::memory_property_flag::device_local,
			const gpu::image_view_create_info& view_info = {},
			const void* data = nullptr,
			std::string_view tag = "",
			std::source_location loc = std::source_location::current()
		) -> image;

		auto create_image(
			const gpu::image_desc& desc,
			std::string_view tag = "",
			const std::source_location& loc = std::source_location::current()
		) -> image;

		[[nodiscard]] auto live_allocation_count() const -> std::uint32_t;

		[[nodiscard]] auto tracking_enabled() const -> bool;

		auto destroy_buffer(
			gpu::buffer_handle buffer
		) -> void;

		auto retire(
			gpu::buffer_handle buffer
		) -> void;

		auto retire(
			gpu::image_handle image
		) -> void;

		auto retire(
			gpu::acceleration_structure acceleration_structure
		) -> void;

		auto retire(
			gpu::semaphore_handle semaphore
		) -> void;

		auto collect_garbage() -> void;

		[[nodiscard]]
		auto buffer_device_address(
			gpu::buffer_handle buffer
		) const -> gpu::device_address;

		auto destroy_image(
			gpu::image_handle image
		) -> void;

		auto destroy_image_view(
			gpu::image_view_handle view
		) const -> void;

		auto free_allocation(
			const allocation& alloc
		) -> void;

		[[nodiscard]]
		auto create_image_unbound(
			const gpu::image_create_info& info
		) const -> std::pair<gpu::image_handle, gpu::memory_requirements>;

		[[nodiscard]]
		auto create_buffer_unbound(
			const gpu::buffer_desc& info
		) const -> std::pair<gpu::buffer_handle, gpu::memory_requirements>;

		auto bind_image_memory(
			gpu::image_handle img,
			gpu::device_memory mem,
			gpu::device_size offset
		) const -> void;

		auto bind_buffer_memory(
			gpu::buffer_handle buf,
			gpu::device_memory mem,
			gpu::device_size offset
		) const -> void;

		[[nodiscard]]
		auto create_image_view(
			gpu::image_handle img,
			const gpu::image_view_create_info& info
		) const -> gpu::image_view_handle;

		[[nodiscard]]
		auto allocate_aliased_memory(
			gpu::device_size size,
			std::uint32_t memory_type_index
		) const -> gpu::device_memory;

		auto free_aliased_memory(
			gpu::device_memory mem
		) const -> void;

		[[nodiscard]]
		auto find_memory_type_index(
			std::uint32_t type_bits,
			gpu::memory_property_flags required
		) const -> std::uint32_t;

		auto host_upload_image_layers(
			gpu::image_handle img,
			std::span<const void* const> layer_pointers,
			vec2u extent
		) const -> void;

		[[nodiscard]]
		auto wait_for_fence(
			gpu::fence_handle fence,
			std::uint64_t timeout_ns = std::numeric_limits<std::uint64_t>::max()
		) const -> gpu::result;

		auto reset_fence(
			gpu::fence_handle fence
		) const -> void;

		[[nodiscard]]
		auto acquire_next_image(
			gpu::swap_chain_handle swapchain,
			gpu::semaphore_handle wait_semaphore,
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
		) const -> gpu::fence_handle;

		[[nodiscard]]
		auto swapchain_wait_for_present(
			gpu::swap_chain_handle swapchain,
			std::uint64_t present_id,
			std::uint64_t timeout_ns = std::numeric_limits<std::uint64_t>::max()
		) const -> gpu::result;

		[[nodiscard]]
		auto create_blas(
			const gpu::acceleration_structure_geometry& geometry,
			std::uint32_t prim_count
		) -> blas;

		[[nodiscard]]
		auto create_tlas(
			std::uint32_t max_instances
		) -> tlas;

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

		[[nodiscard]] auto create_semaphore() -> gpu::semaphore_handle;

		[[nodiscard]]
		auto create_timeline_semaphore(
			std::uint64_t initial_value
		) -> gpu::semaphore_handle;

		[[nodiscard]]
		auto semaphore_counter_value(
			gpu::semaphore_handle semaphore
		) const -> std::uint64_t;

		auto wait_semaphore(
			gpu::semaphore_handle semaphore,
			std::uint64_t value
		) const -> void;

	private:
		device(
			class physical_device&& physical_device,
			vk::raii::Device&& device,
			settings& cfg,
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
		) -> buffer;

		auto create_image(
			const vk::ImageCreateInfo& info,
			vk::MemoryPropertyFlags properties,
			const vk::ImageViewCreateInfo& view_info,
			const void* data,
			std::string_view tag,
			std::source_location loc,
			gpu::image_view_create_info engine_view_info = {}
		) -> image;

		auto allocate(
			const vk::MemoryRequirements& requirements,
			vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
			std::string_view tag = "",
			std::source_location loc = std::source_location::current(),
			bool device_address = false
		) -> std::expected<allocation, std::string>;

		auto clean_up() -> void;

		static auto memory_flag_preferences(
			vk::BufferUsageFlags usage
		) -> std::vector<vk::MemoryPropertyFlags>;

		auto host_transition_image_to_general(
			gpu::image_handle img,
			gpu::image_aspect_flag aspect,
			std::uint32_t layer_count
		) const -> void;

		[[nodiscard]]
		auto create_acceleration_structure(
			gpu::buffer_handle storage_buffer,
			gpu::device_size size,
			gpu::acceleration_structure_type type
		) -> gpu::acceleration_structure;

		[[nodiscard]]
		auto acceleration_structure_address(
			gpu::acceleration_structure acceleration_structure
		) const -> gpu::device_address;

		struct memory_block {
			vk::DeviceMemory memory;
			vk::DeviceSize size;
			vk::MemoryPropertyFlags properties;
			std::list<sub_allocation> allocations;
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
		std::mutex m_mutex;

		std::atomic<std::uint32_t> m_live_allocation_count = 0;
		std::atomic<std::uint64_t> m_next_allocation_id = 1;
		bool m_cleaned_up = false;

		settings* m_settings = nullptr;
		std::unordered_map<std::uint64_t, allocation_debug_info> m_live_allocations;

		struct live_image {
			allocation alloc;
			gpu::image_view_handle view;
		};

		struct swap_chain_resources {
			vk::raii::SwapchainKHR swapchain = nullptr;
			std::vector<vk::raii::ImageView> image_views;
			std::vector<vk::raii::Fence> release_fences;
		};

		std::unordered_map<std::uint64_t, allocation> m_live_buffers;
		std::unordered_map<std::uint64_t, live_image> m_live_images;
		std::vector<retired_resource> m_retired_buffers;
		std::vector<retired_resource> m_retired_images;
		retiring_pool<vk::raii::AccelerationStructureKHR> m_acceleration_structures;
		retiring_pool<vk::raii::Semaphore> m_semaphores;
		retiring_pool<swap_chain_resources> m_swapchains;
		std::uint64_t m_resource_frame = 0;
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

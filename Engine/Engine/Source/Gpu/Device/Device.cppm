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

		[[nodiscard]] auto handle() const -> gpu::handle<vulkan::device>;

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
			gpu::handle<command_buffer> cmd,
			pass_marker_domain domain,
			pass_marker marker
		) -> pass_marker_handle;

		auto checkpoint_pass_marker(
			gpu::handle<command_buffer> cmd,
			pass_marker_handle handle
		) -> void;

		auto post_renderpass_pass_marker(
			gpu::handle<command_buffer> cmd,
			pass_marker_handle handle
		) -> void;

		auto end_pass_marker(
			gpu::handle<command_buffer> cmd,
			pass_marker_handle handle
		) -> void;

		[[nodiscard]] auto vulkan_instance(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto vulkan_device(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto frame_command_buffer(
			queue_type queue,
			std::uint32_t frame_index
		) const -> gpu::handle<command_buffer>;

		auto submit(
			queue_type queue,
			const submit_info& info,
			gpu::handle<fence> signal_fence = {}
		) -> void;

		[[nodiscard]] auto present(
			const present_info& info
		) -> result;

		[[nodiscard]] auto wait_for_fence(
			gpu::handle<fence> f,
			std::uint64_t timeout_ns = std::numeric_limits<std::uint64_t>::max()
		) const -> result;

		auto reset_fence(
			gpu::handle<fence> f
		) const -> void;

		auto reset_worker_command_pools(
			std::uint32_t frame_index
		) -> void;

		[[nodiscard]] auto acquire_worker_command_buffer(
			queue_type queue,
			std::size_t worker_index,
			std::uint32_t frame_index
		) -> gpu::handle<command_buffer>;

		[[nodiscard]] auto make_video_encoder(
			vec2u extent
		) -> std::optional<video_encoder>;

		[[nodiscard]] auto create_image_unbound(
			const image_create_info& info
		) const -> std::pair<gpu::handle<image>, memory_requirements>;

		[[nodiscard]] auto create_buffer_unbound(
			const buffer_create_info& info
		) const -> std::pair<gpu::handle<buffer>, memory_requirements>;

		auto bind_image_memory(
			gpu::handle<image> img,
			device_memory_handle mem,
			device_size offset
		) const -> void;

		auto bind_buffer_memory(
			gpu::handle<buffer> buf,
			device_memory_handle mem,
			device_size offset
		) const -> void;

		[[nodiscard]] auto create_image_view(
			gpu::handle<image> img,
			const image_view_create_info& info
		) const -> gpu::handle<image_view>;

		[[nodiscard]] auto allocate_aliased_memory(
			device_size size,
			std::uint32_t memory_type_index
		) const -> device_memory_handle;

		auto free_aliased_memory(
			device_memory_handle mem
		) const -> void;

		[[nodiscard]] auto find_memory_type_index(
			std::uint32_t type_bits,
			memory_property_flags required
		) const -> std::uint32_t;

		[[nodiscard]] auto make_aliased_image(
			gpu::handle<image> img_handle,
			gpu::handle<image_view> view_handle,
			image_format format,
			vec3u extent,
			const image_view_create_info& view_info,
			std::string_view tag
		) -> std::unique_ptr<image>;

		[[nodiscard]] auto make_aliased_buffer(
			gpu::handle<buffer> buf_handle,
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

auto gse::gpu::device::vulkan_instance(this auto& self) -> auto& {
	return self.m_instance;
}

auto gse::gpu::device::vulkan_device(this auto& self) -> auto& {
	return self.m_device_config;
}

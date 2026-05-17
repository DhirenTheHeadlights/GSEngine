export module gse.gpu:device;

import std;

import :aliases;
import :transient_executor;
import :vulkan_aftermath;
import :vulkan_buffer;
import :vulkan_command_pools;
import :vulkan_device;
import :vulkan_instance;
import :vulkan_queues;
import :descriptor_heap;
import :types;

import gse.assert;
import gse.os;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.save;

export namespace gse::gpu {
	class device final : public non_copyable {
	public:
		[[nodiscard]] static auto create(
			const window::data& win,
			bool validation_layers_enabled,
			vulkan::device::settings& device_cfg
		) -> std::unique_ptr<device>;

		~device() override;

		[[nodiscard]] auto allocator(
		) -> vulkan::device&;

		[[nodiscard]] auto allocator(
		) const -> const vulkan::device&;

		[[nodiscard]] auto descriptor_heap(
			this auto& self
		) -> decltype(auto);

		[[nodiscard]] auto surface_format(
		) const -> image_format;

		[[nodiscard]] auto queue_family(
			gpu::queue_type queue
		) const -> std::uint32_t;

		auto wait_idle(
		) const -> void;

		[[nodiscard]] auto timestamp_period(
		) const -> float;

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
			handle<command_buffer> cmd,
			pass_marker_domain domain,
			pass_marker marker
		) -> pass_marker_handle;

		auto checkpoint_pass_marker(
			handle<command_buffer> cmd,
			pass_marker_handle handle
		) -> void;

		auto post_renderpass_pass_marker(
			handle<command_buffer> cmd,
			pass_marker_handle handle
		) -> void;

		auto end_pass_marker(
			handle<command_buffer> cmd,
			pass_marker_handle handle
		) -> void;

		[[nodiscard]] auto vulkan_instance(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto vulkan_device(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto vulkan_queue(
		) -> vulkan::queue&;

		[[nodiscard]] auto vulkan_command(
		) -> vulkan::command&;

		[[nodiscard]] auto worker_command_pools(
		) -> vulkan::worker_command_pools&;

		[[nodiscard]] auto transient(
		) -> transient_executor&;

		[[nodiscard]] auto descriptor_buffer_props(
		) const -> const descriptor_buffer_properties&;

		[[nodiscard]] auto video_encode_enabled(
		) const -> bool;

	private:
		device(
			vulkan::aftermath&& aftermath_tracker,
			vulkan::instance&& instance,
			vulkan::device&& device,
			vulkan::queue&& queue,
			vulkan::command&& command,
			vulkan::worker_command_pools&& worker_pools,
			transient_executor&& transient,
			std::unique_ptr<gpu::descriptor_heap> descriptor_heap,
			descriptor_buffer_properties desc_buf_props,
			image_format surface_format,
			bool video_encode_enabled
		);

		vulkan::aftermath m_aftermath;
		vulkan::instance m_instance;
		vulkan::device m_device_config;
		vulkan::queue m_queue;
		vulkan::command m_command;
		vulkan::worker_command_pools m_worker_pools;
		transient_executor m_transient;
		std::unique_ptr<gpu::descriptor_heap> m_descriptor_heap;
		descriptor_buffer_properties m_descriptor_buffer_props;
		image_format m_surface_format;
		std::atomic<bool> m_device_lost_reported = false;
		bool m_video_encode_enabled = false;

		static constexpr std::size_t pass_marker_ring_size = 128;

		struct pass_marker_ring {
			std::array<pass_marker, pass_marker_ring_size> entries{};
			std::atomic<std::uint64_t> seq{ 1 };
			vulkan::buffer checkpoint_buffer;
			const std::uint32_t* checkpoint_slots = nullptr;
		};

		std::array<pass_marker_ring, pass_marker_domain_count> m_pass_marker_rings;
	};
}

auto gse::gpu::device::descriptor_heap(this auto& self) -> decltype(auto) {
	return (*self.m_descriptor_heap);
}

auto gse::gpu::device::vulkan_instance(this auto& self) -> auto& {
	return self.m_instance;
}

auto gse::gpu::device::vulkan_device(this auto& self) -> auto& {
	return self.m_device_config;
}

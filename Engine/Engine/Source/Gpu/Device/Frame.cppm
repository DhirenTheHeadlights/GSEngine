export module gse.gpu:frame;

import std;

import :device;
import :swap_chain;
import :present_pacer;

import gse.gpu_backend;
import gse.os;
import gse.core;

export namespace gse::gpu {
	struct present_target {
		id window_id;
		swap_chain* swapchain = nullptr;
		window::window_surface* window = nullptr;
		swapchain_sync<device> sync;
		std::uint32_t image_index = 0;
		std::uint64_t next_present_id = 1;
		std::array<std::uint64_t, max_frames_in_flight> present_ids_in_flight{};
		bool minimized_last = false;
		std::uint64_t minimized_frames = 0;
		bool restore_pending = false;
		bool acquired = false;
	};

	struct queue_submission {
		queue_type queue = queue_type::graphics;
		std::vector<command_buffer_handle> command_buffers;
		std::vector<semaphore_submit_info> waits;
		std::vector<semaphore_submit_info> signals;
	};

	class frame final : public non_copyable {
	public:
		~frame();

		[[nodiscard]] static auto create(
			device& dev,
			swap_chain* sc
		) -> std::unique_ptr<frame>;

		[[nodiscard]] auto swapchain() const -> swap_chain*;

		auto set_present_target(
			window::window_surface* win
		) -> void;

		auto add_present_target(
			id window_id,
			swap_chain* sc,
			window::window_surface* win
		) -> void;

		auto remove_present_target(
			id window_id
		) -> void;

		[[nodiscard]] auto target(
			id window_id
		) const -> const present_target*;

		[[nodiscard]] auto targets() const -> std::span<const present_target>;

		[[nodiscard]] auto current_frame() const -> std::uint32_t;

		[[nodiscard]] auto frame_count() const -> std::uint64_t;

		[[nodiscard]] auto queue_fence_signaled(
			queue_type queue,
			std::uint32_t ring_slot
		) const -> bool;

		[[nodiscard]] auto image_index() const -> std::uint32_t;

		[[nodiscard]]
		auto command_buffer(
			queue_type queue = queue_type::graphics
		) const -> command_buffer_handle;

		[[nodiscard]] auto frame_in_progress() const -> bool;

		auto begin() -> std::expected<frame_token, frame_status>;

		auto end(
			std::span<const queue_submission> aux_submissions = {},
			std::span<const semaphore_submit_info> extra_graphics_waits = {},
			std::span<const command_buffer_handle> graphics_buffers = {},
			std::span<const semaphore_submit_info> extra_graphics_signals = {}
		) -> void;

	private:
		frame(
			swapchain_sync<device>&& s,
			frame_fences<device>&& fences,
			device& dev,
			swap_chain* sc
		);

		auto recreate_resources(
			present_target& t
		) -> expected<void>;

		auto recreate_surface(
			present_target& t,
			std::string_view reason
		) -> expected<void>;

		static auto create_sync_objects(
			device& dev,
			const swap_chain& sc
		) -> swapchain_sync<device>;

		static int s_live_count;

		frame_fences<device> m_fences;
		std::vector<present_target> m_targets;
		std::array<std::uint64_t, queue_type_count> m_command_buffers{};
		std::uint32_t m_current_frame = 0;
		std::uint64_t m_frame_count = 0;
		bool m_frame_in_progress = false;
		device* m_device;
		present_pacer m_primary_pacer;
	};
}
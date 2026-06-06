export module gse.gpu:frame;

import std;

import :aliases;
import :device;
import :swap_chain;

import gse.os;
import gse.core;

export namespace gse::gpu {
	struct queue_submission {
		queue_type queue = queue_type::graphics;
		command_buffer_handle command_buffer;
		std::vector<semaphore_submit_info> waits;
		std::vector<semaphore_submit_info> signals;
	};

	class frame final : public non_copyable {
	public:
		[[nodiscard]] static auto create(
			device& dev,
			swap_chain& sc
		) -> std::unique_ptr<frame>;

		[[nodiscard]] auto current_frame() const -> std::uint32_t;

		[[nodiscard]] auto image_index() const -> std::uint32_t;

		[[nodiscard]]
		auto command_buffer(
			queue_type queue = queue_type::graphics
		) const -> command_buffer_handle;

		[[nodiscard]] auto frame_in_progress() const -> bool;

		auto begin(
			window::data& win
		) -> std::expected<frame_token, frame_status>;

		auto end(
			window::data& win,
			std::span<const queue_submission> aux_submissions = {},
			std::span<const semaphore_submit_info> extra_graphics_waits = {}
		) -> void;

		auto set_sync(
			sync&& s
		) -> void;

	private:
		frame(
			sync&& s,
			std::uint32_t image_index,
			device& dev,
			swap_chain& sc
		);

		auto recreate_resources(
			const window::data& win
		) -> void;

		static auto create_sync_objects(
			device& dev,
			const swap_chain& sc
		) -> sync;

		sync m_sync;
		std::uint32_t m_image_index = 0;
		std::array<std::uint64_t, queue_type_count> m_command_buffers{};
		std::uint32_t m_current_frame = 0;
		bool m_frame_in_progress = false;
		device* m_device;
		swap_chain* m_swapchain;
		std::uint64_t m_next_present_id = 1;
		std::array<std::uint64_t, max_frames_in_flight> m_present_ids_in_flight{};
	};
}

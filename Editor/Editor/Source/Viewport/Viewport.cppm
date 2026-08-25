export module gse.ide.viewport;

import std;
import gse;
import gse.gpu;
import gse.gpu_record;
import gse.ide.build;

export namespace gse::ide::viewport {
	struct pending_session {
		std::uint32_t generation = 0;
		std::uint32_t instance = 0;
		std::shared_ptr<const attached_surface_message> message;
	};

	struct imported_session {
		std::uint32_t generation = 0;
		std::uint32_t instance = 0;
		std::array<gpu::shared_surface, attached_ring_size> surfaces{};
		std::array<gpu::bindless_handle, attached_ring_size> slots{};
		gpu::handle<gpu::semaphore> produced_semaphore{};
		gpu::handle<gpu::semaphore> consumed_semaphore{};
		std::uint64_t released_value = 0;
	};

	struct retiring_session {
		imported_session session;
		std::uint32_t frames_remaining = gpu::max_frames_in_flight + 1;
	};

	struct [[= system_state<"Viewport">{}]] data {
		[[= shared]] per_frame_resource<gpu::image> targets;
		[[= shared]] std::array<gpu::bindless_handle, per_frame_resource<gpu::image>::frames_in_flight> slots;
		[[= shared]] gpu::bindless_slot display_slot = {};
		[[= shared]] vec2u extent{ 0, 0 };
		[[= shared]] bool ready = false;
		[[= shared]] std::array<gpu::bindless_slot, build_runner::max_attached_instances> instance_slots{};
		[[= shared]] std::array<vec2u, build_runner::max_attached_instances> instance_extents{};
		[[= shared]] std::array<bool, build_runner::max_attached_instances> instance_live{};
		std::vector<pending_session> pending;
		std::vector<imported_session> imported;
		std::vector<retiring_session> retiring;
	};

	[[= system_init{}]]
	auto init(
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		const context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		channel_write<gpu::render_pass_request> pass_out,
		channel_read<build_runner::attached_session_ended, build_runner::attached_surface_ready> surface_in,
		channel_write<build_runner::attached_surface_rejected, build_runner::attached_surface_imported> surface_out,
		shared_view<build_runner::data> build_d
	) -> async::task<>;
}

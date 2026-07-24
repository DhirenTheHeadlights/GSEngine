export module gse.ide.viewport;

import std;
import gse;
import gse.gpu;

export namespace gse::ide::viewport {
	struct [[= gse::system_state<"Viewport">{}]] data {
		[[= gse::shared]] gse::per_frame_resource<gse::gpu::image> targets;
		[[= gse::shared]] std::array<gse::gpu::bindless_handle, gse::per_frame_resource<gse::gpu::image>::frames_in_flight> slots;
		[[= gse::shared]] gse::gpu::bindless_slot display_slot = {};
		[[= gse::shared]] gse::vec2u extent{ 0, 0 };
		[[= gse::shared]] bool ready = false;
		[[= gse::shared]] bool imported_ready = false;
		std::array<gse::gpu::shared_surface, gse::attached_ring_size> imported{};
		std::array<gse::gpu::bindless_handle, gse::attached_ring_size> imported_slot_handles{};
		gse::gpu::handle<gse::gpu::semaphore> imported_semaphore{};
		std::optional<gse::attached_surface_message> pending_import;
	};

	[[= gse::system_init{}]]
	auto init(
		gse::shared_view<gse::gpu::context::data> gpu_s,
		data& d
	) -> gse::async::task<>;

	[[= gse::system_frame{}]]
	auto frame(
		const gse::context& ctx,
		gse::shared_view<gse::gpu::context::data> gpu_s,
		data& d
	) -> gse::async::task<>;
}

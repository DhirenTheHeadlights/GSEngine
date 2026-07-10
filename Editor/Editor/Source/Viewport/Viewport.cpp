module gse.ide.viewport;

import std;
import gse;
import gse.gpu;
import gse.gpu_record;
import gse.ide.build;
import gse.win32;

namespace gse::ide::viewport {
	constexpr gse::gpu::sampler_desc viewport_sampler{
		.min = gse::gpu::sampler_filter::linear,
		.mag = gse::gpu::sampler_filter::linear,
		.address_u = gse::gpu::sampler_address_mode::clamp_to_edge,
		.address_v = gse::gpu::sampler_address_mode::clamp_to_edge,
		.address_w = gse::gpu::sampler_address_mode::clamp_to_edge,
	};

	constexpr gse::vec2u viewport_extent{ 1280, 720 };
}

auto gse::ide::viewport::init(const gse::shared_view<gse::gpu::context::data> gpu_s, data& d) -> gse::async::task<> {
	for (std::size_t i = 0; i < gse::per_frame_resource<gse::gpu::image>::frames_in_flight; ++i) {
		d.targets[i] = gpu_s.device->create_image(
			{
				.size = viewport_extent,
				.format = gse::gpu::image_format::r8g8b8a8_unorm,
				.usage = gse::gpu::image_flag::color_attachment | gse::gpu::image_flag::sampled,
			},
			"editor_viewport"
		);
		gse::gpu::transition_image_to(*gpu_s.device, d.targets[i]);
		d.slots[i] = gpu_s.device->register_texture(d.targets[i], viewport_sampler);
	}
	d.extent = viewport_extent;
	d.ready = true;
	gse::log::println(gse::log::category::render, "Editor viewport: {}x{} render target ready", viewport_extent.x(), viewport_extent.y());
	return {};
}

auto gse::ide::viewport::frame(const gse::context& ctx, const gse::shared_view<gse::gpu::context::data> gpu_s, data& d) -> gse::async::task<> {
	if (!d.ready || !gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	if (!d.imported_ready) {
		if (const auto pending = gse::ide::build_runner::take_imported_surface()) {
			const gse::gpu::shared_surface_desc desc{
				.extent = pending->extent,
				.format = pending->format,
			};
			bool ok = true;
			for (std::size_t i = 0; i < gse::attached_ring_size; ++i) {
				auto imported = gpu_s.device->import_shared_surface(desc, pending->surface_handles[i]);
				if (!imported) {
					gse::log::println(gse::log::level::error, gse::log::category::render, "Editor viewport: import_shared_surface[{}] failed: {}", i, imported.error());
					ok = false;
					break;
				}
				d.imported[i] = *imported;
				const gse::gpu::image_view_create_info view_info{
					.format = d.imported[i].format,
					.view_type = gse::gpu::image_view_type::e2d,
					.aspects = gse::gpu::image_aspect_flags(gse::gpu::image_aspect_flag::color),
					.level_count = 1,
					.layer_count = 1,
				};
				gse::gpu::image wrapped(
					d.imported[i].image,
					d.imported[i].view,
					gse::gpu::format_value(d.imported[i].format),
					{ d.imported[i].extent.x(), d.imported[i].extent.y(), 1 },
					view_info
				);
				gse::gpu::transition_image_to(*gpu_s.device, wrapped);
				d.imported_slot_handles[i] = gpu_s.device->register_texture(wrapped, viewport_sampler);
				gse::win32::CloseHandle(pending->surface_handles[i]);
			}
			if (ok) {
				auto sem = gpu_s.device->import_semaphore_handle(pending->semaphore_handle);
				if (sem) {
					d.imported_semaphore = *sem;
					d.imported_ready = true;
					gse::log::println(gse::log::category::render, "Editor viewport: imported game surface ring {}x{}", pending->extent.x(), pending->extent.y());
				}
				else {
					gse::log::println(gse::log::level::error, gse::log::category::render, "Editor viewport: import_semaphore_handle failed: {}", sem.error());
				}
			}
			gse::win32::CloseHandle(pending->semaphore_handle);
		}
	}

	if (d.imported_ready) {
		const auto v = gpu_s.device->semaphore_counter_value(d.imported_semaphore);
		const std::size_t slot = static_cast<std::size_t>(v % gse::attached_ring_size);
		d.display_slot = d.imported_slot_handles[slot].slot();
		gpu_s.render_graph->add_graphics_wait({
			.semaphore = d.imported_semaphore,
			.value = v,
			.stages = gse::gpu::pipeline_stage_flag::all_commands,
		});
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();
	constexpr std::size_t buffers = gse::per_frame_resource<gse::gpu::image>::frames_in_flight;
	d.display_slot = d.slots[(frame_index + 1) % buffers].slot();

	co_await gse::gpu::pass<^^gse::ide::viewport::frame>(ctx)
		.color(gse::gpu::clear_color(
			gse::gpu::color_clear{ 0.10f, 0.13f, 0.20f, 1.0f },
			d.targets[frame_index]
		));
}

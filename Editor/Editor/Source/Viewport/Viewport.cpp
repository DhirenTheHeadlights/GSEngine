module gse.ide.viewport;

import std;
import gse;
import gse.gpu;
import gse.gpu_record;
import gse.ide.build;

namespace gse::ide::viewport {
	constexpr gpu::sampler_desc viewport_sampler{
		.min = gpu::sampler_filter::linear,
		.mag = gpu::sampler_filter::linear,
		.address_u = gpu::sampler_address_mode::clamp_to_edge,
		.address_v = gpu::sampler_address_mode::clamp_to_edge,
		.address_w = gpu::sampler_address_mode::clamp_to_edge,
	};

	constexpr vec2u viewport_extent{ 1280, 720 };

	auto destroy_imported_session(
		gpu::device& device,
		imported_session& session
	) -> void;

	auto reset_session(
		data& d,
		std::uint32_t generation
	) -> void;

	auto collect_retiring_sessions(
		gpu::device& device,
		data& d
	) -> void;
}

auto gse::ide::viewport::destroy_imported_session(gpu::device& device, imported_session& session) -> void {
	for (gpu::bindless_handle& slot : session.slots) {
		slot = {};
	}
	for (const gpu::shared_surface& surface : session.surfaces) {
		if (surface.image) {
			device.destroy_shared_surface(surface);
		}
	}
	if (session.produced_semaphore) {
		device.retire(session.produced_semaphore);
	}
	if (session.consumed_semaphore) {
		device.retire(session.consumed_semaphore);
	}
	session = imported_session{};
}

auto gse::ide::viewport::reset_session(data& d, const std::uint32_t generation) -> void {
	if (d.pending && d.pending->generation == generation) {
		d.pending.reset();
	}
	if (d.imported && d.imported->generation == generation) {
		d.display_slot = d.slots[0].slot();
		d.extent = viewport_extent;
		d.retiring.push_back({
			.session = std::move(*d.imported),
		});
		d.imported.reset();
	}
}

auto gse::ide::viewport::collect_retiring_sessions(gpu::device& device, data& d) -> void {
	std::erase_if(d.retiring, [&](retiring_session& retiring) {
		if (--retiring.frames_remaining > 0) {
			return false;
		}
		destroy_imported_session(device, retiring.session);
		return true;
	});
}

auto gse::ide::viewport::init(const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	for (std::size_t i = 0; i < per_frame_resource<gpu::image>::frames_in_flight; ++i) {
		d.targets[i] = gpu_s.device->create_image(
			{
				.size = viewport_extent,
				.format = gpu::image_format::r8g8b8a8_unorm,
				.usage = { gpu::image_flag::color_attachment, gpu::image_flag::sampled },
			},
			"editor_viewport"
		);
		gpu::transition_image_to(*gpu_s.device, d.targets[i]);
		d.slots[i] = gpu_s.device->register_texture(d.targets[i], viewport_sampler);
	}
	d.extent = viewport_extent;
	d.ready = true;
	log::println(log::category::render, "Editor viewport: {}x{} render target ready", viewport_extent.x(), viewport_extent.y());
	return {};
}

auto gse::ide::viewport::frame(const context& ctx, const shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out, const channel_read<build_runner::attached_session_ended, build_runner::attached_surface_ready> surface_in, const channel_write<build_runner::attached_surface_rejected, build_runner::attached_surface_imported> surface_out, const shared_view<build_runner::data> build_d) -> async::task<> {
	for (const build_runner::attached_session_ended& ended : surface_in.of<build_runner::attached_session_ended>()) {
		reset_session(d, ended.generation);
	}

	if (d.pending && (!build_d.session || build_d.session->generation != d.pending->generation)) {
		reset_session(d, d.pending->generation);
	}
	if (d.imported && (!build_d.session || build_d.session->generation != d.imported->generation)) {
		reset_session(d, d.imported->generation);
	}

	for (const build_runner::attached_surface_ready& ready : surface_in.of<build_runner::attached_surface_ready>()) {
		if (!build_d.session || build_d.session->generation != ready.generation) {
			continue;
		}
		if (!ready.message) {
			surface_out.push<build_runner::attached_surface_rejected>({
				.generation = ready.generation,
			});
			continue;
		}
		if ((d.pending && d.pending->generation == ready.generation) || (d.imported && d.imported->generation == ready.generation)) {
			continue;
		}
		if (ready.message->backend != gpu::active_backend) {
			log::println(
				log::level::error,
				log::category::render,
				"Editor viewport: game runs {} but editor runs {}; shared surfaces cannot cross graphics APIs",
				ready.message->backend,
				gpu::active_backend
			);
			surface_out.push<build_runner::attached_surface_rejected>({
				.generation = ready.generation,
			});
		}
		else {
			if (d.pending) {
				reset_session(d, d.pending->generation);
			}
			if (d.imported) {
				reset_session(d, d.imported->generation);
			}
			d.pending.emplace(pending_session{
				.generation = ready.generation,
				.message = ready.message,
			});
		}
	}

	if (!d.ready || !gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}
	collect_retiring_sessions(*gpu_s.device, d);

	if (!d.imported && d.pending) {
		const pending_session pending = std::move(*d.pending);
		d.pending.reset();
		const gpu::shared_surface_desc desc{
			.extent = pending.message->extent,
			.format = pending.message->format,
		};
		imported_session imported{
			.generation = pending.generation,
		};
		bool ok = true;
		for (std::size_t i = 0; i < attached_ring_size; ++i) {
			if (ok) {
				const auto surface = gpu_s.device->import_shared_surface(desc, pending.message->surface_handles[i]);
				if (surface) {
					imported.surfaces[i] = *surface;
					const gpu::image_view_create_info view_info{
						.format = imported.surfaces[i].format,
						.view_type = gpu::image_view_type::e2d,
						.aspects = gpu::image_aspect_flags(gpu::image_aspect_flag::color),
						.level_count = 1,
						.layer_count = 1,
					};
					gpu::image wrapped(
						imported.surfaces[i].image,
						imported.surfaces[i].view,
						imported.surfaces[i].format,
						{ imported.surfaces[i].extent.x(), imported.surfaces[i].extent.y(), 1 },
						view_info
					);
					gpu::transition_image_to(*gpu_s.device, wrapped);
					imported.slots[i] = gpu_s.device->register_texture(wrapped, viewport_sampler);
				}
				else {
					log::println(log::level::error, log::category::render, "Editor viewport: import_shared_surface[{}] failed: {}", i, surface.error());
					ok = false;
				}
			}
		}
		if (ok) {
			const auto produced = gpu_s.device->import_semaphore_handle(pending.message->produced_semaphore_handle);
			if (produced) {
				imported.produced_semaphore = *produced;
			}
			else {
				log::println(log::level::error, log::category::render, "Editor viewport: import produced semaphore failed: {}", produced.error());
				ok = false;
			}
		}
		if (ok) {
			const auto consumed = gpu_s.device->import_semaphore_handle(pending.message->consumed_semaphore_handle);
			if (consumed) {
				imported.consumed_semaphore = *consumed;
			}
			else {
				log::println(log::level::error, log::category::render, "Editor viewport: import consumed semaphore failed: {}", consumed.error());
				ok = false;
			}
		}
		if (ok) {
			d.extent = pending.message->extent;
			d.imported.emplace(std::move(imported));
			surface_out.push<build_runner::attached_surface_imported>({
				.generation = pending.generation,
			});
			log::println(log::category::render, "Editor viewport: imported game surface ring {}x{}", pending.message->extent.x(), pending.message->extent.y());
		}
		else {
			d.retiring.push_back({
				.session = std::move(imported),
			});
			surface_out.push<build_runner::attached_surface_rejected>({
				.generation = pending.generation,
			});
		}
	}

	if (d.imported) {
		const std::uint64_t produced_value = gpu_s.device->semaphore_counter_value(d.imported->produced_semaphore);
		if (produced_value > 0) {
			const std::size_t slot = static_cast<std::size_t>(produced_value % attached_ring_size);
			d.display_slot = d.imported->slots[slot].slot();
			gpu_s.render_graph->add_graphics_wait({
				.semaphore = d.imported->produced_semaphore,
				.value = produced_value,
				.stages = gpu::pipeline_stage_flag::all_commands,
			});
			const std::uint64_t released_value = produced_value >= attached_ring_size
				? produced_value - (attached_ring_size - 1)
				: 0;
			if (released_value > d.imported->released_value) {
				gpu_s.render_graph->add_graphics_signal({
					.semaphore = d.imported->consumed_semaphore,
					.value = released_value,
					.stages = gpu::pipeline_stage_flag::all_commands,
				});
				d.imported->released_value = released_value;
			}
		}
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();
	constexpr std::size_t buffers = per_frame_resource<gpu::image>::frames_in_flight;
	d.display_slot = d.slots[(frame_index + 1) % buffers].slot();

	co_await gpu::pass<^^gse::ide::viewport::frame>(pass_out)
		.color(gpu::clear_color(
			gpu::color_clear{
				.r = 0.10f,
				.g = 0.13f,
				.b = 0.20f,
				.a = 1.0f,
			},
			d.targets[frame_index]
		));
}

module gse.graphics;

import std;

import :capture_renderer;
import :ui_renderer;
import :capture_ring;
import :mp4_muxer;
import :settings;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.log;
import gse.save;
import gse.time;

namespace gse::renderer::capture {
	struct [[= shaders::shader_struct]] push_constants {
		vec2u extent;
		std::uint32_t rgba_index;
	};

	struct [[
		= shaders::binding<0, 1>{},
		= shaders::storage_image
	]] output_y {
		using element = float;
	};

	struct [[
		= shaders::binding<0, 2>{},
		= shaders::storage_image
	]] output_uv {
		using element = vec2f;
	};

	using shader_binding_types = type_pack<output_y, output_uv, shaders::bindless::textures>;

	using entry = gpu::compute_entry<gpu::body_path<"Compute/rgba_to_nv12">, gpu::layout<"rgba_to_nv12">, gpu::bindings<shader_binding_types>, gpu::threads<16, 16, 1>, gpu::push_constant<push_constants>, gpu::system_values<gpu::dispatch_thread_id>>;
}

auto gse::renderer::capture::system::run(run_context& ctx, const gpu::context::data& gpu_s, const asset::data& assets_s, const actions::system::data& sys, data& d) -> async::task<> {
	const auto register_action = [&](const std::string_view name, const key default_key) -> actions::handle {
		const id action_id = generate_id(name);
		ctx.channels.push<actions::add_action_request>({
			.name = std::string(name),
			.default_key = default_key,
			.action_id = action_id,
		});
		return actions::handle(action_id);
	};

	d.screenshot_action = register_action("Screenshot", key::f9);
	d.save_clip_action = register_action("Save Clip", key::f10);

	if (!gpu_s.device->video_encode_enabled()) {
		log::println(log::category::render, "Video encode not available, capture limited to screenshots");
	}
	else if (const auto caps = gpu::video_encoder::probe(*gpu_s.device); !caps.available) {
		log::println(log::category::render, "Video encode probe failed, capture limited to screenshots");
	}
	else {
		const auto ext = gpu_s.render_graph->extent();
		const auto half_ext = vec2u{ ext.x() / 2, ext.y() / 2 };

		d.convert_pipeline =
			gpu::build_compute_program(
				*gpu_s.device,
				*gpu_s.shader_registry,
				*gpu_s.bindless_textures,
				entry::pod
			);

		d.capture_sampler = gpu::sampler::create(
			gpu_s.device->allocator(),
			{
				.min = gpu::sampler_filter::nearest,
				.mag = gpu::sampler_filter::nearest,
				.address_u = gpu::sampler_address_mode::clamp_to_edge,
				.address_v = gpu::sampler_address_mode::clamp_to_edge,
				.address_w = gpu::sampler_address_mode::clamp_to_edge,
			}
		);

		for (std::size_t i = 0; i < per_frame_resource<gpu::image>::frames_in_flight; ++i) {
			d.rgba_captures[i] = gpu::image::create(
				gpu_s.device->allocator(),
				{
					.size = ext,
					.format = gpu::image_format::r8g8b8a8_srgb,
					.usage = gpu::image_flag::sampled | gpu::image_flag::transfer_dst,
				}
			);

			d.rgba_slots[i] = gpu_s.bindless_textures->allocate(
				d.rgba_captures[i].view(),
				d.capture_sampler.native()
			);

			d.y_planes[i] = gpu::image::create(
				gpu_s.device->allocator(),
				{
					.size = ext,
					.format = gpu::image_format::r8_unorm,
					.usage = gpu::image_flag::storage | gpu::image_flag::transfer_src,
				}
			);
			gpu::transition_image_to(*gpu_s.device, d.y_planes[i]);

			d.uv_planes[i] = gpu::image::create(
				gpu_s.device->allocator(),
				{
					.size = half_ext,
					.format = gpu::image_format::r8g8_unorm,
					.usage = gpu::image_flag::storage | gpu::image_flag::transfer_src,
				}
			);
			gpu::transition_image_to(*gpu_s.device, d.uv_planes[i]);

			d.convert_descriptors[i] =
				gpu::allocate_descriptors(
					*gpu_s.shader_registry,
					gpu_s.device->descriptor_heap(),
					entry::pod
				);

			gpu::descriptor_writer(
				gpu_s.device->handle(),
				d.convert_descriptors[i]
			)
				.storage_image<output_y>(d.y_planes[i])
				.storage_image<output_uv>(d.uv_planes[i])
				.commit();
		}

		d.encoder = gpu::video_encoder::create(*gpu_s.device, ext, caps);

		d.clip_ring.set_budget(d.ring_budget);
		d.applied_ring_budget = d.ring_budget;

		d.encode_active = true;
	}

	while (true) {
		const auto& action_state = actions::system::current_state(sys);

		if (actions::system::pressed(action_state, sys, d.screenshot_action)) {
			ctx.channels.push<screenshot_request>({});
		}
		if (actions::system::pressed(action_state, sys, d.save_clip_action)) {
			ctx.channels.push<save_clip_request>({});
		}

		co_await ctx.next_tick();
	}
}

auto gse::renderer::capture::system::frame(const frame_context& ctx, shared_view<gpu::context> gpu_s, data& d) -> async::task<> {
	const auto frame_index = gpu_s.render_graph->current_frame();

	auto& [staging, width, height, pending] = d.screenshots[frame_index];

	if (pending && !d.write_in_progress->load()) {
		pending = false;
		d.write_in_progress->store(true);

		const auto w = width;
		const auto h = height;
		const auto byte_count = static_cast<std::size_t>(w) * h * 4;
		std::vector<std::byte> pixels(byte_count);
		gse::memcpy(pixels.data(), staging.host_read().data(), byte_count);

		const bool needs_swizzle = gpu_s.swapchain->is_bgra();
		const auto timestamp = system_clock::timestamp_filename();

		task::post(
			[pixels = std::move(pixels),
			 w,
			 h,
			 needs_swizzle,
			 timestamp,
			 write_flag = d.write_in_progress.get()] mutable {
				for (std::size_t i = 0; i < pixels.size(); i += 4) {
					if (needs_swizzle) {
						std::swap(pixels[i], pixels[i + 2]);
					}
					pixels[i + 3] = std::byte{ 0xFF };
				}

				const auto path = config::resource_path / "Screenshots" / std::format(
																			  "screenshot_{}.png",
																			  timestamp
																		  );
				std::filesystem::create_directories(path.parent_path());

				image::write_png(path, w, h, 4, pixels.data());
				log::println(log::category::render, "Screenshot saved: {}", path.string());

				write_flag->store(false);
			},
			trace_id<"capture::screenshot_write">()
		);
	}

	if (d.ring_budget != d.applied_ring_budget) {
		d.clip_ring.set_budget(d.ring_budget);
		d.applied_ring_budget = d.ring_budget;
	}

	if (d.encode_active && d.encoder.valid()) {
		d.encoder.wait(frame_index);
		if (auto unit = d.encoder.read_bitstream(frame_index)) {
			const bool was_keyframe = unit->keyframe;
			const auto byte_count = unit->bytes.size();
			d.clip_ring.push(std::move(*unit));
			if (!d.first_ring_push_logged) {
				d.first_ring_push_logged = true;
				log::println(
					log::category::render,
					"First clip ring push: {} bytes, keyframe={}",
					byte_count,
					was_keyframe
				);
			}
		}
		d.encoder.encode_frame(frame_index, d.y_planes[frame_index], d.uv_planes[frame_index]);
	}

	if (!ctx.read_channel<save_clip_request>().empty()) {
		if (!d.encode_active || !d.encoder.valid()) {
			log::println(log::category::render, "Save Clip pressed but video capture is unavailable");
		}
		else if (d.clip_save_in_progress->load()) {
			log::println(log::category::render, "Clip save already in progress, ignoring request");
		}
		else {
			auto snapshot = d.clip_ring.snapshot_from_earliest_keyframe();
			if (snapshot.empty()) {
				log::println(log::category::render, "Clip ring has no keyframe yet, skipping save");
			}
			else {
				d.clip_save_in_progress->store(true);

				const auto path =
					config::resource_path / "Clips" / std::format(
														  "clip_{}.mp4",
														  system_clock::timestamp_filename()
													  );
				std::filesystem::create_directories(path.parent_path());

				task::post(
					[snap = std::move(snapshot),
					 codec = d.encoder.codec(),
					 extent = d.encoder.extent(),
					 path,
					 flag = d.clip_save_in_progress.get()] mutable {
						const auto ok = mp4::mux(snap, { codec, extent }, path);
						if (ok) {
							log::println(log::category::render, "Clip saved: {}", path.string());
						}
						else {
							log::println(
								log::level::warning,
								log::category::render,
								"Failed to mux clip to {}",
								path.string()
							);
						}
						flag->store(false);
					},
					trace_id<"capture::clip_mux">()
				);
			}
		}
	}

	if (!ctx.read_channel<screenshot_request>().empty() && !d.write_in_progress->load()) {
		const auto ext = gpu_s.render_graph->extent();

		if (const auto needed = static_cast<std::size_t>(ext.x()) * ext.y() * 4; !staging.valid() || staging.size() < needed) {
			staging = gpu::buffer::create(
				gpu_s.device->allocator(),
				{
					.size = needed,
					.usage = gpu::buffer_flag::transfer_dst,
				}
			);
		}

		width = ext.x();
		height = ext.y();
		d.screenshot_requested = true;
	}

	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const bool do_screenshot = d.screenshot_requested;
	const bool do_encode = d.encode_active;

	if (!do_screenshot && !do_encode) {
		co_return;
	}

	const auto ext = gpu_s.render_graph->extent();

	gpu::typed_push_constants<push_constants> convert_pc{
		.data = {
			.extent = ext,
			.rgba_index = d.rgba_slots[frame_index].valid() ? d.rgba_slots[frame_index].index : shaders::bindless::invalid_index,
		},
		.stages = gpu::stage_flag::compute,
	};

	auto rec = co_await gpu::pass<system>(ctx).after<ui::system>();

	if (do_screenshot) {
		rec.capture_swapchain(*gpu_s.swapchain, *gpu_s.frame, staging);
		pending = true;
		d.screenshot_requested = false;
	}

	if (do_encode) {
		const auto capture_extent = d.rgba_captures[frame_index].extent();
		rec.blit_swapchain_to_image(
			*gpu_s.swapchain,
			*gpu_s.frame,
			d.rgba_captures[frame_index],
			vec2u{ capture_extent.x(), capture_extent.y() }
		);
		rec.sample_image(d.rgba_captures[frame_index], gpu::pipeline_stage_flag::compute_shader);
		rec.bind(d.convert_pipeline);
		rec.bind_descriptors(d.convert_pipeline, d.convert_descriptors[frame_index]);
		rec.push(d.convert_pipeline, convert_pc);
		rec.dispatch((ext.x() + 15) / 16, (ext.y() + 15) / 16, 1);
	}
}

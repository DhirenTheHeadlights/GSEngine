export module gse.graphics:capture_renderer;

import std;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.log;
import gse.save;

import :ui_renderer;
import :capture_ring;
import :mp4_muxer;

export namespace gse::renderer::capture {
	struct pending_screenshot {
		gpu::buffer staging;
		std::uint32_t width = 0;
		std::uint32_t height = 0;
		bool pending = false;
	};

	struct screenshot_request {};
	struct save_clip_request {};

	struct state {
		actions::handle screenshot_action;
		actions::handle save_clip_action;
	};

	struct system {
		struct resources {
			gpu::pipeline convert_pipeline;
			resource::handle<shader> convert_shader;
			per_frame_resource<gpu::descriptor_region> convert_descriptors;
			per_frame_resource<gpu::image> rgba_captures;
			per_frame_resource<gpu::image> y_planes;
			per_frame_resource<gpu::image> uv_planes;
			gpu::sampler capture_sampler;
			bool encode_active = false;
		};

		struct frame_data {
			per_frame_resource<pending_screenshot> screenshots;
			bool screenshot_requested = false;
			std::unique_ptr<std::atomic<bool>> write_in_progress = std::make_unique<std::atomic<bool>>(false);
			std::unique_ptr<std::atomic<bool>> clip_save_in_progress = std::make_unique<std::atomic<bool>>(false);
			gpu::video_encoder encoder;
			ring clip_ring;
			time ring_budget = seconds(30.f);
			bool first_ring_push_logged = false;
		};

		static auto initialize(
			const init_context& phase,
			resources& r,
			frame_data& fd,
			state& s
		) -> void;

		static auto update(
			const update_context& ctx,
			state& s
		) -> void;

		static auto frame(
			const frame_context& ctx,
			const resources& r,
			frame_data& fd,
			const state& s
		) -> async::task<>;
	};
}

auto gse::renderer::capture::system::initialize(const init_context& phase, resources& r, frame_data& fd, state& s) -> void {
	const auto register_action = [&](const std::string_view name, const key default_key) -> actions::handle {
		const id action_id = generate_id(name);
		phase.channels.push<actions::add_action_request>({
			.name = std::string(name),
			.default_key = default_key,
			.action_id = action_id
		});
		return actions::handle(action_id);
	};

	s.screenshot_action = register_action("Screenshot", key::f9);
	s.save_clip_action = register_action("Save Clip", key::f10);

	auto& ctx = phase.get<gpu::context>();

	if (!ctx.device().video_encode_enabled()) {
		log::println(log::category::render, "Video encode not available, capture limited to screenshots");
		return;
	}

	const auto caps = gpu::video_encoder::probe(ctx.device());
	if (!caps.available) {
		log::println(log::category::render, "Video encode probe failed, capture limited to screenshots");
		return;
	}

	const auto ext = ctx.graph().extent();
	const auto half_ext = vec2u{ ext.x() / 2, ext.y() / 2 };

	r.convert_shader = ctx.get<shader>("Shaders/Compute/rgba_to_nv12");
	ctx.instantly_load(r.convert_shader);

	r.convert_pipeline = gpu::create_compute_pipeline(ctx, *r.convert_shader, "push_constants");

	r.capture_sampler = gpu::create_sampler(ctx, {
		.min = gpu::sampler_filter::nearest,
		.mag = gpu::sampler_filter::nearest,
		.address_u = gpu::sampler_address_mode::clamp_to_edge,
		.address_v = gpu::sampler_address_mode::clamp_to_edge,
		.address_w = gpu::sampler_address_mode::clamp_to_edge
	});

	for (std::size_t i = 0; i < per_frame_resource<gpu::image>::frames_in_flight; ++i) {
		r.rgba_captures[i] = gpu::create_image(ctx, {
			.size = ext,
			.format = gpu::image_format::r8g8b8a8_srgb,
			.usage = gpu::image_flag::sampled | gpu::image_flag::transfer_dst
		});

		r.y_planes[i] = gpu::create_image(ctx, {
			.size = ext,
			.format = gpu::image_format::r8_unorm,
			.usage = gpu::image_flag::storage | gpu::image_flag::transfer_src
		});

		r.uv_planes[i] = gpu::create_image(ctx, {
			.size = half_ext,
			.format = gpu::image_format::r8g8_unorm,
			.usage = gpu::image_flag::storage | gpu::image_flag::transfer_src
		});

		r.convert_descriptors[i] = gpu::allocate_descriptors(ctx, *r.convert_shader);

		gpu::descriptor_writer(ctx, r.convert_shader, r.convert_descriptors[i])
			.image("input_rgba", r.rgba_captures[i], r.capture_sampler, gpu::image_layout::shader_read_only)
			.storage_image("output_y", r.y_planes[i])
			.storage_image("output_uv", r.uv_planes[i])
			.commit();
	}

	fd.encoder = gpu::video_encoder::create(ctx.device(), ext, caps);

	phase.channels.push(save::make_property_registration(
		"Graphics",
		"Clip Ring Buffer Length",
		"How many seconds of gameplay to hold in the clip ring buffer.",
		fd.ring_budget
	));
	fd.clip_ring.set_budget(fd.ring_budget);

	r.encode_active = true;
}

auto gse::renderer::capture::system::update(const update_context& ctx, state& s) -> void {
	const auto* sys = ctx.try_state_of<actions::system_state>();
	if (!sys) {
		return;
	}

	const auto& action_state = sys->current_state();

	if (s.screenshot_action.pressed(action_state, *sys)) {
		ctx.channels.push<screenshot_request>({});
	}
	if (s.save_clip_action.pressed(action_state, *sys)) {
		ctx.channels.push<save_clip_request>({});
	}
}

auto gse::renderer::capture::system::frame(const frame_context& ctx, const resources& r, frame_data& fd, const state& s) -> async::task<> {
	auto* gpu_ctx = ctx.try_get<gpu::context>();
	if (!gpu_ctx) co_return;

	const auto frame_index = gpu_ctx->graph().current_frame();

	auto& [staging, width, height, pending] = fd.screenshots[frame_index];

	if (pending && !fd.write_in_progress->load()) {
		pending = false;
		fd.write_in_progress->store(true);

		const auto w = width;
		const auto h = height;
		const auto byte_count = static_cast<std::size_t>(w) * h * 4;
		std::vector<std::byte> pixels(byte_count);
		gse::memcpy(pixels.data(), staging.mapped(), byte_count);

		const bool needs_swizzle = gpu_ctx->swapchain().is_bgra();
		const auto timestamp = system_clock::timestamp_filename();

		task::post([pixels = std::move(pixels), w, h, needs_swizzle, timestamp, write_flag = fd.write_in_progress.get()] mutable {
			for (std::size_t i = 0; i < pixels.size(); i += 4) {
				if (needs_swizzle) {
					std::swap(pixels[i], pixels[i + 2]);
				}
				pixels[i + 3] = std::byte{ 0xFF };
			}

			const auto path = config::resource_path / "Screenshots" / std::format("screenshot_{}.png", timestamp);
			std::filesystem::create_directories(path.parent_path());

			image::write_png(path, w, h, 4, pixels.data());
			log::println(log::category::render, "Screenshot saved: {}", path.string());

			write_flag->store(false);
		});
	}

	if (fd.ring_budget != fd.clip_ring.budget()) {
		fd.clip_ring.set_budget(fd.ring_budget);
	}

	if (r.encode_active && fd.encoder) {
		fd.encoder.wait(frame_index);
		if (auto unit = fd.encoder.read_bitstream(frame_index)) {
			const bool was_keyframe = unit->keyframe;
			const auto byte_count = unit->bytes.size();
			fd.clip_ring.push(std::move(*unit));
			if (!fd.first_ring_push_logged) {
				fd.first_ring_push_logged = true;
				log::println(
					log::category::render,
					"First clip ring push: {} bytes, keyframe={}",
					byte_count,
					was_keyframe
				);
			}
		}
		fd.encoder.encode_frame(frame_index, r.y_planes[frame_index], r.uv_planes[frame_index]);
	}

	if (!ctx.read_channel<save_clip_request>().empty()) {
		if (!r.encode_active || !fd.encoder) {
			log::println(log::category::render, "Save Clip pressed but video capture is unavailable");
		}
		else if (fd.clip_save_in_progress->load()) {
			log::println(log::category::render, "Clip save already in progress, ignoring request");
		}
		else {
			auto snapshot = fd.clip_ring.snapshot_from_earliest_keyframe();
			if (snapshot.empty()) {
				log::println(log::category::render, "Clip ring has no keyframe yet, skipping save");
			}
			else {
				fd.clip_save_in_progress->store(true);

				const auto path = config::resource_path / "Clips" / std::format("clip_{}.mp4", system_clock::timestamp_filename());
				std::filesystem::create_directories(path.parent_path());

				task::post([
					snap = std::move(snapshot),
					codec = fd.encoder.codec(),
					extent = fd.encoder.extent(),
					path,
					flag = fd.clip_save_in_progress.get()
				] mutable {
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
				});
			}
		}
	}

	if (!ctx.read_channel<screenshot_request>().empty() && !fd.write_in_progress->load()) {
		const auto ext = gpu_ctx->graph().extent();

		if (const auto needed = static_cast<std::size_t>(ext.x()) * ext.y() * 4; !staging || staging.size() < needed) {
			staging = gpu::create_buffer(*gpu_ctx, {
				.size = needed,
				.usage = gpu::buffer_flag::transfer_dst
			});
		}

		width = ext.x();
		height = ext.y();
		fd.screenshot_requested = true;
	}

	if (!gpu_ctx->graph().frame_in_progress()) co_return;

	if (fd.screenshot_requested) {
		gpu_ctx->graph().add_pass<pending_screenshot>()
			.after<ui::state>()
			.record([&swapchain = gpu_ctx->swapchain(), &frame = gpu_ctx->frame(), &staging](const gpu::recording_context& rec) {
				rec.capture_swapchain(swapchain, frame, staging);
			});

		fd.screenshots[frame_index].pending = true;
		fd.screenshot_requested = false;
	}

	if (r.encode_active) {
		const auto ext = gpu_ctx->graph().extent();

		gpu_ctx->graph().add_pass<pending_screenshot>()
			.after<ui::state>()
			.record([&swapchain = gpu_ctx->swapchain(), &frame = gpu_ctx->frame(), &rgba = r.rgba_captures[frame_index], rgba_extent = r.rgba_captures[frame_index].extent()](const gpu::recording_context& rec) {
				rec.blit_swapchain_to_image(swapchain, frame, rgba, rgba_extent);
			});

		auto pc = r.convert_shader->cache_push_block("push_constants");
		pc.set("extent", ext);

		gpu_ctx->graph().add_pass<state>()
			.after<pending_screenshot>()
			.record([&r, frame_index, ext, pc = std::move(pc)](const gpu::recording_context& rec) {
				rec.bind(r.convert_pipeline);
				rec.bind_descriptors(r.convert_pipeline, r.convert_descriptors[frame_index]);
				rec.push(r.convert_pipeline, pc);
				rec.dispatch((ext.x() + 15) / 16, (ext.y() + 15) / 16, 1);
			});
	}
}

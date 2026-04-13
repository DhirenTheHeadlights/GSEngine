export module gse.graphics:capture_renderer;

import std;

import gse.platform;
import gse.utility;
import gse.math;
import gse.log;
import :ui_renderer;

export namespace gse::renderer::capture {
	struct pending_screenshot {
		gpu::buffer staging;
		std::uint32_t width = 0;
		std::uint32_t height = 0;
		bool pending = false;
	};

	struct encode_resources {
		gpu::pipeline convert_pipeline;
		resource::handle<shader> convert_shader;
		per_frame_resource<gpu::descriptor_region> convert_descriptors;
		per_frame_resource<gpu::image> rgba_captures;
		per_frame_resource<gpu::image> y_planes;
		per_frame_resource<gpu::image> uv_planes;
		gpu::sampler capture_sampler;
		gpu::video_encoder encoder;
		bool active = false;
	};

	struct state {
		per_frame_resource<pending_screenshot> screenshots;
		bool screenshot_requested = false;
		std::unique_ptr<std::atomic<bool>> write_in_progress = std::make_unique<std::atomic<bool>>(false);

		encode_resources encode;
	};

	struct system {
		static auto initialize(const initialize_phase& phase, state& s) -> void;
		static auto prepare_render(const prepare_render_phase& phase, state& s) -> void;
		static auto render(const render_phase& phase, const state& s) -> void;
		static auto end_frame(const end_frame_phase& phase, state& s) -> void;
	};
}

auto gse::renderer::capture::system::initialize(const initialize_phase& phase, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();

	if (!ctx.device_ref().video_encode_enabled()) {
		log::println(log::category::render, "Video encode not available, capture limited to screenshots");
		return;
	}

	const auto ext = ctx.graph().extent();
	const auto half_ext = vec2u{ ext.x() / 2, ext.y() / 2 };

	s.encode.convert_shader = ctx.get<shader>("Shaders/Compute/rgba_to_nv12");
	ctx.instantly_load(s.encode.convert_shader);

	s.encode.convert_pipeline = gpu::create_compute_pipeline(ctx.device_ref(), *s.encode.convert_shader, "push_constants");

	s.encode.capture_sampler = gpu::create_sampler(ctx.device_ref(), {
		.min = gpu::sampler_filter::nearest,
		.mag = gpu::sampler_filter::nearest,
		.address_u = gpu::sampler_address_mode::clamp_to_edge,
		.address_v = gpu::sampler_address_mode::clamp_to_edge,
		.address_w = gpu::sampler_address_mode::clamp_to_edge
	});

	for (std::size_t i = 0; i < per_frame_resource<gpu::image>::frames_in_flight; ++i) {
		s.encode.rgba_captures[i] = gpu::create_image(ctx.device_ref(), {
			.size = ext,
			.format = gpu::image_format::r8g8b8a8_srgb,
			.usage = gpu::image_flag::sampled | gpu::image_flag::transfer_dst
		});

		s.encode.y_planes[i] = gpu::create_image(ctx.device_ref(), {
			.size = ext,
			.format = gpu::image_format::r8_unorm,
			.usage = gpu::image_flag::storage | gpu::image_flag::transfer_src
		});

		s.encode.uv_planes[i] = gpu::create_image(ctx.device_ref(), {
			.size = half_ext,
			.format = gpu::image_format::r8g8_unorm,
			.usage = gpu::image_flag::storage | gpu::image_flag::transfer_src
		});

		s.encode.convert_descriptors[i] = gpu::allocate_descriptors(ctx.device_ref(), *s.encode.convert_shader);

		gpu::descriptor_writer(ctx.device_ref(), s.encode.convert_shader, s.encode.convert_descriptors[i])
			.image("input_rgba", s.encode.rgba_captures[i], s.encode.capture_sampler, gpu::image_layout::shader_read_only)
			.storage_image("output_y", s.encode.y_planes[i])
			.storage_image("output_uv", s.encode.uv_planes[i])
			.commit();
	}

	const auto caps = gpu::video_encoder::probe(ctx.device_ref());
	if (!caps.available) {
		log::println(log::category::render, "Capture color convert initialized but no encoder available");
		s.encode.active = true;
		return;
	}

	const auto cap_ext = vec2u{
		std::min(ext.x(), caps.max_extent.x()),
		std::min(ext.y(), caps.max_extent.y())
	};

	s.encode.encoder = gpu::video_encoder::create(ctx.device_ref(), cap_ext, caps.codec);
	s.encode.active = true;
	log::println(log::category::render, "Capture encode pipeline initialized ({}x{})", ext.x(), ext.y());
}

auto gse::renderer::capture::system::prepare_render(const prepare_render_phase& phase, state& s) -> void {
	auto* ctx = phase.try_get<gpu::context>();
	if (!ctx) return;

	const auto frame_index = ctx->graph().current_frame();
	auto& [staging, width, height, pending] = s.screenshots[frame_index];

	if (pending && !s.write_in_progress->load()) {
		pending = false;
		s.write_in_progress->store(true);

		const auto w = width;
		const auto h = height;
		const auto byte_count = static_cast<std::size_t>(w) * h * 4;
		std::vector<std::byte> pixels(byte_count);
		gse::memcpy(pixels.data(), staging.mapped(), byte_count);

		const bool needs_swizzle = ctx->swapchain().is_bgra();

		const auto timestamp = system_clock::timestamp_filename();

		task::post([pixels = std::move(pixels), w, h, needs_swizzle, timestamp, write_flag = s.write_in_progress.get()] mutable {
			for (std::size_t i = 0; i < pixels.size(); i += 4) {
				if (needs_swizzle) std::swap(pixels[i], pixels[i + 2]);
				pixels[i + 3] = std::byte{0xFF};
			}

			const auto path = config::resource_path / "Screenshots" / std::format("screenshot_{}.png", timestamp);
			std::filesystem::create_directories(path.parent_path());

			image::write_png(path, w, h, 4, pixels.data());
			log::println(log::category::render, "Screenshot saved: {}", path.string());

			write_flag->store(false);
		});
	}

	if (s.encode.active && s.encode.encoder) {
		s.encode.encoder.wait(frame_index);
		s.encode.encoder.encode_frame(frame_index, s.encode.y_planes[frame_index], s.encode.uv_planes[frame_index]);
	}

	if (const auto* input = phase.try_state_of<input::system_state>(); input->current_state().key_pressed(key::f9) && !s.write_in_progress->load()) {
		const auto ext = ctx->graph().extent();

		if (const auto needed = static_cast<std::size_t>(ext.x()) * ext.y() * 4; !staging || staging.size() < needed) {
			staging = gpu::create_buffer(ctx->device_ref(), {
				.size = needed,
				.usage = gpu::buffer_flag::transfer_dst
			});
		}

		width = ext.x();
		height = ext.y();
		s.screenshot_requested = true;
	}
}

auto gse::renderer::capture::system::render(const render_phase& phase, const state& s) -> void {
	auto* ctx = phase.try_get<gpu::context>();
	if (!ctx || !ctx->graph().frame_in_progress()) return;

	const auto frame_index = ctx->graph().current_frame();

	if (s.screenshot_requested) {
		const auto& staging = s.screenshots[frame_index].staging;

		ctx->graph().add_pass<pending_screenshot>()
			.after<ui::state>()
			.record([&swapchain = ctx->swapchain(), &frame = ctx->frame(), &staging](const gpu::recording_context& rec) {
				rec.capture_swapchain(swapchain, frame, staging);
			});
	}

	if (s.encode.active) {
		const auto ext = ctx->graph().extent();

		ctx->graph().add_pass<encode_resources>()
			.after<ui::state>()
			.record([&swapchain = ctx->swapchain(), &frame = ctx->frame(), &rgba = s.encode.rgba_captures[frame_index]](const gpu::recording_context& rec) {
				rec.blit_swapchain_to_image(swapchain, frame, rgba);
			});

		auto pc = s.encode.convert_shader->cache_push_block("push_constants");
		pc.set("extent", std::array{ ext.x(), ext.y() });

		ctx->graph().add_pass<state>()
			.after<encode_resources>()
			.record([&s, frame_index, ext, pc = std::move(pc)](const gpu::recording_context& rec) {
				rec.bind(s.encode.convert_pipeline);
				rec.bind_descriptors(s.encode.convert_pipeline, s.encode.convert_descriptors[frame_index]);
				rec.push(s.encode.convert_pipeline, pc);
				rec.dispatch((ext.x() + 15) / 16, (ext.y() + 15) / 16, 1);
			});
	}
}

auto gse::renderer::capture::system::end_frame(const end_frame_phase& phase, state& s) -> void {
	if (!s.screenshot_requested) return;

	const auto* ctx = phase.try_get<gpu::context>();
	if (!ctx) return;

	const auto frame_index = ctx->graph().current_frame();
	s.screenshots[frame_index].pending = true;
	s.screenshot_requested = false;
}

module gse.graphics:taa_renderer_impl;

import std;

import :taa_renderer;
import :depth_prepass_renderer;
import :forward_renderer;
import :physics_debug_renderer;
import :render_targets;
import :sdf_grid_renderer;
import :sky_renderer;
import :world_text_renderer;


import gse.gpu;
import gse.gpu_record;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.meta;

namespace gse::renderer::taa {
	struct taa_bindings {
		shaders::texture2d<vec4f> hdr_color;
		shaders::texture2d<vec4f> velocity_color;
		shaders::texture2d<vec4f> history_color;
		shaders::sampler_state color_sampler;
	};

	struct [[= shaders::shader_struct]] push_constants {
		float blend_alpha;
		std::uint32_t taa_enabled;
		vec2f inv_allocated_extent;
		vec2f screen_uv_scale;
		vec2f screen_uv_max;
		vec2f active_extent;
		float motion_blend_alpha;
		float motion_full_pixels;
	};

	constexpr float motion_blend_alpha = 0.4f;
	constexpr float motion_full_pixels = 16.0f;

	using entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/Taa">,
		gpu::bindings<taa_bindings>,
		gpu::fullscreen_pass,
		gpu::helpers<"Screen/screen_target">,
		gpu::push_constant<push_constants>,
		gpu::color_targets<gpu::color_format::hdr, gpu::color_format::hdr>
	>;

	auto recreate_history(
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> void;

	auto rebind_views(
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> void;
}

auto gse::renderer::taa::recreate_history(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
	const auto extent = gpu_s.render_graph->allocated_extent();
	const auto active = gpu_s.render_graph->extent();
	if (active != d.history_active_extent) {
		d.history_active_extent = active;
		d.frames_since_history_invalid = 0;
	}
	if (d.history.front().handle() && d.history.front().extent() == extent) {
		return;
	}
	d.frames_since_history_invalid = 0;
	if (extent.x() == 0 || extent.y() == 0) {
		for (auto& image : d.history) {
			image = {};
		}
		return;
	}
	for (std::size_t i = 0; i < d.history.size(); ++i) {
		d.history[i] = gpu_s.device->create_image(
			{
				.size = extent,
				.format = gpu::image_format::r16g16b16a16_sfloat,
				.usage = { gpu::image_flag::color_attachment, gpu::image_flag::sampled },
			},
			std::format("taa_history_{}", i)
		);
		gpu::transition_image_to(*gpu_s.device, d.history[i]);
	}
}

auto gse::renderer::taa::rebind_views(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
	auto& hdr = gpu_s.render_graph->framebuffer_image<targets::hdr_color>();
	if (hdr.handle()) {
		if (!d.hdr_view.valid()) {
			d.hdr_view = gpu_s.device->allocate_image_slot();
		}
		gpu_s.device->write_sampled_image(d.hdr_view.slot(), hdr);
	}
	auto& velocity = gpu_s.render_graph->framebuffer_image<targets::velocity>();
	if (velocity.handle()) {
		if (!d.velocity_view.valid()) {
			d.velocity_view = gpu_s.device->allocate_image_slot();
		}
		gpu_s.device->write_sampled_image(d.velocity_view.slot(), velocity);
	}
}

auto gse::renderer::taa::init(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	d.pipeline = gpu::build_graphics_program(*gpu_s.device, entry::pod);

	d.sampler = gpu_s.device->register_sampler(gpu::linear_clamp_sampler);

	recreate_history(gpu_s, d);
	rebind_views(gpu_s, d);

	gpu_s.render_graph->on_resize(
		[gpu_s, &d]() {
			recreate_history(gpu_s, d);
			rebind_views(gpu_s, d);
		}
	);

	return {};
}

auto gse::renderer::taa::frame(const context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out, const channel_write<camera::jitter_request> jitter_out, const channel_read<camera::temporal_reset_request> reset_in) -> async::task<> {
	jitter_out.push<camera::jitter_request>({
		.enabled = d.taa_enabled,
	});

	if (!reset_in.of<camera::temporal_reset_request>().empty()) {
		d.frames_since_history_invalid = 0;
	}

	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto& hdr = gpu_s.render_graph->framebuffer_image<targets::hdr_color>();
	if (!hdr.handle()) {
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();
	if (!d.history[frame_index].handle()) {
		co_return;
	}

	const auto ext = gpu_s.render_graph->extent();
	const auto allocated = gpu_s.render_graph->allocated_extent();
	const bool history_ready = d.taa_enabled && d.frames_since_history_invalid >= 2;
	++d.frames_since_history_invalid;

	auto rec = co_await gpu::pass<^^frame>(pass_out)
		.pipeline(d.pipeline)
		.color(gpu::clear_color(
			gpu::color_clear{ 0.0f, 0.0f, 0.0f, 1.0f },
			d.history[frame_index]
		))
		.color(gpu::clear_color(
			gpu::color_clear{ 0.0f, 0.0f, 0.0f, 1.0f },
			gpu_s.render_graph->framebuffer_image<targets::post_taa_color>()
		))
		.after<^^forward::frame, ^^sky::frame, ^^physics_debug::frame, ^^sdf_grid::frame, ^^world_text::frame, ^^depth_prepass::frame>();

	rec.set_viewport(ext);
	rec.set_scissor(ext);
	rec.push_bindings<entry>(
		{
			.blend_alpha = d.blend_alpha,
			.taa_enabled = history_ready ? 1u : 0u,
			.inv_allocated_extent = vec2f{ 1.0f / static_cast<float>(allocated.x()), 1.0f / static_cast<float>(allocated.y()) },
			.screen_uv_scale = gpu_s.render_graph->screen_uv_scale(),
			.screen_uv_max = gpu_s.render_graph->screen_uv_max(),
			.active_extent = vec2f{ static_cast<float>(ext.x()), static_cast<float>(ext.y()) },
			.motion_blend_alpha = std::max(d.blend_alpha, motion_blend_alpha),
			.motion_full_pixels = motion_full_pixels,
		},
		{
			.hdr_color = d.hdr_view,
			.velocity_color = d.velocity_view,
			.history_color = d.history[1u - frame_index],
			.color_sampler = d.sampler,
		}
	);
	rec.draw(3);
}
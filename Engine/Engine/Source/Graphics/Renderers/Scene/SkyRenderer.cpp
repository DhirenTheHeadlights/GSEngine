module gse.graphics:sky_renderer_impl;

import std;

import :sky_renderer;
import :atmosphere_renderer;
import :camera_latch_renderer;
import :camera_system;
import :shared_shaders;
import :cloud_renderer;
import :forward_renderer;
import :render_targets;

import gse.gpu;
import gse.gpu_record;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.meta;

namespace gse::renderer::sky {
	struct sky_bindings {
		shaders::texture2d<vec4f> transmittance_in;
		shaders::sampler_state lut_sampler;
		shaders::texture2d<vec4f> cloud_layer_in;
	};

	struct [[= shaders::shader_struct]]
	push_constants {
		vec3f sun_direction;
		atmosphere::atmosphere_length camera_altitude;
		vec3<irradiance> sun_irradiance;
		float sun_cos_radius;
		vec2f cloud_layer_uv_scale;
		vec2f cloud_layer_uv_max;
		float star_intensity;
		float star_density;
		float star_fade;
		std::uint32_t cloud_layer_rendered;
	};

	using entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/Sky">,
		gpu::types<shaders::common::shader_types>,
		gpu::bindings<sky_bindings, shaders::standard_3d::camera, shaders::standard_3d::sky_view, atmosphere::bindings>,
		gpu::helpers<"Atmosphere/atmosphere_common", "Screen/screen_target">,
		gpu::fullscreen_pass,
		gpu::push_constant<push_constants>,
		gpu::color_targets<gpu::color_format::hdr>,
		gpu::depth<true, false, gpu::compare_op::equal>
	>;
}

auto gse::renderer::sky::init(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	d.pipeline = gpu::build_graphics_program(*gpu_s.device, entry::pod);
	return {};
}

auto gse::renderer::sky::frame(const context& ctx, shared_view<gpu::context::data> gpu_s, const data& d, const channel_write<gpu::render_pass_request> pass_out, shared_view<atmosphere::data> atm_state, shared_view<cloud::data> cloud_state, shared_view<camera::data> cam_state, shared_view<camera_latch::data> latch_s) -> async::task<> {
	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	if (!atm_state.enabled || !cam_state.world_visible || !atm_state.atmosphere_ubo_buffer.valid()) {
		co_return;
	}

	const auto ext = gpu_s.render_graph->extent();
	const auto& layer = cloud_state.layer;

	auto rec = co_await gpu::pass<^^frame>(pass_out)
		.pipeline(d.pipeline)
		.color(gpu::load_color(gpu_s.render_graph->framebuffer_image<targets::hdr_color>()))
		.depth(gpu::load_depth())
		.after<^^camera_latch::frame, ^^forward::frame, ^^atmosphere::sky_view_pass, ^^cloud::cloud_resolve_pass>();

	rec.set_viewport(ext);
	rec.set_scissor(ext);
	rec.push_bindings<entry>(
		{
			.sun_direction = atm_state.sun_direction,
			.camera_altitude = atm_state.camera_altitude,
			.sun_irradiance = {
				atm_state.sun_intensity * atm_state.sun_color.x(),
				atm_state.sun_intensity * atm_state.sun_color.y(),
				atm_state.sun_intensity * atm_state.sun_color.z(),
			},
			.sun_cos_radius = cos(d.sun_angular_radius),
			.cloud_layer_uv_scale = gpu::screen_uv_scale_for(layer.extent, layer.allocated_extent),
			.cloud_layer_uv_max = gpu::screen_uv_max_for(layer.extent, layer.allocated_extent),
			.star_intensity = d.star_intensity,
			.star_density = d.star_density,
			.star_fade = d.star_fade,
			.cloud_layer_rendered = layer.rendered ? 1u : 0u,
		},
		{
			.transmittance_in = atm_state.transmittance_lut,
			.lut_sampler = atm_state.lut_sampler_bindless,
			.cloud_layer_in = layer.images[gpu_s.render_graph->current_frame()],
			.camera_ubo = latch_s.camera_buffers[gpu_s.render_graph->current_frame()],
			.sky_view_in = atm_state.sky_view_lut,
			.sky_view_sampler = atm_state.sky_view_sampler_bindless,
			.atmosphere_ubo = atm_state.atmosphere_ubo_buffer,
		}
	);
	rec.draw(3);
}

module gse.graphics:tonemap_renderer_impl;

import std;

import :tonemap_renderer;
import :bloom_renderer;
import :depth_prepass_renderer;
import :forward_renderer;
import :physics_debug_renderer;
import :render_targets;
import :sdf_grid_renderer;
import :taa_renderer;
import :world_text_renderer;


import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.meta;

namespace gse::renderer::tonemap {
	struct [[
		= shaders::binding<0, 0>{},
		= shaders::texture2d
	]] hdr_color {
		using element = vec4f;
	};

	struct [[
		= shaders::binding<0, 1>{},
		= shaders::texture2d
	]] bloom_color {
		using element = vec4f;
	};

	struct [[
		= shaders::binding<0, 2>{},
		= shaders::texture2d
	]] velocity_color {
		using element = vec4f;
	};

	struct [[
		= shaders::binding<0, 3>{},
		= shaders::sampler_state
	]] color_sampler {};

	struct [[= shaders::shader_struct]] push_constants {
		float exposure;
		float bloom_intensity;
		std::uint32_t show_velocity;
	};

	using shader_binding_types = type_pack<hdr_color, bloom_color, velocity_color, color_sampler>;

	using entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/Tonemap">,
		gpu::bindings<shader_binding_types>,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::depth<false, false>,
		gpu::depth_target<gpu::depth_format::none>
	>;

	auto rebind_views(
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> void;
}

auto gse::renderer::tonemap::rebind_views(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
	auto& hdr = gpu_s.render_graph->framebuffer_image<targets::post_taa_color>();
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

auto gse::renderer::tonemap::init(context& ctx, const shared_view<gpu::context::data> gpu_s, const shared_view<bloom::data> bloom_state, data& d) -> async::task<> {
	d.pipeline = gpu::build_graphics_program(*gpu_s.device, entry::pod);

	d.sampler = gpu_s.device->register_sampler(
		{
			.min = gpu::sampler_filter::linear,
			.mag = gpu::sampler_filter::linear,
			.address_u = gpu::sampler_address_mode::clamp_to_edge,
			.address_v = gpu::sampler_address_mode::clamp_to_edge,
			.address_w = gpu::sampler_address_mode::clamp_to_edge,
		}
	);

	rebind_views(gpu_s, d);

	gpu::context::on_swap_chain_recreate(
		gpu_s,
		[gpu_s, &d]() {
			rebind_views(gpu_s, d);
		}
	);

	return {};
}

auto gse::renderer::tonemap::frame(const context& ctx, shared_view<gpu::context::data> gpu_s, data& d, shared_view<bloom::data> bloom_state) -> async::task<> {
	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto& hdr = gpu_s.render_graph->framebuffer_image<targets::post_taa_color>();
	if (!hdr.handle()) {
		co_return;
	}

	const auto ext = gpu_s.render_graph->extent();

	const bool bloom_active = bloom_state.bloom_quality != bloom::quality_level::off && bloom_state.active_mip_count > 0;

	const auto bloom_slot = bloom_active ? bloom_state.mips_up[0].sampled_slot() : d.hdr_view.slot();

	auto rec = co_await gpu::pass<^^gse::renderer::tonemap::frame>(ctx)
		.pipeline(d.pipeline)
		.color(gpu::load_color())
		.after<^^forward::frame, ^^physics_debug::frame, ^^sdf_grid::frame, ^^world_text::frame, ^^bloom::downsample_pass, ^^bloom::upsample_pass, ^^depth_prepass::frame, ^^taa::frame>();

	rec.sample_image(hdr, gpu::pipeline_stage_flag::fragment_shader);
	if (bloom_active) {
		rec.sample_image(bloom_state.mips_up[0], gpu::pipeline_stage_flag::fragment_shader);
	}
	if (d.show_velocity) {
		rec.sample_image(
			gpu_s.render_graph->framebuffer_image<targets::velocity>(),
			gpu::pipeline_stage_flag::fragment_shader
		);
	}
	rec.set_viewport(ext);
	rec.set_scissor(ext);
	rec.push_bindings<entry>(
		{
			.exposure = d.exposure,
			.bloom_intensity = bloom_active ? bloom_state.bloom_intensity : 0.0f,
			.show_velocity = d.show_velocity ? 1u : 0u,
		},
		{
			.hdr_color = d.hdr_view.slot(),
			.bloom_color = bloom_slot,
			.velocity_color = d.velocity_view.slot(),
			.color_sampler = d.sampler.slot(),
		}
	);
	rec.draw(3);
}

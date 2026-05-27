module gse.graphics;

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
		= shaders::sampler2d
	]] hdr_color {};

	struct [[
		= shaders::binding<0, 1>{},
		= shaders::sampler2d
	]] bloom_color {};

	struct [[
		= shaders::binding<0, 2>{},
		= shaders::sampler2d
	]] velocity_color {};

	struct [[= shaders::shader_struct]] push_constants {
		float exposure;
		float bloom_intensity;
		std::uint32_t show_velocity;
	};

	using shader_binding_types = type_pack<hdr_color, bloom_color, velocity_color>;

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
		const gpu::context::data& gpu_s,
		system::data& d
	) -> void;
}

auto gse::renderer::tonemap::rebind_views(const gpu::context::data& gpu_s, system::data& d) -> void {
	auto& hdr = gpu_s.render_graph->framebuffer_image<targets::post_taa_color>();
	if (hdr.handle()) {
		d.hdr_view.rebind_sampled(*gpu_s.bindless_heaps, hdr);
	}
	auto& velocity = gpu_s.render_graph->framebuffer_image<targets::velocity>();
	if (velocity.handle()) {
		d.velocity_view.rebind_sampled(*gpu_s.bindless_heaps, velocity);
	}
}

auto gse::renderer::tonemap::system::run(run_context& ctx, const gpu::context::data& gpu_s, const bloom::system::data& bloom_state, data& d) -> async::task<> {
	d.pipeline =
		gpu::build_graphics_program(
			*gpu_s.device,
			*gpu_s.bindless_heaps,
			entry::pod
		);

	d.sampler = gpu::bindless_sampler::create(
		*gpu_s.bindless_heaps,
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
		[&gpu_s, &d]() {
			rebind_views(gpu_s, d);
		}
	);

	co_return;
}

auto gse::renderer::tonemap::system::frame(const frame_context& ctx, shared_view<gpu::context> gpu_s, data& d, shared_view<bloom::system> bloom_state) -> async::task<> {
	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto& hdr = gpu_s.render_graph->framebuffer_image<targets::post_taa_color>();
	if (!hdr.handle()) {
		co_return;
	}

	const auto ext = gpu_s.render_graph->extent();

	const bool bloom_active =
		bloom_state.bloom_quality != bloom::quality_level::off && bloom_state.active_mip_count > 0;

	const auto bloom_slot = bloom_active
		? bloom_state.mips_up[0].sampled_slot()
		: d.hdr_view.sampled_slot();

	auto rec = co_await gpu::pass<system>(ctx)
		.pipeline(d.pipeline)
		.color(gpu::clear_color(gpu::color_clear{ 0.0f, 0.0f, 0.0f, 1.0f }))
		.after<forward::system, physics_debug::system, sdf_grid::system, world_text::system, bloom::downsample_pass, bloom::upsample_pass, depth_prepass::system, taa::system>();

	rec.sample_image(hdr, gpu::pipeline_stage_flag::fragment_shader);
	if (bloom_active) {
		rec.sample_image(bloom_state.mips_up[0].image(), gpu::pipeline_stage_flag::fragment_shader);
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
			.hdr_color = { d.hdr_view.sampled_slot(), d.sampler.slot() },
			.bloom_color = { bloom_slot, d.sampler.slot() },
			.velocity_color = { d.velocity_view.sampled_slot(), d.sampler.slot() },
		}
	);
	rec.draw(3);
}

module gse.graphics;

import std;

import :tonemap_renderer;
import :bloom_renderer;
import :forward_renderer;
import :physics_debug_renderer;
import :render_targets;
import :sdf_grid_renderer;
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

	struct [[= shaders::shader_struct]] push_constants {
		float exposure;
		float bloom_intensity;
	};

	using shader_binding_types = type_pack<hdr_color, bloom_color>;

	using entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/Tonemap">,
		gpu::layout<"tonemap">,
		gpu::bindings<shader_binding_types>,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::depth<false, false>,
		gpu::depth_target<gpu::depth_format::none>
	>;

	auto rewrite_descriptors(
		const gpu::context::data& gpu_s,
		const bloom::system::data& bloom_state,
		system::data& d
	) -> void;
}

auto gse::renderer::tonemap::rewrite_descriptors(const gpu::context::data& gpu_s, const bloom::system::data& bloom_state, system::data& d) -> void {
	auto& hdr = gpu_s.render_graph->framebuffer_image<targets::hdr_color>();
	if (!hdr.handle()) {
		return;
	}
	const auto& bloom_source = bloom_state.active_mip_count > 0 ? bloom_state.mips_down[0] : hdr;
	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		gpu::descriptor_writer(gpu::context::device_handle(*gpu_s.device), d.descriptors[i])
			.combined_image_sampler<hdr_color>(hdr, d.sampler)
			.combined_image_sampler<bloom_color>(bloom_source, d.sampler)
			.commit();
	}
}

auto gse::renderer::tonemap::system::run(run_context& ctx, const gpu::context::data& gpu_s, const bloom::system::data& bloom_state, data& d) -> async::task<> {
	d.pipeline =
		gpu::build_graphics_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, entry::pod);

	d.sampler = gpu::sampler::create(
		gpu_s.device->allocator(),
		{
			.min = gpu::sampler_filter::linear,
			.mag = gpu::sampler_filter::linear,
			.address_u = gpu::sampler_address_mode::clamp_to_edge,
			.address_v = gpu::sampler_address_mode::clamp_to_edge,
			.address_w = gpu::sampler_address_mode::clamp_to_edge,
		}
	);

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		d.descriptors[i] =
			gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), entry::pod);
	}

	rewrite_descriptors(gpu_s, bloom_state, d);

	gpu::context::on_swap_chain_recreate(gpu_s, [&gpu_s, &bloom_state, &d]() {
		rewrite_descriptors(gpu_s, bloom_state, d);
	});

	co_return;
}

auto gse::renderer::tonemap::system::frame(const frame_context& ctx, shared_view<gpu::context> gpu_s, data& d, shared_view<bloom::system> bloom_state) -> async::task<> {
	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto& hdr = gpu_s.render_graph->framebuffer_image<targets::hdr_color>();
	if (!hdr.handle()) {
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();
	const auto ext = gpu_s.render_graph->extent();

	const bool bloom_active =
		bloom_state.bloom_quality != bloom::quality_level::off && bloom_state.active_mip_count > 0;

	const gpu::typed_push_constants<push_constants> pc{
		.data = {
			.exposure = d.exposure,
			.bloom_intensity = bloom_active ? bloom_state.bloom_intensity : 0.0f,
		},
		.stages = gpu::stage_flag::vertex | gpu::stage_flag::fragment,
	};

	auto rec = co_await gpu::pass<system>(ctx)
		.pipeline(d.pipeline)
		.color(gpu::clear_color(gpu::color_clear{ 0.0f, 0.0f, 0.0f, 1.0f }))
		.after<
			forward::system,
			physics_debug::system,
			sdf_grid::system,
			world_text::system,
			bloom::downsample_pass
		>();

	rec.sample_image(hdr, gpu::pipeline_stage_flag::fragment_shader);
	if (bloom_active) {
		rec.sample_image(bloom_state.mips_down[0], gpu::pipeline_stage_flag::fragment_shader);
	}
	rec.set_viewport(ext);
	rec.set_scissor(ext);
	rec.bind_descriptors(d.pipeline, d.descriptors[frame_index]);
	rec.push(d.pipeline, pc);
	rec.draw(3);
}

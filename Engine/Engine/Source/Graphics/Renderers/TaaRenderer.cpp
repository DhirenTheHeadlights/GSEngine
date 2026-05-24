module gse.graphics;

import std;

import :taa_renderer;
import :atmosphere_renderer;
import :cloud_renderer;
import :depth_prepass_renderer;
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

namespace gse::renderer::taa {
	struct [[
		= shaders::binding<0, 0>{},
		= shaders::sampler2d
	]] hdr_color {};

	struct [[
		= shaders::binding<0, 1>{},
		= shaders::sampler2d
	]] velocity_color {};

	struct [[
		= shaders::binding<0, 2>{},
		= shaders::sampler2d
	]] history_color {};

	struct [[= shaders::shader_struct]] push_constants {
		float blend_alpha;
		std::uint32_t taa_enabled;
		vec2f inv_extent;
	};

	using shader_binding_types = type_pack<hdr_color, velocity_color, history_color>;

	using entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/Taa">,
		gpu::layout<"taa">,
		gpu::bindings<shader_binding_types>,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::depth<false, false>,
		gpu::depth_target<gpu::depth_format::none>,
		gpu::color_targets<gpu::color_format::hdr, gpu::color_format::hdr>
	>;

	auto recreate_history(
		const gpu::context::data& gpu_s,
		system::data& d
	) -> void;

	auto rewrite_descriptors(
		const gpu::context::data& gpu_s,
		system::data& d
	) -> void;
}

auto gse::renderer::taa::recreate_history(const gpu::context::data& gpu_s, system::data& d) -> void {
	const auto extent = gpu_s.render_graph->extent();
	d.frames_since_history_invalid = 0;
	if (extent.x() == 0 || extent.y() == 0) {
		for (auto& image : d.history) {
			image = {};
		}
		return;
	}
	for (std::size_t i = 0; i < d.history.size(); ++i) {
		d.history[i] = gpu::image::create(
			gpu_s.device->allocator(),
			{
				.size = extent,
				.format = gpu::image_format::r16g16b16a16_sfloat,
				.usage = gpu::image_flag::color_attachment | gpu::image_flag::sampled,
			},
			std::format(
				"taa_history_{}",
				i
			)
		);
		gpu::transition_image_to(*gpu_s.device, d.history[i]);
	}
}

auto gse::renderer::taa::rewrite_descriptors(const gpu::context::data& gpu_s, system::data& d) -> void {
	auto& hdr = gpu_s.render_graph->framebuffer_image<targets::hdr_color>();
	if (!hdr.handle()) {
		return;
	}
	auto& velocity = gpu_s.render_graph->framebuffer_image<targets::velocity>();
	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		const auto& prev = d.history[1u - i];
		if (!prev.handle()) {
			continue;
		}
		gpu::descriptor_writer(
			gpu_s.device->handle(),
			d.descriptors[i]
		)
			.combined_image_sampler<hdr_color>(
				hdr,
				d.sampler
			)
			.combined_image_sampler<velocity_color>(
				velocity,
				d.sampler
			)
			.combined_image_sampler<history_color>(
				prev,
				d.sampler
			)
			.commit();
	}
}

auto gse::renderer::taa::system::run(run_context& ctx, const gpu::context::data& gpu_s, data& d) -> async::task<> {
	d.pipeline =
		gpu::build_graphics_program(
			*gpu_s.device,
			*gpu_s.shader_registry,
			*gpu_s.bindless_textures,
			entry::pod
		);

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
			gpu::allocate_descriptors(
				*gpu_s.shader_registry,
				gpu_s.device->descriptor_heap(),
				entry::pod
			);
	}

	recreate_history(gpu_s, d);
	rewrite_descriptors(gpu_s, d);

	gpu::context::on_swap_chain_recreate(gpu_s, [&gpu_s, &d]() {
		recreate_history(gpu_s, d);
		rewrite_descriptors(gpu_s, d);
	});

	co_return;
}

auto gse::renderer::taa::system::frame(const frame_context& ctx, shared_view<gpu::context> gpu_s, data& d) -> async::task<> {
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
	const bool history_ready = d.taa_enabled && d.frames_since_history_invalid >= 2;
	const gpu::typed_push_constants<push_constants> pc{
		.data = {
			.blend_alpha = d.blend_alpha,
			.taa_enabled = history_ready ? 1u : 0u,
			.inv_extent = vec2f{ 1.0f / static_cast<float>(ext.x()), 1.0f / static_cast<float>(ext.y()) },
		},
		.stages = gpu::stage_flag::vertex | gpu::stage_flag::fragment,
	};
	++d.frames_since_history_invalid;

	auto rec = co_await gpu::pass<system>(ctx)
		.pipeline(d.pipeline)
		.color(gpu::clear_color(
			gpu::color_clear{ 0.0f, 0.0f, 0.0f, 1.0f },
			d.history[frame_index]
		))
		.color(gpu::clear_color(
			gpu::color_clear{ 0.0f, 0.0f, 0.0f, 1.0f },
			gpu_s.render_graph->framebuffer_image<targets::post_taa_color>()
		))
		.after<forward::system, atmosphere::sky_raster_pass, cloud::cloud_composite_pass, physics_debug::system, sdf_grid::system, world_text::system, depth_prepass::system>();

	rec.sample_image(hdr, gpu::pipeline_stage_flag::fragment_shader);
	rec.sample_image(
		gpu_s.render_graph->framebuffer_image<targets::velocity>(),
		gpu::pipeline_stage_flag::fragment_shader
	);
	rec.sample_image(d.history[1u - frame_index], gpu::pipeline_stage_flag::fragment_shader);
	rec.set_viewport(ext);
	rec.set_scissor(ext);
	rec.bind_descriptors(d.pipeline, d.descriptors[frame_index]);
	rec.push(d.pipeline, pc);
	rec.draw(3);
}

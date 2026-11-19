module gse.graphics:oit_renderer_impl;

import std;

import :oit_renderer;
import :geometry_collector;
import :camera_latch_renderer;
import :atmosphere_renderer;
import :forward_renderer;
import :physics_debug_renderer;
import :sdf_grid_renderer;
import :sky_renderer;
import :world_text_renderer;
import :render_targets;
import :shared_shaders;
import :mesh;
import :model;


import gse.math;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.gpu;
import gse.gpu_record;
import gse.gpu_backend;
import gse.meta;

namespace gse::renderer::oit {
	struct [[= shaders::shader_struct]] oit_push_constants {
		std::uint32_t meshlet_offset;
		std::uint32_t meshlet_count;
		std::uint32_t first_instance;
		irradiance sun_intensity;
		vec3f sun_direction;
		irradiance sun_ambient;
		vec3f sun_color;
	};

	using accum_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/meshlet_oit">,
		gpu::types<shaders::common::shader_types, shaders::forward::shader_types>,
		gpu::bindings<shaders::standard_3d::camera, shaders::standard_3d::materials, shaders::meshlet::vertices, shaders::meshlet::meshlets, shaders::meshlet::instances, shaders::bindless::sampler>,
		gpu::bindless_textures,
		gpu::helpers<"Standard3D/meshlet_common">,
		gpu::amplification_stage<>,
		gpu::mesh_stage<>,
		gpu::fragment_stage<"fs_oit_main">,
		gpu::push_constant<oit_push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::color_targets<gpu::color_format::hdr, gpu::color_format::hdr>,
		gpu::depth<true, false, gpu::compare_op::greater_or_equal>
	>;

	struct composite_bindings {
		shaders::texture2d<vec4f> oit_accum_in;
		shaders::texture2d<vec4f> oit_reveal_in;
		shaders::sampler_state oit_sampler;
	};

	struct [[= shaders::shader_struct]] composite_push_constants {
		vec2f screen_uv_scale;
		vec2f screen_uv_max;
	};

	using composite_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/OitComposite">,
		gpu::bindings<composite_bindings>,
		gpu::fullscreen_pass,
		gpu::helpers<"Screen/screen_target">,
		gpu::push_constant<composite_push_constants>,
		gpu::color_targets<gpu::color_format::hdr>,
		gpu::blend<gpu::blend_preset::alpha>
	>;

	auto rebind_views(
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> void;
}

auto gse::renderer::oit::rebind_views(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
	auto& accum = gpu_s.render_graph->framebuffer_image<targets::oit_accum>();
	if (accum.handle()) {
		if (!d.accum_view.valid()) {
			d.accum_view = gpu_s.device->allocate_image_slot();
		}
		gpu_s.device->write_sampled_image(d.accum_view.slot(), accum);
	}
	auto& reveal = gpu_s.render_graph->framebuffer_image<targets::oit_reveal>();
	if (reveal.handle()) {
		if (!d.reveal_view.valid()) {
			d.reveal_view = gpu_s.device->allocate_image_slot();
		}
		gpu_s.device->write_sampled_image(d.reveal_view.slot(), reveal);
	}
}

auto gse::renderer::oit::init(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	{
		gpu::program_batch batch(*gpu_s.device);
		batch.add(d.accum_pipeline, accum_entry::pod);
		batch.add(d.composite_pipeline, composite_entry::pod);
	}

	d.sampler = gpu_s.device->register_sampler(gpu::linear_clamp_sampler);

	d.material_sampler = gpu_s.device->register_sampler({});

	rebind_views(gpu_s, d);

	gpu_s.render_graph->on_resize(
		[gpu_s, &d]() {
			rebind_views(gpu_s, d);
		}
	);

	return {};
}

auto gse::renderer::oit::frame(context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out, const channel_read<geometry_collector::render_data> geometry_in, shared_view<camera_latch::data> latch_s, shared_view<geometry_collector::data> gc_r, shared_view<atmosphere::data> atm_state) -> async::task<> {
	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto& render_items = geometry_in.of<geometry_collector::render_data>();
	if (render_items.empty()) {
		co_return;
	}

	const auto& data = render_items[0];
	if (data.transparent_batches.empty()) {
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();
	const auto ext = gpu_s.render_graph->extent();

	auto rec = co_await gpu::pass<^^accumulate_pass>(pass_out)
		.pipeline(d.accum_pipeline)
		.color(
			gpu::clear_color(
				gpu::color_clear{ 0.0f, 0.0f, 0.0f, 0.0f },
				gpu_s.render_graph->framebuffer_image<targets::oit_accum>()
			)
		)
		.color(
			gpu::clear_color(
				gpu::color_clear{ 1.0f, 1.0f, 1.0f, 1.0f },
				gpu_s.render_graph->framebuffer_image<targets::oit_reveal>()
			)
		)
		.depth(gpu::load_depth())
		.after<^^camera_latch::frame, ^^forward::frame, ^^sdf_grid::frame, ^^world_text::frame, ^^sky::frame>();

	rec.set_viewport(ext);
	rec.set_scissor(ext);

	constexpr std::array<std::uint8_t, 2> blend_enables{ 1, 1 };
	constexpr std::array<gpu::color_blend_equation, 2> blend_equations{
		gpu::color_blend_equation{
			.src_color = gpu::blend_factor::one,
			.dst_color = gpu::blend_factor::one,
			.color_op = gpu::blend_op::add,
			.src_alpha = gpu::blend_factor::one,
			.dst_alpha = gpu::blend_factor::one,
			.alpha_op = gpu::blend_op::add,
		},
		gpu::color_blend_equation{
			.src_color = gpu::blend_factor::zero,
			.dst_color = gpu::blend_factor::one_minus_src_alpha,
			.color_op = gpu::blend_op::add,
			.src_alpha = gpu::blend_factor::zero,
			.dst_alpha = gpu::blend_factor::one_minus_src_alpha,
			.alpha_op = gpu::blend_op::add,
		},
	};
	rec.set_color_blend_enable(0, blend_enables);
	rec.set_color_blend_equation(0, blend_equations);

	const auto camera_slot = latch_s.camera_buffers[frame_index].slot();
	const auto material_palette_slot = gc_r.material_palette_buffers[frame_index].slot();
	const auto instance_data_slot = gc_r.instance_buffer[frame_index].slot();

	for (std::size_t i = 0; i < data.transparent_batches.size(); ++i) {
		const auto& batch = data.transparent_batches[i];
		const auto& mesh = batch.key.resolve_mesh();
		if (!mesh.has_meshlets() || !mesh.upload_token().ready()) {
			continue;
		}

		const auto& ml = mesh.meshlet_gpu();
		const std::uint32_t meshlet_count = mesh.meshlet_count();

		rec.push_bindings<accum_entry>(
			{
				.meshlet_offset = 0,
				.meshlet_count = meshlet_count,
				.first_instance = batch.first_instance,
				.sun_intensity = atm_state.sun_intensity,
				.sun_direction = atm_state.sun_direction,
				.sun_ambient = atm_state.sun_ambient_strength * atm_state.sun_intensity,
				.sun_color = atm_state.sun_color * atm_state.sun_transmittance,
			},
			{
				.camera_ubo = camera_slot,
				.material_palette = material_palette_slot,
				.vertices_buffer = batch.deformed_vertices.valid() ? batch.deformed_vertices : ml.vertex_storage.slot(),
				.meshlets_buffer = ml.descriptors,
				.meshlet_vertex_indices = ml.vertices,
				.meshlet_triangles = ml.triangles,
				.meshlet_bounds_buffer = ml.bounds,
				.instance_data_buffer = instance_data_slot,
				.textures_sampler = d.material_sampler,
			}
		);

		rec.draw_mesh_tasks_indirect(
			gc_r.transparent_indirect_commands_buffer[frame_index],
			i * sizeof(gpu::draw_mesh_tasks_indirect_command),
			1,
			sizeof(gpu::draw_mesh_tasks_indirect_command)
		);
	}

	if (!d.accum_view.valid() || !d.reveal_view.valid()) {
		rebind_views(gpu_s, d);
	}

	auto composite_rec = co_await gpu::pass<^^composite_pass>(pass_out)
		.pipeline(d.composite_pipeline)
		.color(gpu::load_color(gpu_s.render_graph->framebuffer_image<targets::hdr_color>()))
		.after<^^accumulate_pass, ^^forward::frame, ^^sky::frame, ^^physics_debug::frame>();

	composite_rec.set_viewport(ext);
	composite_rec.set_scissor(ext);

	composite_rec.push_bindings<composite_entry>(
		{
			.screen_uv_scale = gpu_s.render_graph->screen_uv_scale(),
			.screen_uv_max = gpu_s.render_graph->screen_uv_max(),
		},
		{
			.oit_accum_in = d.accum_view,
			.oit_reveal_in = d.reveal_view,
			.oit_sampler = d.sampler,
		}
	);
	composite_rec.draw(3);
}

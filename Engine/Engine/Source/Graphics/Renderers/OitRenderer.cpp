module gse.graphics:oit_renderer_impl;

import std;

import :oit_renderer;
import :geometry_collector;
import :camera_system;
import :atmosphere_renderer;
import :cloud_renderer;
import :forward_renderer;
import :physics_debug_renderer;
import :sdf_grid_renderer;
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
	struct [[= shaders::binding<0, 0>{}]] camera_ubo {
		using element = shaders::common::camera_data;
	};

	struct [[
		= shaders::binding<0, 5>{},
		= shaders::ssbo_readonly
	]] material_palette {
		using element = shaders::forward::material_data;
	};

	struct [[
		= shaders::binding<1, 5>{},
		= shaders::ssbo_readonly
	]] instance_data_buffer {
		using element = shaders::common::instance_data;
	};

	using accum_binding_types = type_pack<
		camera_ubo,
		material_palette,
		shaders::meshlet::vertices_buffer,
		shaders::meshlet::meshlets_buffer,
		shaders::meshlet::meshlet_vertex_indices,
		shaders::meshlet::meshlet_triangles,
		shaders::meshlet::meshlet_bounds_buffer,
		instance_data_buffer,
		shaders::bindless::textures,
		shaders::bindless::textures_sampler
	>;

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
		gpu::bindings<accum_binding_types>,
		gpu::helpers<"Standard3D/meshlet_common">,
		gpu::amplification_stage<"as_main">,
		gpu::mesh_stage<"ms_main">,
		gpu::fragment_stage<"fs_oit_main">,
		gpu::push_constant<oit_push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::color_targets<gpu::color_format::hdr, gpu::color_format::hdr>,
		gpu::blend<gpu::blend_preset::none>,
		gpu::depth<true, false, gpu::compare_op::less_or_equal>
	>;

	struct [[
		= shaders::binding<0, 0>{},
		= shaders::texture2d
	]] oit_accum_in {
		using element = vec4f;
	};

	struct [[
		= shaders::binding<0, 1>{},
		= shaders::texture2d
	]] oit_reveal_in {
		using element = vec4f;
	};

	struct [[
		= shaders::binding<0, 2>{},
		= shaders::sampler_state
	]] oit_sampler {};

	using composite_binding_types = type_pack<oit_accum_in, oit_reveal_in, oit_sampler>;

	using composite_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/OitComposite">,
		gpu::bindings<composite_binding_types>,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::depth<false, false>,
		gpu::depth_target<gpu::depth_format::none>,
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
	d.accum_pipeline = gpu::build_graphics_program(*gpu_s.device, accum_entry::pod);
	d.composite_pipeline = gpu::build_graphics_program(*gpu_s.device, composite_entry::pod);

	constexpr std::size_t camera_ubo_size = sizeof(shaders::common::camera_data);
	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		d.camera_ubo_buffers[i] = gpu_s.device->create_buffer(
			{
				.size = camera_ubo_size,
				.stride = sizeof(shaders::common::camera_data),
				.usage = gpu::buffer_flag::storage,
				.bindless = true
			},
			"oit.camera_ubo"
		);
	}

	d.sampler = gpu_s.device->register_sampler(
		{
			.min = gpu::sampler_filter::linear,
			.mag = gpu::sampler_filter::linear,
			.address_u = gpu::sampler_address_mode::clamp_to_edge,
			.address_v = gpu::sampler_address_mode::clamp_to_edge,
			.address_w = gpu::sampler_address_mode::clamp_to_edge,
		}
	);

	d.material_sampler = gpu_s.device->register_sampler(
		{
			.min = gpu::sampler_filter::linear,
			.mag = gpu::sampler_filter::linear,
			.address_u = gpu::sampler_address_mode::repeat,
			.address_v = gpu::sampler_address_mode::repeat,
			.address_w = gpu::sampler_address_mode::repeat,
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

auto gse::renderer::oit::frame(context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out, const channel_read<geometry_collector::render_data> geometry_in, shared_view<camera::data> cam_state, shared_view<geometry_collector::data> gc_r, shared_view<atmosphere::data> atm_state) -> async::task<> {
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

	const auto view = cam_state.view_matrix;
	const auto proj = cam_state.projection_matrix;
	const shaders::common::camera_data camera{
		.view = view,
		.proj = proj,
		.inv_view = view.inverse(),
		.inv_view_proj = (proj * view).inverse(),
		.prev_view = cam_state.prev_view_matrix,
		.prev_proj = cam_state.prev_projection_matrix,
		.jitter_ndc = cam_state.jitter_ndc,
		.prev_jitter_ndc = cam_state.prev_jitter_ndc,
	};
	d.camera_ubo_buffers[frame_index].host_write(camera);

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
		.after<^^forward::frame, ^^sdf_grid::frame, ^^world_text::frame, ^^cloud::cloud_composite_pass>();

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

	const auto camera_slot = d.camera_ubo_buffers[frame_index].slot();
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
				.meshlets_buffer = ml.descriptors.slot(),
				.meshlet_vertex_indices = ml.vertices.slot(),
				.meshlet_triangles = ml.triangles.slot(),
				.meshlet_bounds_buffer = ml.bounds.slot(),
				.instance_data_buffer = instance_data_slot,
				.textures_sampler = d.material_sampler.slot(),
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
		.after<^^accumulate_pass, ^^forward::frame, ^^atmosphere::sky_raster_pass, ^^cloud::cloud_composite_pass, ^^physics_debug::frame>();

	composite_rec.sample_image(gpu_s.render_graph->framebuffer_image<targets::oit_accum>(), gpu::pipeline_stage_flag::fragment_shader);
	composite_rec.sample_image(gpu_s.render_graph->framebuffer_image<targets::oit_reveal>(), gpu::pipeline_stage_flag::fragment_shader);
	composite_rec.set_viewport(ext);
	composite_rec.set_scissor(ext);

	composite_rec.push_bindings<composite_entry>({
		.oit_accum_in = d.accum_view.slot(),
		.oit_reveal_in = d.reveal_view.slot(),
		.oit_sampler = d.sampler.slot(),
	});
	composite_rec.draw(3);
}

module gse.graphics;

import std;

import :forward_renderer;
import :atmosphere_renderer;
import :geometry_collector;
import :depth_prepass_renderer;
import :gi_probe_renderer;
import :rt_shadow_renderer;
import :light_culling_renderer;
import :cull_compute_renderer;
import :camera_system;
import :render_targets;
import :texture;
import :point_light;
import :spot_light;
import :directional_light;
import :settings;

import gse.math;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.save;
import gse.meta;

namespace gse::renderer::forward {
	struct [[= shaders::binding<0, 0>{}]] camera_ubo {
		using element = shaders::common::camera_data;
	};

	struct [[
		= shaders::binding<0, 1>{},
		= shaders::ssbo_readonly
	]] lights_ssbo {
		using element = shaders::forward::light;
	};

	struct [[
		= shaders::binding<0, 2>{},
		= shaders::tlas
	]] scene_tlas {};

	struct [[
		= shaders::binding<0, 3>{},
		= shaders::ssbo_readonly
	]] light_index_list {
		using element = std::uint32_t;
	};

	struct [[
		= shaders::binding<0, 4>{},
		= shaders::ssbo_readonly
	]] tile_light_table {
		using element = vec2u;
	};

	struct [[
		= shaders::binding<0, 5>{},
		= shaders::ssbo_readonly
	]] material_palette {
		using element = shaders::forward::material_data;
	};

	struct [[
		= shaders::binding<0, 6>{},
		= shaders::sampler3d
	]] aerial_perspective_in {};

	struct [[
		= shaders::binding<0, 8>{},
		= shaders::sampler2d
	]] gi_atlas {};

	struct [[
		= shaders::binding<1, 5>{},
		= shaders::ssbo_readonly
	]] instance_data_buffer {
		using element = shaders::common::instance_data;
	};

	using shader_binding_types = type_pack<
		camera_ubo,
		lights_ssbo,
		scene_tlas,
		light_index_list,
		tile_light_table,
		material_palette,
		aerial_perspective_in,
		atmosphere::atmosphere_ubo,
		gi_atlas,
		shaders::meshlet::vertices_buffer,
		shaders::meshlet::meshlets_buffer,
		shaders::meshlet::meshlet_vertex_indices,
		shaders::meshlet::meshlet_triangles,
		shaders::meshlet::meshlet_bounds_buffer,
		instance_data_buffer,
		shaders::bindless::textures
	>;

	struct [[= shaders::shader_struct]] meshlet_push_constants {
		std::uint32_t meshlet_offset;
		std::uint32_t meshlet_count;
		std::uint32_t first_instance;
		std::int32_t num_lights;
		vec2u screen_size;
		std::int32_t shadow_quality;
		std::int32_t ao_quality;
		std::int32_t reflection_quality;
		vec3<position> gi_origin;
		length gi_spacing;
		vec3u gi_grid_dim;
		float gi_intensity;
		vec2f gi_atlas_size;
		std::int32_t gi_enabled;
	};

	using meshlet_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/meshlet_geometry">,
		gpu::layout<"forward_3d">,
		gpu::types<shaders::common::shader_types, shaders::forward::shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"Standard3D/meshlet_common", "Standard3D/gi_probe_sample">,
		gpu::amplification_stage<"as_main">,
		gpu::mesh_stage<"ms_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<meshlet_push_constants>,
		gpu::color_targets<gpu::color_format::hdr>,
		gpu::depth<true, false, gpu::compare_op::less_or_equal>
	>;

	auto rebind_tlas_views(
		const gpu::context::data& gpu_s,
		const rt_shadow::system::data& rt_state,
		system::data& d
	) -> void;
}

auto gse::renderer::forward::rebind_tlas_views(const gpu::context::data& gpu_s, const rt_shadow::system::data& rt_state, system::data& d) -> void {
	for (std::size_t i = 0; i < per_frame_resource<gpu::bindless_tlas_view>::frames_in_flight; ++i) {
		const auto fi = static_cast<std::uint32_t>(i);
		d.tlas_views[i].rebind(
			gpu_s.device->allocator(),
			*gpu_s.bindless_heaps,
			(*rt_state.tlas_ptrs[fi]).handle()
		);
	}
}

auto gse::renderer::forward::system::run(run_context& ctx, const gpu::context::data& gpu_s, const asset::data& assets_s, const rt_shadow::system::data& rt_state, const light_culling::system::data& lc_r, const atmosphere::system::data& atm_state, const gi_probe::system::data& gi_state, const geometry_collector::system::data& gc_state, data& d) -> async::task<> {
	d.pipeline = gpu::build_graphics_program(
		*gpu_s.device,
		*gpu_s.shader_registry,
		*gpu_s.bindless_heaps,
		meshlet_entry::pod
	);

	constexpr std::size_t camera_ubo_size = sizeof(shaders::common::camera_data);
	constexpr std::size_t light_stride = sizeof(shaders::forward::light);
	constexpr std::size_t light_buffer_size = light_stride * max_lights;

	d.light_staging.reserve(light_buffer_size);

	d.gi_sampler = gpu::bindless_sampler::create(
		*gpu_s.bindless_heaps,
		{
			.min = gpu::sampler_filter::linear,
			.mag = gpu::sampler_filter::linear,
			.address_u = gpu::sampler_address_mode::clamp_to_edge,
			.address_v = gpu::sampler_address_mode::clamp_to_edge,
			.address_w = gpu::sampler_address_mode::clamp_to_edge,
		}
	);

	for (std::size_t i = 0; i < per_frame_resource<gpu::bindless_buffer>::frames_in_flight; ++i) {
		d.camera_ubo_buffers[i] = gpu::bindless_buffer::create(
			gpu_s.device->allocator(),
			*gpu_s.bindless_heaps,
			{
				.size = camera_ubo_size,
				.usage = gpu::buffer_flag::uniform
			},
			"forward.camera_ubo"
		);

		d.light_buffers[i] = gpu::bindless_buffer::create(
			gpu_s.device->allocator(),
			*gpu_s.bindless_heaps,
			{
				.size = light_buffer_size,
				.usage = gpu::buffer_flag::storage
			},
			"forward.lights"
		);
	}

	rebind_tlas_views(gpu_s, rt_state, d);

	gpu::context::on_swap_chain_recreate(
		gpu_s,
		[&gpu_s, &rt_state, &d]() {
			rebind_tlas_views(gpu_s, rt_state, d);
		}
	);

	co_return;
}

auto gse::renderer::forward::system::frame(frame_context& ctx, shared_view<gpu::context> gpu_s, data& d, shared_view<camera::system> cam_state, shared_view<geometry_collector::system> gc_r, shared_view<light_culling::system> lc_r, shared_view<atmosphere::system> atm_state, shared_view<gi_probe::system> gi_state) -> async::task<> {
	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto& render_items = ctx.read_channel<geometry_collector::render_data>();
	if (render_items.empty()) {
		const auto ext = gpu_s.render_graph->extent();
		auto rec = co_await gpu::pass<system>(ctx)
			.color(
				gpu::clear_color(
					gpu::color_clear{ 0.0f, 0.0f, 0.0f, 1.0f },
					gpu_s.render_graph->framebuffer_image<targets::hdr_color>()
				)
			)
			.depth(
				gpu::clear_depth(
					gpu::depth_clear{
						.depth = 1.0f
					}
				)
			);
		rec.set_viewport(ext);
		rec.set_scissor(ext);
		co_return;
	}

	const auto& data = render_items[0];
	const auto frame_index = gpu_s.render_graph->current_frame();

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
	d.camera_ubo_buffers[frame_index].buffer().host_write(camera);

	auto dir_chunk = ctx.components<directional_light_component>();
	auto spot_chunk = ctx.components<spot_light_component>();
	auto point_chunk = ctx.components<point_light_component>();

	const std::size_t total_lights = std::min(
		dir_chunk.size() + spot_chunk.size() + point_chunk.size() + 1u,
		max_lights
	);
	auto& staging = d.light_staging;
	staging.assign(total_lights * sizeof(shaders::forward::light), std::byte{ 0 });
	auto* staging_lights = reinterpret_cast<shaders::forward::light*>(staging.data());
	std::size_t light_count = 0;

	if (light_count < max_lights) {
		const auto sun_to_surface = -atm_state.sun_direction;
		staging_lights[light_count] = {
			.light_type = shaders::forward::light_type::directional,
			.direction = view.transform_direction(sun_to_surface),
			.world_direction = sun_to_surface,
			.color = atm_state.sun_color,
			.intensity = atm_state.sun_intensity,
			.ambient_strength = atm_state.sun_ambient_strength,
			.source_radius = atm_state.sun_source_radius,
		};
		++light_count;
	}

	for (const auto& comp : dir_chunk) {
		if (light_count >= max_lights) {
			break;
		}
		staging_lights[light_count] = {
			.light_type = shaders::forward::light_type::directional,
			.direction = view.transform_direction(comp.direction),
			.world_direction = comp.direction,
			.color = comp.color,
			.intensity = comp.intensity,
			.ambient_strength = comp.ambient_strength,
			.source_radius = comp.source_radius,
		};
		++light_count;
	}

	for (const auto& comp : spot_chunk) {
		if (light_count >= max_lights) {
			break;
		}
		const float cut_off_cos = gse::cos(comp.cut_off);
		const float outer_cut_off_cos = gse::cos(comp.outer_cut_off);
		staging_lights[light_count] = {
			.light_type = shaders::forward::light_type::spot,
			.position = view.transform_point(comp.position),
			.direction = view.transform_direction(comp.direction),
			.world_position = comp.position,
			.world_direction = comp.direction,
			.color = comp.color,
			.intensity = comp.intensity,
			.constant = comp.constant,
			.linear = comp.linear,
			.quadratic = comp.quadratic,
			.cut_off = cut_off_cos,
			.outer_cut_off = outer_cut_off_cos,
			.ambient_strength = comp.ambient_strength,
			.source_radius = comp.source_radius,
		};
		++light_count;
	}

	for (const auto& comp : point_chunk) {
		if (light_count >= max_lights) {
			break;
		}
		staging_lights[light_count] = {
			.light_type = shaders::forward::light_type::point,
			.position = view.transform_point(comp.position),
			.world_position = comp.position,
			.color = comp.color,
			.intensity = comp.intensity,
			.constant = comp.constant,
			.linear = comp.linear,
			.quadratic = comp.quadratic,
			.ambient_strength = comp.ambient_strength,
			.source_radius = comp.source_radius,
		};
		++light_count;
	}

	if (light_count > 0) {
		d.light_buffers[frame_index].buffer().host_write(
			staging.data(),
			light_count * sizeof(shaders::forward::light)
		);
	}

	const auto& normal_batches = data.normal_batches;

	const auto ext = gpu_s.render_graph->extent();
	const int num_lights_i = static_cast<int>(light_count);
	const int shadow_quality_i = static_cast<int>(d.shadow_quality);
	const int ao_quality_i = static_cast<int>(d.ao_quality);
	const int reflection_quality_i = static_cast<int>(d.reflection_quality);

	constexpr vec2f gi_atlas_size_v{
		static_cast<float>(gi_probe::grid_dim.x() * gi_probe::probe_tile_size),
		static_cast<float>(gi_probe::grid_dim.y() * gi_probe::grid_dim.z() * gi_probe::probe_tile_size),
	};
	const bool gi_enabled = gi_state.quality != gi_probe::quality_level::off;

	auto rec = co_await gpu::pass<system>(ctx)
		.pipeline(d.pipeline)
		.color(
			gpu::clear_color(
				gpu::color_clear{ 0.0f, 0.0f, 0.0f, 1.0f },
				gpu_s.render_graph->framebuffer_image<targets::hdr_color>()
			)
		)
		.depth(gpu::load_depth())
		.after<rt_shadow::system, light_culling::system, depth_prepass::system, atmosphere::ap_compute_pass, gi_probe::system>();
	rec.set_viewport(ext);
	rec.set_scissor(ext);

	rec.sample_image(gi_state.irradiance_atlas.image(), gpu::pipeline_stage_flag::fragment_shader);

	if (normal_batches.empty()) {
		co_return;
	}

	const auto camera_ubo_slot = d.camera_ubo_buffers[frame_index].slot();
	const auto lights_slot = d.light_buffers[frame_index].slot();
	const auto tlas_slot = d.tlas_views[frame_index].slot();
	const auto light_index_list_slot = lc_r.light_index_list_buffers[frame_index].slot();
	const auto tile_light_table_slot = lc_r.tile_light_table_buffers[frame_index].slot();
	const auto material_palette_slot = gc_r.material_palette_buffers[frame_index].slot();
	const auto atmosphere_ubo_slot = atm_state.atmosphere_ubo_buffer.slot();
	const auto instance_data_slot = gc_r.instance_buffer[frame_index].slot();

	const gpu::combined_sampler_arg aerial_perspective_arg{
		.image = atm_state.ap_volume.sampled_slot(),
		.sampler = atm_state.lut_sampler_bindless.slot(),
	};
	const gpu::combined_sampler_arg gi_atlas_arg{
		.image = gi_state.irradiance_atlas.sampled_slot(),
		.sampler = d.gi_sampler.slot(),
	};

	for (std::size_t i = 0; i < normal_batches.size(); ++i) {
		const auto& batch = normal_batches[i];
		const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

		if (!mesh.has_meshlets()) {
			continue;
		}

		if (!mesh.upload_token().ready()) {
			continue;
		}

		const auto& ml = mesh.meshlet_gpu();
		const std::uint32_t meshlet_count = mesh.meshlet_count();

		rec.push_bindings<meshlet_entry>(
			{
				.meshlet_offset = 0,
				.meshlet_count = meshlet_count,
				.first_instance = batch.first_instance,
				.num_lights = num_lights_i,
				.screen_size = vec2u{ ext.x(), ext.y() },
				.shadow_quality = shadow_quality_i,
				.ao_quality = ao_quality_i,
				.reflection_quality = reflection_quality_i,
				.gi_origin = gi_state.origin_world,
				.gi_spacing = gi_state.spacing,
				.gi_grid_dim = gi_probe::grid_dim,
				.gi_intensity = gi_state.intensity,
				.gi_atlas_size = gi_atlas_size_v,
				.gi_enabled = gi_enabled ? 1 : 0,
			},
			{
				.camera_ubo = camera_ubo_slot,
				.lights_ssbo = lights_slot,
				.scene_tlas = tlas_slot,
				.light_index_list = light_index_list_slot,
				.tile_light_table = tile_light_table_slot,
				.material_palette = material_palette_slot,
				.aerial_perspective_in = aerial_perspective_arg,
				.atmosphere_ubo = atmosphere_ubo_slot,
				.gi_atlas = gi_atlas_arg,
				.vertices_buffer = ml.vertex_storage.slot(),
				.meshlets_buffer = ml.descriptors.slot(),
				.meshlet_vertex_indices = ml.vertices.slot(),
				.meshlet_triangles = ml.triangles.slot(),
				.meshlet_bounds_buffer = ml.bounds.slot(),
				.instance_data_buffer = instance_data_slot,
			}
		);

		rec.draw_mesh_tasks_indirect(
			gc_r.normal_indirect_commands_buffer[frame_index].buffer(),
			i * sizeof(gpu::draw_mesh_tasks_indirect_command),
			1,
			sizeof(gpu::draw_mesh_tasks_indirect_command)
		);
	}
}

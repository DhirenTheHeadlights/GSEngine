module gse.graphics:gi_probe_renderer_impl;

import std;

import :gi_probe_renderer;
import :atmosphere_renderer;
import :camera_system;
import :directional_light;
import :geometry_collector;
import :light_packing;
import :lighting_source;
import :point_light;
import :shared_shaders;
import :spot_light;


import gse.gpu;
import gse.gpu_record;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.meta;
import gse.log;
import gse.physics;

namespace gse::renderer::gi_probe {
	constexpr std::size_t max_lights = 1024;

	struct [[= shaders::shader_struct]] push_constants {
		vec3<position> origin_world;
		length spacing;
		vec3u grid_dim_uniform;
		std::uint32_t frame_counter;
		length trace_t_max;
		std::uint32_t light_count;
		std::uint32_t history_valid;
		std::uint32_t first_probe;
	};

	struct [[= shaders::shader_constant_block]] gi_probe_limits {
		float surface_sky_response = 0.1f;
		float temporal_blend = 0.0625f;
		std::uint32_t capture_resolution = gi_probe::capture_resolution;
	};

	struct [[= shaders::shader_struct]]
	capture_push_constants {
		vec3<position> origin_world;
		length spacing;
		vec3u grid_dim;
		std::uint32_t first_probe;
		length trace_t_max;
		length near_plane;
		std::uint32_t probe_count;
		std::uint32_t light_count;
		std::uint32_t meshlet_offset;
		std::uint32_t meshlet_count;
		std::uint32_t first_instance;
	};

	struct [[= shaders::texture2d]] capture_in {
		using element = vec4f;
	};

	struct [[= shaders::tlas]] scene_tlas {};

	struct [[= shaders::storage_image]] irradiance_atlas_out {
		using element = vec4f;
	};

	struct [[= shaders::ssbo_readonly]] material_palette {
		using element = shaders::forward::material_data;
	};

	struct [[= shaders::texture2d]] sky_view_in {
		using element = vec4f;
	};

	struct [[= shaders::sampler_state]] sky_view_sampler {};

	struct [[= shaders::ssbo_readonly]] lights {
		using element = shaders::forward::light;
	};

	using common_bindings = type_pack<irradiance_atlas_out, material_palette, sky_view_in, sky_view_sampler, lights, atmosphere::atmosphere_ubo>;

	template <typename TechniqueBindings, typename TechniqueHelpers, fixed_string Body = "Compute/gi_probe_update">
	using update_entry = gpu::compute_entry<
		gpu::body_path<Body>,
		gpu::types<shaders::forward::shader_types, type_pack<gi_probe_limits>>,
		gpu::bindings<typename type_pack_concat<TechniqueBindings, common_bindings>::type>,
		gpu::helpers<"Atmosphere/atmosphere_common", "Standard3D/light_eval", "Standard3D/probe_radiance">,
		TechniqueHelpers,
		gpu::helpers<"Standard3D/probe_irradiance_project">,
		gpu::threads<rays_per_probe>,
		gpu::push_constant<push_constants>,
		gpu::system_values<gpu::dispatch_thread_id, gpu::group_id, gpu::group_thread_id>
	>;

	using ray_traced_entry = update_entry<type_pack<scene_tlas>, gpu::helpers<"Standard3D/probe_radiance_ray_traced">>;

	using raster_helpers = gpu::helpers<"Standard3D/cube_faces", "Standard3D/probe_capture_common", "Standard3D/probe_radiance_raster">;

	using raster_entry = update_entry<type_pack<capture_in>, raster_helpers>;

	using raster_overwrite_entry = update_entry<type_pack<capture_in>, raster_helpers, "Compute/gi_probe_update_overwrite">;

	using capture_bindings = type_pack<
		material_palette,
		lights,
		sky_view_in,
		sky_view_sampler,
		atmosphere::atmosphere_ubo,
		shaders::meshlet::vertices_buffer,
		shaders::meshlet::meshlets_buffer,
		shaders::meshlet::meshlet_vertex_indices,
		shaders::meshlet::meshlet_triangles,
		shaders::meshlet::meshlet_bounds_buffer,
		shaders::meshlet::instance_data_buffer
	>;

	using capture_vertex_bindings = type_pack<
		material_palette,
		lights,
		sky_view_in,
		sky_view_sampler,
		atmosphere::atmosphere_ubo,
		shaders::meshlet::vertices_buffer,
		shaders::meshlet::instance_data_buffer
	>;

	using capture_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/gi_probe_capture">,
		gpu::types<shaders::common::shader_types, shaders::forward::shader_types, type_pack<gi_probe_limits>>,
		gpu::bindings<capture_bindings>,
		gpu::helpers<"Standard3D/meshlet_common", "Atmosphere/atmosphere_common", "Standard3D/light_eval", "Standard3D/cube_faces", "Standard3D/probe_capture_common", "Standard3D/probe_capture_shading">,
		gpu::amplification_stage<"as_main">,
		gpu::mesh_stage<"ms_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<capture_push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::color_targets<gpu::color_format::hdr>,
		gpu::depth<true, true, gpu::compare_op::greater_or_equal>
	>;

	using capture_vertex_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/gi_probe_capture_vertex">,
		gpu::types<shaders::common::shader_types, shaders::forward::shader_types, type_pack<gi_probe_limits>>,
		gpu::bindings<capture_vertex_bindings>,
		gpu::helpers<"Standard3D/meshlet_common", "Atmosphere/atmosphere_common", "Standard3D/light_eval", "Standard3D/cube_faces", "Standard3D/probe_capture_common", "Standard3D/probe_capture_shading">,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<capture_push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::color_targets<gpu::color_format::hdr>,
		gpu::depth<true, true, gpu::compare_op::greater_or_equal>
	>;

	auto atlas_extent(
		vec3u dim
	) -> vec2u;

	auto recreate_atlas(
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> void;

	auto create_capture_targets(
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> void;

	auto record_capture(
		gpu::recording_context& rec,
		const geometry_collector::render_data& geometry,
		const capture_push_constants& base,
		const gpu::binding_args<capture_bindings>& bindings,
		bool vertex_pulled
	) -> void;
}

auto gse::renderer::gi_probe::create_capture_targets(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
	const vec2u extent{ 6 * capture_resolution, probes_per_capture * capture_resolution };
	d.capture_radiance = gpu_s.device->create_image(
		{
			.size = extent,
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.usage = { gpu::image_flag::color_attachment, gpu::image_flag::sampled },
		},
		"gi_probe.capture_radiance"
	);
	gpu::transition_image_to(*gpu_s.device, d.capture_radiance);
	d.capture_depth = gpu_s.device->create_image(
		{
			.size = extent,
			.format = gpu::image_format::d32_sfloat,
			.usage = { gpu::image_flag::depth_attachment },
		},
		"gi_probe.capture_depth"
	);
	gpu::transition_image_to(*gpu_s.device, d.capture_depth);
}

auto gse::renderer::gi_probe::record_capture(gpu::recording_context& rec, const geometry_collector::render_data& geometry, const capture_push_constants& base, const gpu::binding_args<capture_bindings>& bindings, const bool vertex_pulled) -> void {
	for (const auto& batch : geometry.normal_batches) {
		const auto& mesh = batch.key.resolve_mesh();
		if (!mesh.has_meshlets() || !mesh.upload_token().ready() || batch.instance_count == 0) {
			continue;
		}

		const auto& ml = mesh.meshlet_gpu();
		const auto vertices_slot = batch.deformed_vertices.valid() ? batch.deformed_vertices : ml.vertex_storage.slot();

		if (vertex_pulled) {
			auto pc = base;
			pc.first_instance = batch.first_instance;
			rec.push_bindings<capture_vertex_entry>(
				pc,
				{
					.material_palette = bindings.material_palette,
					.lights = bindings.lights,
					.sky_view_in = bindings.sky_view_in,
					.sky_view_sampler = bindings.sky_view_sampler,
					.atmosphere_ubo = bindings.atmosphere_ubo,
					.vertices_buffer = vertices_slot,
					.instance_data_buffer = bindings.instance_data_buffer,
				}
			);
			rec.bind_index(mesh.index_gpu_buffer());
			rec.draw_indexed(static_cast<std::uint32_t>(mesh.indices().size()), batch.instance_count * base.probe_count * 6);
			continue;
		}

		auto pc = base;
		pc.meshlet_offset = 0;
		pc.meshlet_count = mesh.meshlet_count();
		pc.first_instance = batch.first_instance;

		auto args = bindings;
		args.vertices_buffer = vertices_slot;
		args.meshlets_buffer = ml.descriptors.slot();
		args.meshlet_vertex_indices = ml.vertices.slot();
		args.meshlet_triangles = ml.triangles.slot();
		args.meshlet_bounds_buffer = ml.bounds.slot();

		rec.push_bindings<capture_entry>(pc, args);
		rec.draw_mesh_tasks((pc.meshlet_count + 31) / 32, batch.instance_count, pc.probe_count * 6);
	}
}

auto gse::renderer::gi_probe::grid_dim_for(const quality_level quality) -> vec3u {
	return annotation_from_enum(quality, probe_grid_info{}).dim;
}

auto gse::renderer::gi_probe::atlas_extent(const vec3u dim) -> vec2u {
	return {
		dim.x() * probe_tile_size,
		dim.y() * dim.z() * probe_tile_size,
	};
}

auto gse::renderer::gi_probe::recreate_atlas(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
	const auto ext = atlas_extent(d.atlas_grid_dim);
	d.irradiance_atlas = gpu_s.device->create_image(
		{
			.size = ext,
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.usage = { gpu::image_flag::storage, gpu::image_flag::sampled },
		},
		"gi_irradiance_atlas"
	);
	gpu::transition_image_to(*gpu_s.device, d.irradiance_atlas);
}

auto gse::renderer::gi_probe::init(context& ctx, const shared_view<gpu::context::data> gpu_s, const shared_view<lighting::data> lighting_s, const shared_view<geometry_collector::data> gc_state, data& d) -> async::task<> {
	if (lighting_s.lighting_technique == lighting::technique::ray_traced) {
		d.update_pipeline = gpu::build_compute_program(*gpu_s.device, ray_traced_entry::pod);
	}
	else {
		d.vertex_pulled = !gpu_s.device->capabilities().mesh_shaders;
		d.update_pipeline = d.vertex_pulled
			? gpu::build_compute_program(*gpu_s.device, raster_overwrite_entry::pod)
			: gpu::build_compute_program(*gpu_s.device, raster_entry::pod);
		d.capture_pipeline = d.vertex_pulled
			? gpu::build_graphics_program(*gpu_s.device, capture_vertex_entry::pod)
			: gpu::build_graphics_program(*gpu_s.device, capture_entry::pod);
		create_capture_targets(gpu_s, d);
	}
	log::println(log::category::render, "gi_probe: capture path {}", lighting_s.lighting_technique == lighting::technique::ray_traced ? "ray_traced" : d.vertex_pulled ? "vertex_pulled" : "mesh");
	d.atlas_grid_dim = grid_dim_for(d.quality);

	d.sky_view_sampler = gpu_s.device->register_sampler(
		{
			.min = gpu::sampler_filter::linear,
			.mag = gpu::sampler_filter::linear,
			.address_u = gpu::sampler_address_mode::repeat,
			.address_v = gpu::sampler_address_mode::clamp_to_edge,
			.address_w = gpu::sampler_address_mode::clamp_to_edge,
		}
	);

	recreate_atlas(gpu_s, d);

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		d.light_buffers[i] = gpu_s.device->create_buffer(
			{
				.size = sizeof(shaders::forward::light) * max_lights,
				.stride = sizeof(shaders::forward::light),
				.usage = gpu::buffer_flag::storage,
			}
		);
	}

	return {};
}

auto gse::renderer::gi_probe::frame(context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out, const channel_read<geometry_collector::render_data> geometry_in, shared_view<camera::data> cam_state, shared_view<atmosphere::data> atm_state, shared_view<geometry_collector::data> gc_r, read<directional_light_component> dir_lights, read<spot_light_component> spot_lights, read<point_light_component> point_lights, read<physics::transform_component> transforms, const channel_read<camera::temporal_reset_request> reset_in, const shared_view<lighting::data> lighting_s, const channel_read<lighting_source::ray_traced_scene> scene_in) -> async::task<> {
	if (!reset_in.of<camera::temporal_reset_request>().empty()) {
		d.frame_counter = 0;
		d.history_reset = true;
		d.capture_cursor = 0;
	}

	if (const auto& published = scene_in.of<lighting_source::ray_traced_scene>(); !published.empty()) {
		d.ray_traced_scene = published[published.size() - 1];
	}

	if (d.quality == quality_level::off) {
		co_return;
	}

	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	if (!d.irradiance_atlas.valid()) {
		co_return;
	}

	const auto& rt_render_items = geometry_in.of<geometry_collector::render_data>();
	if (rt_render_items.empty() || rt_render_items[0].normal_batches.empty()) {
		co_return;
	}

	const auto inv_view = cam_state.view_matrix.inverse();
	const auto cam_world = inv_view.transform_point(vec3<position>{});

	const auto active_grid_dim = d.atlas_grid_dim;
	const length half_x = (static_cast<float>(active_grid_dim.x() - 1) * 0.5f) * d.spacing;
	const length half_z = (static_cast<float>(active_grid_dim.z() - 1) * 0.5f) * d.spacing;
	d.origin_world = vec3<position>(cam_world.x() - half_x, meters(1.0f), cam_world.z() - half_z);

	if (!isfinite(d.origin_world) || !isfinite(d.spacing) || !isfinite(d.trace_t_max)) {
		log::println(log::level::warning, log::category::render, "gi_probe: non-finite ray inputs (origin/spacing/trace_max); skipping GI trace this frame");
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();

	std::array<shaders::forward::light, max_lights> packed_lights{};
	const auto light_count = light_packing::pack(packed_lights, cam_state.view_matrix, atm_state, dir_lights, spot_lights, point_lights, transforms);

	if (light_count > 0) {
		d.light_buffers[frame_index].host_write(packed_lights.data(), light_count * sizeof(shaders::forward::light));
	}

	if (lighting_s.lighting_technique == lighting::technique::raster) {
		const length near_plane = meters(0.05f);
		const std::uint32_t total_probes = active_grid_dim.x() * active_grid_dim.y() * active_grid_dim.z();
		const auto first_probe = d.capture_cursor;
		d.capture_cursor = (d.capture_cursor + probes_per_capture) % total_probes;

		{
			auto capture_rec = co_await gpu::pass<^^capture_pass>(pass_out)
				.pipeline(d.capture_pipeline)
				.color(gpu::clear_color(gpu::color_clear{ 0.0f, 0.0f, 0.0f, 0.0f }, d.capture_radiance))
				.depth(gpu::clear_depth(gpu::depth_clear{ 0.0f }, d.capture_depth))
				.after<^^atmosphere::sky_view_pass>();
			const auto capture_extent = d.capture_radiance.extent();
			capture_rec.set_viewport(vec2u{ capture_extent.x(), capture_extent.y() });
			capture_rec.set_scissor(vec2u{ capture_extent.x(), capture_extent.y() });
			record_capture(
				capture_rec,
				rt_render_items[0],
				{
					.origin_world = d.origin_world,
					.spacing = d.spacing,
					.grid_dim = active_grid_dim,
					.first_probe = first_probe,
					.trace_t_max = d.trace_t_max,
					.near_plane = near_plane,
					.probe_count = probes_per_capture,
					.light_count = static_cast<std::uint32_t>(light_count),
				},
				{
					.material_palette = gc_r.material_palette_buffers[frame_index].slot(),
					.lights = d.light_buffers[frame_index].slot(),
					.sky_view_in = atm_state.sky_view_lut.sampled_slot(),
					.sky_view_sampler = d.sky_view_sampler.slot(),
					.atmosphere_ubo = atm_state.atmosphere_ubo_buffer.slot(),
					.instance_data_buffer = gc_r.instance_buffer[frame_index].slot(),
				},
				d.vertex_pulled
			);
		}

		auto rec = co_await gpu::pass<^^frame>(pass_out).pipeline(d.update_pipeline).after<^^capture_pass>();
		const auto dispatch_update = [&]<typename Entry>() {
			rec.dispatch<Entry>(
				{
					.origin_world = d.origin_world,
					.spacing = d.spacing,
					.grid_dim_uniform = active_grid_dim,
					.frame_counter = d.frame_counter,
					.trace_t_max = d.trace_t_max,
					.light_count = static_cast<std::uint32_t>(light_count),
					.history_valid = 0u,
					.first_probe = first_probe,
				},
				{
					.capture_in = d.capture_radiance.sampled_slot(),
					.irradiance_atlas_out = d.irradiance_atlas.storage_slot(),
					.material_palette = gc_r.material_palette_buffers[frame_index].slot(),
					.sky_view_in = atm_state.sky_view_lut.sampled_slot(),
					.sky_view_sampler = d.sky_view_sampler.slot(),
					.lights = d.light_buffers[frame_index].slot(),
					.atmosphere_ubo = atm_state.atmosphere_ubo_buffer.slot(),
				},
				vec3u{ probes_per_capture, 1u, 1u }
			);
		};
		if (d.vertex_pulled) {
			dispatch_update.template operator()<raster_overwrite_entry>();
		}
		else {
			dispatch_update.template operator()<raster_entry>();
		}

		++d.frame_counter;
		d.history_reset = false;
		co_return;
	}

	if (!d.ray_traced_scene) {
		co_return;
	}
	const auto& scene = *d.ray_traced_scene;

	auto rec = co_await gpu::pass<^^frame>(pass_out).pipeline(d.update_pipeline).after(std::span(&scene.pass, 1)).after<^^atmosphere::sky_view_pass>();

	rec.dispatch<ray_traced_entry>(
		{
			.origin_world = d.origin_world,
			.spacing = d.spacing,
			.grid_dim_uniform = active_grid_dim,
			.frame_counter = d.frame_counter,
			.trace_t_max = d.trace_t_max,
			.light_count = static_cast<std::uint32_t>(light_count),
			.history_valid = d.history_reset ? 0u : 1u,
			.first_probe = 0,
		},
		{
			.scene_tlas = gpu::make_acceleration_structure_arg(scene.tlas_addresses[frame_index], scene.tlas_slots[frame_index]),
			.irradiance_atlas_out = d.irradiance_atlas.storage_slot(),
			.material_palette = gc_r.material_palette_buffers[frame_index].slot(),
			.sky_view_in = atm_state.sky_view_lut.sampled_slot(),
			.sky_view_sampler = d.sky_view_sampler.slot(),
			.lights = d.light_buffers[frame_index].slot(),
			.atmosphere_ubo = atm_state.atmosphere_ubo_buffer.slot(),
		},
		active_grid_dim
	);

	++d.frame_counter;
	d.history_reset = false;
}
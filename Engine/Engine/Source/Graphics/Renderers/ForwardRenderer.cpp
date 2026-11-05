module gse.graphics:forward_renderer_impl;

import gse.assets;
import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.gpu;
import gse.gpu_record;
import gse.math;
import gse.meta;
import gse.os;
import gse.save;
import gse.time;
import std;

import :atmosphere_renderer;
import :camera_system;
import :depth_prepass_renderer;
import :directional_light;
import :forward_renderer;
import :geometry_collector;
import :gi_probe_renderer;
import :light_culling_renderer;
import :light_packing;
import :lighting_source;
import :mesh;
import :point_light;
import :render_targets;
import :skin_renderer;
import :spot_light;
import :texture;
import :settings;

namespace gse::renderer::forward {
	struct [[= shaders::uniform_block]] camera_ubo {
		using element = shaders::common::camera_data;
	};

	struct [[= shaders::ssbo_readonly]] lights_ssbo {
		using element = shaders::forward::light;
	};

	struct [[= shaders::tlas]] scene_tlas {};

	struct [[= shaders::ssbo_readwrite]] light_index_list {
		using element = std::uint32_t;
	};

	struct [[= shaders::ssbo_readwrite]] tile_light_table {
		using element = vec2u;
	};

	struct [[= shaders::ssbo_readonly]] material_palette {
		using element = shaders::forward::material_data;
	};

	struct [[= shaders::texture3d]] aerial_perspective_in {
		using element = vec4f;
	};

	struct [[= shaders::texture2d]] gi_atlas {
		using element = vec4f;
	};

	struct [[= shaders::sampler_state]] ap_sampler {};

	struct [[= shaders::sampler_state]] gi_atlas_sampler {};

	struct [[= shaders::texture2d]] cloud_shadow_in {
		using element = vec4f;
	};

	struct [[= shaders::sampler_state]] cloud_shadow_sampler {};

	struct [[= shaders::texture2d]] sky_view_in {
		using element = vec4f;
	};

	struct [[= shaders::sampler_state]] sky_view_sampler {};

	struct [[= shaders::texture2d]] shadow_cascade_atlas {
		using element = vec4f;
	};

	struct [[= shaders::texture2d]] shadow_local_atlas {
		using element = vec4f;
	};

	struct [[= shaders::ssbo_readonly]] shadow_tiles {
		using element = shaders::raster_shadow::shadow_tile;
	};

	struct [[= shaders::ssbo_readonly]] light_shadow_tiles {
		using element = vec2u;
	};

	struct [[= shaders::texture2d]] ambient_occlusion_in {
		using element = vec4f;
	};

	struct [[= shaders::texture2d]] reflection_in {
		using element = vec4f;
	};

	struct [[= shaders::texture2d]] material_diffuse_in {
		using element = vec4f;
	};

	struct [[= shaders::sampler_state]] material_diffuse_sampler {};

	using ray_traced_bindings = type_pack<
		scene_tlas
	>;

	using raster_bindings = type_pack<
		shadow_cascade_atlas,
		shadow_local_atlas,
		shadow_tiles,
		light_shadow_tiles,
		ambient_occlusion_in,
		reflection_in
	>;

	using leading_bindings = type_pack<
		camera_ubo,
		lights_ssbo
	>;

	using trailing_bindings = type_pack<
		light_index_list,
		tile_light_table,
		material_palette,
		aerial_perspective_in,
		atmosphere::atmosphere_ubo,
		gi_atlas,
		ap_sampler,
		gi_atlas_sampler,
		cloud::cloud_shadow_ubo,
		cloud_shadow_in,
		cloud_shadow_sampler,
		sky_view_in,
		sky_view_sampler,
		shaders::meshlet::vertices_buffer,
		shaders::meshlet::meshlets_buffer,
		shaders::meshlet::meshlet_vertex_indices,
		shaders::meshlet::meshlet_triangles,
		shaders::meshlet::meshlet_bounds_buffer,
		shaders::meshlet::instance_data_buffer,
		shaders::bindless::textures,
		shaders::bindless::textures_sampler
	>;

	using vertex_pulled_trailing_bindings = type_pack<
		light_index_list,
		tile_light_table,
		material_palette,
		aerial_perspective_in,
		atmosphere::atmosphere_ubo,
		gi_atlas,
		ap_sampler,
		gi_atlas_sampler,
		cloud::cloud_shadow_ubo,
		cloud_shadow_in,
		cloud_shadow_sampler,
		sky_view_in,
		sky_view_sampler,
		shaders::meshlet::vertices_buffer,
		shaders::meshlet::instance_data_buffer,
		material_diffuse_in,
		material_diffuse_sampler
	>;

	template <typename TechniqueBindings>
	using shader_binding_types = type_pack_concat<leading_bindings, typename type_pack_concat<TechniqueBindings, trailing_bindings>::type>::type;

	using vertex_pulled_binding_types = type_pack_concat<leading_bindings, typename type_pack_concat<raster_bindings, vertex_pulled_trailing_bindings>::type>::type;

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
		vec3f sun_color;
		irradiance sun_ambient;
		irradiance shadow_cutoff;
		std::uint32_t frame_counter;
	};

	template <typename TechniqueBindings, fixed_string TechniqueHelper>
	using meshlet_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/meshlet_geometry">,
		gpu::types<shaders::common::shader_types, shaders::forward::shader_types, shaders::raster_shadow::shader_types>,
		gpu::bindings<shader_binding_types<TechniqueBindings>>,
		gpu::helpers<"Standard3D/meshlet_common", "Standard3D/gi_probe_sample", "Standard3D/light_eval", "Atmosphere/atmosphere_common", "Standard3D/lighting_shared", "Standard3D/lighting_common", "Standard3D/cube_faces", TechniqueHelper, "Standard3D/material_bindless", "Standard3D/forward_shading">,
		gpu::amplification_stage<"as_main">,
		gpu::mesh_stage<"ms_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<meshlet_push_constants>,
		gpu::color_targets<gpu::color_format::hdr>,
		gpu::depth<true, false, gpu::compare_op::greater_or_equal>
	>;

	using ray_traced_entry = meshlet_entry<ray_traced_bindings, "Standard3D/lighting_ray_traced">;

	using raster_entry = meshlet_entry<raster_bindings, "Standard3D/lighting_raster">;

	using vertex_pulled_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/vertex_geometry">,
		gpu::types<shaders::common::shader_types, shaders::forward::shader_types, shaders::raster_shadow::shader_types>,
		gpu::bindings<vertex_pulled_binding_types>,
		gpu::helpers<"Standard3D/meshlet_common", "Standard3D/gi_probe_sample", "Standard3D/light_eval", "Atmosphere/atmosphere_common", "Standard3D/lighting_shared", "Standard3D/lighting_common", "Standard3D/cube_faces", "Standard3D/lighting_raster", "Standard3D/material_single_texture", "Standard3D/forward_shading">,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<meshlet_push_constants>,
		gpu::color_targets<gpu::color_format::hdr>,
		gpu::depth<true, false, gpu::compare_op::greater_or_equal>
	>;
}

auto gse::renderer::forward::init(context& ctx, const shared_view<gpu::context::data> gpu_s, const shared_view<asset::data> assets_s, const shared_view<lighting::data> lighting_s, const shared_view<light_culling::data> lc_r, const shared_view<atmosphere::data> atm_state, const shared_view<geometry_collector::data> gc_state, data& d) -> async::task<> {
	d.vertex_pulled = !gpu_s.device->capabilities().mesh_shaders;
	if (d.vertex_pulled) {
		d.pipeline = gpu::build_graphics_program(*gpu_s.device, vertex_pulled_entry::pod);
	}
	else {
		d.pipeline = lighting_s.lighting_technique == lighting::technique::ray_traced
			? gpu::build_graphics_program(*gpu_s.device, ray_traced_entry::pod)
			: gpu::build_graphics_program(*gpu_s.device, raster_entry::pod);
	}

	constexpr std::size_t camera_ubo_size = sizeof(shaders::common::camera_data);
	constexpr std::size_t light_stride = sizeof(shaders::forward::light);
	constexpr std::size_t light_buffer_size = light_stride * max_lights;

	d.light_staging.reserve(light_buffer_size);

	d.gi_sampler = gpu_s.device->register_sampler(
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

	if (d.vertex_pulled) {
		constexpr std::array<std::byte, 4> white{ std::byte{ 0xFF }, std::byte{ 0xFF }, std::byte{ 0xFF }, std::byte{ 0xFF } };
		d.material_fallback = gpu_s.device->create_image(
			{
				.size = { 1, 1 },
				.format = gpu::image_format::r8g8b8a8_unorm,
				.usage = { gpu::image_flag::sampled, gpu::image_flag::transfer_dst },
			},
			"forward.material_fallback"
		);
		gpu_s.device->upload_image_2d(d.material_fallback, white.data());
	}

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		d.camera_ubo_buffers[i] = gpu_s.device->create_buffer(
			{
				.size = camera_ubo_size,
				.stride = sizeof(shaders::common::camera_data),
				.usage = gpu::buffer_flag::storage,
			},
			"forward.camera_ubo"
		);

		d.light_buffers[i] = gpu_s.device->create_buffer(
			{
				.size = light_buffer_size,
				.stride = sizeof(shaders::forward::light),
				.usage = gpu::buffer_flag::storage,
			},
			"forward.lights"
		);
	}

	return {};
}

auto gse::renderer::forward::frame(context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out, const channel_read<geometry_collector::render_data> geometry_in, shared_view<camera::data> cam_state, shared_view<geometry_collector::data> gc_r, shared_view<light_culling::data> lc_r, shared_view<atmosphere::data> atm_state, shared_view<cloud::data> cloud_state, const std::optional<shared_view<gi_probe::data>> gi_s, read<directional_light_component> dir_lights, read<spot_light_component> spot_lights, read<point_light_component> point_lights, read<physics::transform_component> transforms, const channel_read<camera::temporal_reset_request> reset_in, const shared_view<lighting::data> lighting_s, const channel_read<lighting_source::ray_traced_scene, lighting_source::raster_shadows, lighting_source::screen_ambient_occlusion, lighting_source::screen_reflections> lighting_in) -> async::task<> {
	if (!reset_in.of<camera::temporal_reset_request>().empty()) {
		d.frame_counter = 0;
	}

	const auto cache_latest = [&]<typename T>(std::optional<T>& slot) {
		if (const auto& published = lighting_in.of<T>(); !published.empty()) {
			slot = published[published.size() - 1];
		}
	};
	cache_latest(d.ray_traced_scene);
	cache_latest(d.raster_shadows);
	cache_latest(d.ambient_occlusion);
	cache_latest(d.reflections);

	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const bool ray_traced = lighting_s.lighting_technique == lighting::technique::ray_traced;
	const bool lighting_ready = ray_traced
		? d.ray_traced_scene.has_value()
		: d.raster_shadows.has_value() && d.ambient_occlusion.has_value() && d.reflections.has_value();

	const auto& render_items = geometry_in.of<geometry_collector::render_data>();
	if (render_items.empty() || !lighting_ready) {
		const auto ext = gpu_s.render_graph->extent();
		auto rec = co_await gpu::pass<^^frame>(pass_out)
			.color(
				gpu::clear_color(
					gpu::color_clear{ 0.0f, 0.0f, 0.0f, 1.0f },
					gpu_s.render_graph->framebuffer_image<targets::hdr_color>()
				)
			)
			.depth(gpu::clear_depth(gpu::depth_clear{
				.depth = 0.0f
			}));
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
	d.camera_ubo_buffers[frame_index].host_write(camera);

	const std::size_t total_lights = std::min(dir_lights.size() + spot_lights.size() + point_lights.size() + 1u,
		max_lights);
	auto& staging = d.light_staging;
	staging.assign(
		total_lights * sizeof(shaders::forward::light),
		std::byte{ 0 }
	);
	const std::span staging_lights(reinterpret_cast<shaders::forward::light*>(staging.data()), total_lights);
	const auto light_count = light_packing::pack(staging_lights, view, atm_state, dir_lights, spot_lights, point_lights, transforms);

	if (light_count > 0) {
		d.light_buffers[frame_index].host_write(staging.data(), light_count * sizeof(shaders::forward::light));
	}

	const auto& normal_batches = data.normal_batches;

	const auto ext = gpu_s.render_graph->extent();
	const int num_lights_i = static_cast<int>(light_count);
	const int shadow_quality_i = static_cast<int>(d.shadow_quality);
	const int ao_quality_i = static_cast<int>(d.ao_quality);
	const int reflection_quality_i = static_cast<int>(d.reflection_quality);

	const vec3u gi_grid_dim = gi_s ? gi_s->atlas_grid_dim : vec3u{};
	const vec2f gi_atlas_size_v{
		static_cast<float>(gi_grid_dim.x() * gi_probe::probe_tile_size),
		static_cast<float>(gi_grid_dim.y() * gi_grid_dim.z() * gi_probe::probe_tile_size),
	};
	const bool gi_enabled = gi_s && gi_s->quality != gi_probe::quality_level::off;

	std::array<id, 3> technique_pass_storage{};
	std::span<const id> technique_passes;
	if (ray_traced) {
		technique_pass_storage[0] = d.ray_traced_scene->pass;
		technique_passes = std::span(technique_pass_storage).first(1);
	}
	else {
		technique_pass_storage = {
			d.raster_shadows->pass,
			d.ambient_occlusion->pass,
			d.reflections->pass,
		};
		technique_passes = technique_pass_storage;
	}

	auto rec = co_await gpu::pass<^^frame>(pass_out)
		.pipeline(d.pipeline)
		.color(
			gpu::clear_color(
				gpu::color_clear{ 0.0f, 0.0f, 0.0f, 1.0f },
				gpu_s.render_graph->framebuffer_image<targets::hdr_color>()
			)
		)
		.depth(gpu::load_depth())
		.after(technique_passes)
		.after<^^light_culling::frame, ^^depth_prepass::frame, ^^atmosphere::ap_compute_pass, ^^gi_probe::frame, ^^skin::deform_pass, ^^cloud::cloud_shadow_pass>();
	rec.set_viewport(ext);
	rec.set_scissor(ext);

	if (normal_batches.empty()) {
		co_return;
	}

	const auto camera_ubo_slot = d.camera_ubo_buffers[frame_index].slot();
	const auto lights_slot = d.light_buffers[frame_index].slot();
	const auto light_index_list_slot = gpu_s.device->buffer_slot(lc_r.light_index_list_buffers[frame_index]);
	const auto tile_light_table_slot = gpu_s.device->buffer_slot(lc_r.tile_light_table_buffers[frame_index]);
	const auto material_palette_slot = gc_r.material_palette_buffers[frame_index].slot();
	const auto atmosphere_ubo_slot = atm_state.atmosphere_ubo_buffer.slot();
	const auto instance_data_slot = gc_r.instance_buffer[frame_index].slot();

	const auto aerial_perspective_slot = atm_state.ap_volume.sampled_slot();
	const auto ap_sampler_slot = atm_state.lut_sampler_bindless.slot();
	const auto gi_atlas_slot = gi_s ? gi_s->irradiance_atlas.sampled_slot() : gpu::bindless_slot{};
	const auto gi_atlas_sampler_slot = d.gi_sampler.slot();

	const auto cloud_shadow_slot = cloud_state.shadow_map.sampled_slot();
	const auto cloud_shadow_sampler_slot = cloud_state.shadow_sampler.slot();
	const auto cloud_shadow_ubo_slot = cloud_state.shadow_ubo_buffer.slot();

	const auto sky_view_slot = atm_state.sky_view_lut.sampled_slot();
	const auto sky_view_sampler_slot = atm_state.sky_view_sampler_bindless.slot();
	++d.frame_counter;

	const meshlet_push_constants base_pc{
		.num_lights = num_lights_i,
		.screen_size = vec2u{ ext.x(), ext.y() },
		.shadow_quality = shadow_quality_i,
		.ao_quality = ao_quality_i,
		.reflection_quality = reflection_quality_i,
		.gi_origin = gi_s ? gi_s->origin_world : vec3<position>{},
		.gi_spacing = gi_s ? gi_s->spacing : length{},
		.gi_grid_dim = gi_grid_dim,
		.gi_intensity = gi_s ? gi_s->intensity : 0.f,
		.gi_atlas_size = gi_atlas_size_v,
		.gi_enabled = gi_enabled ? 1 : 0,
		.sun_color = atm_state.sun_color * atm_state.sun_transmittance,
		.sun_ambient = atm_state.sun_ambient_strength * atm_state.sun_intensity,
		.shadow_cutoff = d.shadow_cutoff,
		.frame_counter = d.frame_counter,
	};

	const auto draw_batches = [&]<typename Entry>(const auto& bind_technique) {
		for (std::size_t i = 0; i < normal_batches.size(); ++i) {
			const auto& batch = normal_batches[i];
			const auto& mesh = batch.key.resolve_mesh();

			if (!mesh.has_meshlets()) {
				continue;
			}

			if (!mesh.upload_token().ready()) {
				continue;
			}

			const auto& ml = mesh.meshlet_gpu();

			gpu::binding_args<gpu::entry_bindings_pack_t<Entry>> bindings{
				.camera_ubo = camera_ubo_slot,
				.lights_ssbo = lights_slot,
				.light_index_list = light_index_list_slot,
				.tile_light_table = tile_light_table_slot,
				.material_palette = material_palette_slot,
				.aerial_perspective_in = aerial_perspective_slot,
				.atmosphere_ubo = atmosphere_ubo_slot,
				.gi_atlas = gi_atlas_slot,
				.ap_sampler = ap_sampler_slot,
				.gi_atlas_sampler = gi_atlas_sampler_slot,
				.cloud_shadow_ubo = cloud_shadow_ubo_slot,
				.cloud_shadow_in = cloud_shadow_slot,
				.cloud_shadow_sampler = cloud_shadow_sampler_slot,
				.sky_view_in = sky_view_slot,
				.sky_view_sampler = sky_view_sampler_slot,
				.vertices_buffer = batch.deformed_vertices.valid() ? batch.deformed_vertices : ml.vertex_storage.slot(),
				.instance_data_buffer = instance_data_slot,
			};
			bind_technique(bindings, ml);

			auto pc = base_pc;
			pc.first_instance = batch.first_instance;

			if constexpr (std::same_as<Entry, vertex_pulled_entry>) {
				const auto& diffuse = mesh.material().diffuse_texture;
				const auto diffuse_view = diffuse.valid() ? diffuse.resolve() : nullptr;
				bindings.material_diffuse_in = diffuse_view ? diffuse_view->gpu_image().sampled_slot() : d.material_fallback.sampled_slot();
				bindings.material_diffuse_sampler = d.material_sampler.slot();
				rec.push_bindings<Entry>(pc, bindings);
				rec.bind_index(mesh.index_gpu_buffer());
				rec.draw_indexed(static_cast<std::uint32_t>(mesh.indices().size()), batch.instance_count);
			}
			else {
				pc.meshlet_offset = 0;
				pc.meshlet_count = mesh.meshlet_count();
				rec.push_bindings<Entry>(pc, bindings);
				rec.draw_mesh_tasks_indirect(
					gc_r.normal_indirect_commands_buffer[frame_index],
					i * sizeof(gpu::draw_mesh_tasks_indirect_command),
					1,
					sizeof(gpu::draw_mesh_tasks_indirect_command)
				);
			}
		}
	};

	const auto bind_meshlets = [&](auto& bindings, const meshlet_gpu_data& ml) {
		bindings.meshlets_buffer = ml.descriptors.slot();
		bindings.meshlet_vertex_indices = ml.vertices.slot();
		bindings.meshlet_triangles = ml.triangles.slot();
		bindings.meshlet_bounds_buffer = ml.bounds.slot();
		bindings.textures_sampler = d.material_sampler.slot();
	};

	const auto bind_raster = [&](auto& bindings) {
		const auto& shadows = *d.raster_shadows;
		bindings.shadow_cascade_atlas = shadows.cascade_atlas;
		bindings.shadow_local_atlas = shadows.local_atlas;
		bindings.shadow_tiles = shadows.tiles[frame_index];
		bindings.light_shadow_tiles = shadows.light_tiles[frame_index];
		bindings.ambient_occlusion_in = d.ambient_occlusion->occlusion;
		bindings.reflection_in = d.reflections->radiance;
	};

	if (d.vertex_pulled) {
		draw_batches.template operator()<vertex_pulled_entry>(
			[&](auto& bindings, const meshlet_gpu_data&) {
				bind_raster(bindings);
			}
		);
	}
	else if (ray_traced) {
		const auto& scene = *d.ray_traced_scene;
		draw_batches.template operator()<ray_traced_entry>(
			[&](auto& bindings, const meshlet_gpu_data& ml) {
				bind_meshlets(bindings, ml);
				bindings.scene_tlas = gpu::make_acceleration_structure_arg(scene.tlas_addresses[frame_index], scene.tlas_slots[frame_index]);
			}
		);
	}
	else {
		draw_batches.template operator()<raster_entry>(
			[&](auto& bindings, const meshlet_gpu_data& ml) {
				bind_meshlets(bindings, ml);
				bind_raster(bindings);
			}
		);
	}
}

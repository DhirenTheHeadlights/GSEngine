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
import :camera_latch_renderer;
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
	struct material_texture_bindings {
		shaders::texture2d<vec4f> material_diffuse_in;
		shaders::sampler_state material_diffuse_sampler;
	};

	using surface_pass = gpu::specs<
		gpu::types<shaders::common::shader_types, shaders::forward::shader_types, shaders::raster_shadow::shader_types>,
		gpu::push_constant<meshlet_push_constants>,
		gpu::color_targets<gpu::color_format::hdr>,
		gpu::depth<true, false, gpu::compare_op::greater_or_equal>
	>;

	template <typename TechniqueBindings, fixed_string TechniqueHelper>
	using meshlet_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/meshlet_geometry">,
		forward_bindings<TechniqueBindings, shaders::meshlet::vertices, shaders::meshlet::meshlets, shaders::meshlet::instances, shaders::bindless::sampler>,
		gpu::bindless_textures,
		surface_helpers<TechniqueHelper, "Standard3D/material_bindless">,
		gpu::amplification_stage<>,
		gpu::mesh_stage<>,
		gpu::fragment_stage<>,
		surface_pass
	>;

	using ray_traced_entry = meshlet_entry<ray_traced_bindings, "Standard3D/lighting_ray_traced">;

	using raster_entry = meshlet_entry<raster_bindings, "Standard3D/lighting_raster">;

	using vertex_pulled_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/vertex_geometry">,
		forward_bindings<raster_bindings, shaders::meshlet::vertices, shaders::meshlet::instances, material_texture_bindings>,
		surface_helpers<"Standard3D/lighting_raster", "Standard3D/material_single_texture">,
		gpu::vertex_stage<>,
		gpu::fragment_stage<>,
		surface_pass
	>;
}

auto gse::renderer::forward::init(context& ctx, const shared_view<gpu::context::data> gpu_s, const shared_view<asset::data> assets_s, const shared_view<lighting::data> lighting_s, const shared_view<light_culling::data> lc_r, const shared_view<atmosphere::data> atm_state, const shared_view<geometry_collector::data> gc_state, data& d) -> async::task<> {
	d.vertex_pulled = !gpu_s.device->capabilities().mesh_shaders;
	d.ray_traced = lighting_s.lighting_technique == lighting::technique::ray_traced;
	if (d.vertex_pulled) {
		d.pipeline = gpu::build_graphics_program(*gpu_s.device, vertex_pulled_entry::pod);
	}
	else {
		d.pipeline = d.ray_traced
			? gpu::build_graphics_program(*gpu_s.device, ray_traced_entry::pod)
			: gpu::build_graphics_program(*gpu_s.device, raster_entry::pod);
	}

	d.light_view_pipeline = gpu::build_compute_program(*gpu_s.device, light_packing::view_entry::pod);

	constexpr std::size_t light_stride = sizeof(shaders::forward::light);
	constexpr std::size_t light_buffer_size = light_stride * max_lights;

	d.light_staging.reserve(light_buffer_size);

	d.gi_sampler = gpu_s.device->register_sampler(gpu::linear_clamp_sampler);

	d.material_sampler = gpu_s.device->register_sampler({});

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
		d.light_buffers[i] = gpu_s.device->create_buffer(
			{
				.size = light_buffer_size,
				.stride = sizeof(shaders::forward::light),
				.usage = gpu::buffer_flag::storage,
				.writable = true,
			},
			"forward.lights"
		);
	}

	return {};
}

auto gse::renderer::forward::frame(context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out, const channel_read<geometry_collector::render_data> geometry_in, shared_view<camera_latch::data> latch_s, shared_view<geometry_collector::data> gc_r, shared_view<light_culling::data> lc_r, shared_view<atmosphere::data> atm_state, shared_view<cloud::data> cloud_state, const std::optional<shared_view<gi_probe::data>> gi_s, read<directional_light_component> dir_lights, read<spot_light_component> spot_lights, read<point_light_component> point_lights, read<physics::transform_component> transforms, const channel_read<camera::temporal_reset_request> reset_in, const shared_view<lighting::data> lighting_s, const channel_read<lighting_source::ray_traced_scene, lighting_source::raster_shadows, lighting_source::screen_ambient_occlusion, lighting_source::screen_reflections> lighting_in) -> async::task<> {
	d.surface.reset();
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

	const std::size_t total_lights = std::min(dir_lights.size() + spot_lights.size() + point_lights.size() + 1u,
		max_lights);
	auto& staging = d.light_staging;
	staging.assign(
		total_lights * sizeof(shaders::forward::light),
		std::byte{ 0 }
	);
	const std::span staging_lights(reinterpret_cast<shaders::forward::light*>(staging.data()), total_lights);
	const auto light_count = light_packing::pack(staging_lights, atm_state, dir_lights, spot_lights, point_lights, transforms);

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

	const auto instance_data_slot = gc_r.instance_buffer[frame_index].slot();
	++d.frame_counter;

	surface_frame surface{
		.camera_ubo = latch_s.camera_buffers[frame_index].slot(),
		.lights = d.light_buffers[frame_index].slot(),
		.light_index_list = gpu_s.device->buffer_slot(lc_r.light_index_list_buffers[frame_index]),
		.tile_light_table = gpu_s.device->buffer_slot(lc_r.tile_light_table_buffers[frame_index]),
		.aerial_perspective = atm_state.ap_volume.sampled_slot(),
		.gi_atlas = gi_s ? gi_s->irradiance_atlas.sampled_slot() : gpu::bindless_slot{},
		.ap_sampler = atm_state.lut_sampler_bindless.slot(),
		.gi_atlas_sampler = d.gi_sampler.slot(),
		.cloud_shadow = cloud_state.shadow_map.sampled_slot(),
		.cloud_shadow_sampler = cloud_state.shadow_sampler.slot(),
		.material_palette = gc_r.material_palette_buffers[frame_index].slot(),
		.atmosphere_ubo = atm_state.atmosphere_ubo_buffer.slot(),
		.cloud_shadow_ubo = cloud_state.shadow_ubo_buffer.slot(),
		.sky_view = atm_state.sky_view_lut.sampled_slot(),
		.sky_view_sampler = atm_state.sky_view_sampler_bindless.slot(),
	};
	if (ray_traced) {
		const auto& scene = *d.ray_traced_scene;
		surface.scene_tlas = gpu::make_acceleration_structure_arg(scene.tlas_addresses[frame_index], scene.tlas_slots[frame_index]);
	}
	else {
		const auto& shadows = *d.raster_shadows;
		surface.shadow_cascade_atlas = shadows.cascade_atlas;
		surface.shadow_local_atlas = shadows.local_atlas;
		surface.shadow_tiles = shadows.tiles[frame_index];
		surface.light_shadow_tiles = shadows.light_tiles[frame_index];
		surface.ambient_occlusion = d.ambient_occlusion->occlusion;
		surface.reflection = d.reflections->radiance;
	}

	surface.push_constants = {
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
	d.surface = surface;

	{
		auto view_rec = co_await gpu::pass<^^light_view_pass>(pass_out).pipeline(d.light_view_pipeline).after<^^camera_latch::frame>();
		view_rec.dispatch<light_packing::view_entry>(
			{
				.light_count = static_cast<std::uint32_t>(light_count),
			},
			{
				.camera_ubo = surface.camera_ubo,
				.lights = surface.lights,
			},
			gpu::group_count<light_packing::view_entry>(static_cast<std::uint32_t>(light_count))
		);
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
		.after<^^light_culling::frame, ^^light_view_pass, ^^depth_prepass::frame, ^^atmosphere::ap_compute_pass, ^^gi_probe::frame, ^^skin::deform_pass, ^^cloud::cloud_shadow_pass>();
	rec.set_viewport(ext);
	rec.set_scissor(ext);

	if (normal_batches.empty()) {
		co_return;
	}

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

			gpu::entry_binding_args<Entry> bindings{
				.vertices_buffer = batch.deformed_vertices.valid() ? batch.deformed_vertices : ml.vertex_storage.slot(),
				.instance_data_buffer = instance_data_slot,
			};
			bind_surface(bindings, surface);
			bind_technique(bindings, ml);

			auto pc = surface.push_constants;
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

	if (d.vertex_pulled) {
		draw_batches.template operator()<vertex_pulled_entry>([](auto&, const meshlet_gpu_data&) {});
	}
	else if (ray_traced) {
		draw_batches.template operator()<ray_traced_entry>(bind_meshlets);
	}
	else {
		draw_batches.template operator()<raster_entry>(bind_meshlets);
	}
}

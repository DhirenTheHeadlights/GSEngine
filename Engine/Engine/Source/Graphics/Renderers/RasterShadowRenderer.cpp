module gse.graphics:raster_shadow_renderer_impl;

import std;

import :atmosphere_renderer;
import :camera_system;
import :directional_light;
import :forward_renderer;
import :geometry_collector;
import :light_packing;
import :lighting_source;
import :mesh;
import :physics_transform_renderer;
import :point_light;
import :raster_shadow_renderer;
import :shared_shaders;
import :skin_renderer;
import :spot_light;

import gse.concurrency;
import gse.containers;
import gse.core;
import gse.ecs;
import gse.gpu;
import gse.gpu_record;
import gse.math;
import gse.physics;

namespace gse::renderer::raster_shadow {
	struct [[= shaders::shader_constant_block]]
	raster_shadow_limits {
		std::uint32_t cascade_count = raster_shadow::cascade_count;
		length local_near_plane = meters(0.05f);
		length caster_reach = meters(200.f);
		length cascade_near = meters(0.5f);
		float cascade_split_blend = 0.8f;
		angle local_fov_margin = degrees(2.f);
		angle spot_max_half_angle = degrees(85.f);
	};

	struct [[= shaders::uniform_block]] camera_ubo {
		using element = shaders::common::camera_data;
	};

	struct [[= shaders::ssbo_readonly]] lights {
		using element = shaders::forward::light;
	};

	struct [[= shaders::ssbo_readonly]] tile_requests {
		using element = tile_request;
	};

	struct [[= shaders::ssbo_readwrite]] tiles_out {
		using element = shaders::raster_shadow::shadow_tile;
	};

	struct [[= shaders::ssbo_readonly]] tiles {
		using element = shaders::raster_shadow::shadow_tile;
	};

	struct [[= shaders::shader_struct]]
	setup_push_constants {
		std::uint32_t tile_count;
		std::int32_t shadow_quality;
	};

	using setup_bindings = type_pack<camera_ubo, lights, tile_requests, tiles_out>;

	using setup_entry = gpu::compute_entry<
		gpu::body_path<"Compute/raster_shadow_setup">,
		gpu::types<shaders::common::shader_types, shaders::forward::shader_types, shaders::raster_shadow::shader_types, type_pack<tile_request, raster_shadow_limits>>,
		gpu::bindings<setup_bindings>,
		gpu::helpers<"Standard3D/lighting_shared", "Standard3D/cube_faces">,
		gpu::threads<64>,
		gpu::push_constant<setup_push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	struct [[= shaders::shader_struct]]
	depth_push_constants {
		std::uint32_t meshlet_offset;
		std::uint32_t meshlet_count;
		std::uint32_t first_instance;
		std::uint32_t tile_index;
	};

	using depth_bindings = type_pack<
		tiles,
		shaders::meshlet::vertices_buffer,
		shaders::meshlet::meshlets_buffer,
		shaders::meshlet::meshlet_vertex_indices,
		shaders::meshlet::meshlet_triangles,
		shaders::meshlet::meshlet_bounds_buffer,
		shaders::meshlet::instance_data_buffer
	>;

	using depth_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/raster_shadow_depth">,
		gpu::types<shaders::common::shader_types, shaders::forward::shader_types, shaders::raster_shadow::shader_types>,
		gpu::bindings<depth_bindings>,
		gpu::helpers<"Standard3D/meshlet_common">,
		gpu::amplification_stage<"as_main">,
		gpu::mesh_stage<"ms_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<depth_push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::depth<true, true, gpu::compare_op::greater_or_equal>,
		gpu::color_targets<>
	>;

	struct [[= shaders::shader_struct]]
	vertex_depth_push_constants {
		std::uint32_t first_instance;
		std::uint32_t tile_index;
	};

	using vertex_depth_bindings = type_pack<
		tiles,
		shaders::meshlet::vertices_buffer,
		shaders::meshlet::instance_data_buffer
	>;

	using vertex_depth_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/raster_shadow_depth_vertex">,
		gpu::types<shaders::common::shader_types, shaders::forward::shader_types, shaders::raster_shadow::shader_types>,
		gpu::bindings<vertex_depth_bindings>,
		gpu::helpers<"Standard3D/meshlet_common">,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<vertex_depth_push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::depth<true, true, gpu::compare_op::greater_or_equal>,
		gpu::color_targets<>
	>;

	auto create_atlas(
		shared_view<gpu::context::data> gpu_s,
		std::uint32_t extent,
		std::string_view tag
	) -> gpu::image;

	auto local_tile_offset(
		std::uint32_t tile
	) -> vec2u;

	auto record_tiles(
		gpu::recording_context& rec,
		const geometry_collector::render_data& geometry,
		std::span<const tile_request> requests,
		std::uint32_t first_tile,
		gpu::bindless_slot tiles_slot,
		gpu::bindless_slot instance_slot,
		bool vertex_pulled
	) -> void;
}

auto gse::renderer::raster_shadow::create_atlas(const shared_view<gpu::context::data> gpu_s, const std::uint32_t extent, const std::string_view tag) -> gpu::image {
	auto atlas = gpu_s.device->create_image(
		{
			.size = { extent, extent },
			.format = gpu::image_format::d32_sfloat,
			.usage = { gpu::image_flag::sampled, gpu::image_flag::depth_attachment },
		},
		tag
	);
	gpu::transition_image_to(*gpu_s.device, atlas);
	return atlas;
}

auto gse::renderer::raster_shadow::local_tile_offset(const std::uint32_t tile) -> vec2u {
	return {
		(tile % local_tiles_per_row) * local_tile_extent,
		(tile / local_tiles_per_row) * local_tile_extent,
	};
}

auto gse::renderer::raster_shadow::record_tiles(gpu::recording_context& rec, const geometry_collector::render_data& geometry, const std::span<const tile_request> requests, const std::uint32_t first_tile, const gpu::bindless_slot tiles_slot, const gpu::bindless_slot instance_slot, const bool vertex_pulled) -> void {
	for (std::size_t t = 0; t < requests.size(); ++t) {
		const auto& request = requests[t];
		const auto x = static_cast<float>(request.atlas_offset.x());
		const auto y = static_cast<float>(request.atlas_offset.y());
		const auto size = static_cast<float>(request.atlas_extent);
		rec.set_viewport(x, y, size, size);
		rec.set_scissor(static_cast<std::int32_t>(request.atlas_offset.x()), static_cast<std::int32_t>(request.atlas_offset.y()), request.atlas_extent, request.atlas_extent);

		for (const auto& batch : geometry.normal_batches) {
			const auto& mesh = batch.key.resolve_mesh();
			if (!mesh.has_meshlets() || !mesh.upload_token().ready() || batch.instance_count == 0) {
				continue;
			}

			const auto& ml = mesh.meshlet_gpu();
			const auto vertices_slot = batch.deformed_vertices.valid() ? batch.deformed_vertices : ml.vertex_storage.slot();
			const auto tile_index = first_tile + static_cast<std::uint32_t>(t);

			if (vertex_pulled) {
				rec.push_bindings<vertex_depth_entry>(
					{
						.first_instance = batch.first_instance,
						.tile_index = tile_index,
					},
					{
						.tiles = tiles_slot,
						.vertices_buffer = vertices_slot,
						.instance_data_buffer = instance_slot,
					}
				);
				rec.bind_index(mesh.index_gpu_buffer());
				rec.draw_indexed(static_cast<std::uint32_t>(mesh.indices().size()), batch.instance_count);
				continue;
			}

			const std::uint32_t meshlet_count = mesh.meshlet_count();

			rec.push_bindings<depth_entry>(
				{
					.meshlet_offset = 0,
					.meshlet_count = meshlet_count,
					.first_instance = batch.first_instance,
					.tile_index = tile_index,
				},
				{
					.tiles = tiles_slot,
					.vertices_buffer = vertices_slot,
					.meshlets_buffer = ml.descriptors.slot(),
					.meshlet_vertex_indices = ml.vertices.slot(),
					.meshlet_triangles = ml.triangles.slot(),
					.meshlet_bounds_buffer = ml.bounds.slot(),
					.instance_data_buffer = instance_slot,
				}
			);
			rec.draw_mesh_tasks((meshlet_count + 31) / 32, batch.instance_count, 1);
		}
	}
}

auto gse::renderer::raster_shadow::init(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	d.setup_pipeline = gpu::build_compute_program(*gpu_s.device, setup_entry::pod);
	d.vertex_pulled = !gpu_s.device->capabilities().mesh_shaders;
	d.depth_pipeline = d.vertex_pulled
		? gpu::build_graphics_program(*gpu_s.device, vertex_depth_entry::pod)
		: gpu::build_graphics_program(*gpu_s.device, depth_entry::pod);

	d.cascade_atlas = create_atlas(gpu_s, cascade_atlas_extent, "raster_shadow.cascades");
	d.local_atlas = create_atlas(gpu_s, local_atlas_extent, "raster_shadow.local");

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		d.camera_ubo_buffers[i] = gpu_s.device->create_buffer(
			{
				.size = sizeof(shaders::common::camera_data),
				.stride = sizeof(shaders::common::camera_data),
				.usage = gpu::buffer_flag::storage,
			},
			"raster_shadow.camera_ubo"
		);
		d.light_buffers[i] = gpu_s.device->create_buffer(
			{
				.size = sizeof(shaders::forward::light) * max_lights,
				.stride = sizeof(shaders::forward::light),
				.usage = gpu::buffer_flag::storage,
			},
			"raster_shadow.lights"
		);
		d.request_buffers[i] = gpu_s.device->create_buffer(
			{
				.size = sizeof(tile_request) * max_tiles,
				.stride = sizeof(tile_request),
				.usage = gpu::buffer_flag::storage,
			},
			"raster_shadow.requests"
		);
		d.tile_buffers[i] = gpu_s.device->create_buffer(
			{
				.size = sizeof(shaders::raster_shadow::shadow_tile) * max_tiles,
				.stride = sizeof(shaders::raster_shadow::shadow_tile),
				.usage = gpu::buffer_flag::storage,
				.writable = true,
			},
			"raster_shadow.tiles"
		);
		d.light_tile_buffers[i] = gpu_s.device->create_buffer(
			{
				.size = sizeof(vec2u) * max_lights,
				.stride = sizeof(vec2u),
				.usage = gpu::buffer_flag::storage,
			},
			"raster_shadow.light_tiles"
		);
	}

	d.packed_lights.reserve(max_lights);
	d.packed_lights.resize(max_lights);
	d.requests.reserve(max_tiles);
	d.light_tiles.reserve(max_lights);
	d.local_candidates.reserve(max_lights);

	return {};
}

auto gse::renderer::raster_shadow::frame(context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out, const channel_write<lighting_source::raster_shadows> shadows_out, const channel_read<geometry_collector::render_data> geometry_in, shared_view<geometry_collector::data> gc_r, shared_view<camera::data> cam_state, shared_view<atmosphere::data> atm_state, shared_view<forward::data> forward_s, read<directional_light_component> dir_lights, read<spot_light_component> spot_lights, read<point_light_component> point_lights, read<physics::transform_component> transforms) -> async::task<> {
	lighting_source::raster_shadows published{
		.cascade_atlas = d.cascade_atlas.sampled_slot(),
		.local_atlas = d.local_atlas.sampled_slot(),
		.pass = gpu::pass_id<^^frame>(),
	};
	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		published.tiles[i] = d.tile_buffers[i].slot();
		published.light_tiles[i] = d.light_tile_buffers[i].slot();
	}
	shadows_out.push<lighting_source::raster_shadows>(std::move(published));

	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto& render_items = geometry_in.of<geometry_collector::render_data>();
	if (render_items.empty() || forward_s.shadow_quality == forward::shadow_quality_level::off) {
		co_return;
	}

	const auto& geometry = render_items[0];
	const auto frame_index = gpu_s.render_graph->current_frame();
	const auto view = cam_state.view_matrix;
	const auto proj = cam_state.projection_matrix;
	const auto camera_position = view.inverse().transform_point(vec3<position>{});

	d.camera_ubo_buffers[frame_index].host_write(shaders::common::camera_data{
		.view = view,
		.proj = proj,
		.inv_view = view.inverse(),
		.inv_view_proj = (proj * view).inverse(),
		.prev_view = cam_state.prev_view_matrix,
		.prev_proj = cam_state.prev_projection_matrix,
		.jitter_ndc = cam_state.jitter_ndc,
		.prev_jitter_ndc = cam_state.prev_jitter_ndc,
	});

	const auto light_count = light_packing::pack(d.packed_lights.span(), view, atm_state, dir_lights, spot_lights, point_lights, transforms);
	d.light_buffers[frame_index].host_write(d.packed_lights.data(), light_count * sizeof(shaders::forward::light));

	d.requests.clear();
	d.light_tiles.assign(light_count, vec2u{ 0, 0 });

	for (std::uint32_t c = 0; c < cascade_count; ++c) {
		d.requests.push_back({
			.light_index = 0,
			.face = c,
			.atlas_offset = { (c % 2) * cascade_extent, (c / 2) * cascade_extent },
			.atlas_extent = cascade_extent,
		});
	}
	d.light_tiles[0] = { 0, cascade_count };

	d.local_candidates.clear();
	for (std::uint32_t i = 0; i < light_count; ++i) {
		const auto& l = d.packed_lights[i];
		if (l.light_type == shaders::forward::light_type::directional) {
			continue;
		}
		d.local_candidates.push_back({
			.distance = length(magnitude(l.world_position - camera_position) - l.radius),
			.light_index = i,
		});
	}
	std::ranges::sort(d.local_candidates.span(), {}, &local_candidate::distance);

	std::uint32_t local_used = 0;
	for (const auto& [distance, index] : d.local_candidates) {
		const auto& l = d.packed_lights[index];
		const std::uint32_t faces = l.light_type == shaders::forward::light_type::point ? 6u : 1u;
		if (local_used + faces > local_tile_count) {
			continue;
		}
		d.light_tiles[index] = { static_cast<std::uint32_t>(d.requests.size()), faces };
		for (std::uint32_t f = 0; f < faces; ++f) {
			d.requests.push_back({
				.light_index = index,
				.face = f,
				.atlas_offset = local_tile_offset(local_used + f),
				.atlas_extent = local_tile_extent,
			});
		}
		local_used += faces;
	}

	const auto tile_count = static_cast<std::uint32_t>(d.requests.size());
	d.request_buffers[frame_index].host_write(d.requests.data(), tile_count * sizeof(tile_request));
	d.light_tile_buffers[frame_index].host_write(d.light_tiles.data(), light_count * sizeof(vec2u));

	{
		auto rec = co_await gpu::pass<^^setup_pass>(pass_out).pipeline(d.setup_pipeline);
		rec.dispatch<setup_entry>(
			{
				.tile_count = tile_count,
				.shadow_quality = static_cast<std::int32_t>(forward_s.shadow_quality),
			},
			{
				.camera_ubo = d.camera_ubo_buffers[frame_index].slot(),
				.lights = d.light_buffers[frame_index].slot(),
				.tile_requests = d.request_buffers[frame_index].slot(),
				.tiles_out = d.tile_buffers[frame_index].slot(),
			},
			vec3u{ (tile_count + 63) / 64, 1u, 1u }
		);
	}

	const auto tiles_slot = d.tile_buffers[frame_index].slot();
	const auto instance_slot = gc_r.instance_buffer[frame_index].slot();
	const auto requests = d.requests.span();

	{
		auto rec = co_await gpu::pass<^^cascade_pass>(pass_out)
			.pipeline(d.depth_pipeline)
			.depth(gpu::clear_depth(gpu::depth_clear{ 0.0f }, d.cascade_atlas))
			.after<^^setup_pass, ^^skin::deform_pass, ^^physics_transform::frame>();
		record_tiles(rec, geometry, requests.first(cascade_count), 0, tiles_slot, instance_slot, d.vertex_pulled);
	}

	auto rec = co_await gpu::pass<^^frame>(pass_out)
		.pipeline(d.depth_pipeline)
		.depth(gpu::clear_depth(gpu::depth_clear{ 0.0f }, d.local_atlas))
		.after<^^cascade_pass>();
	record_tiles(rec, geometry, requests.subspan(cascade_count), cascade_count, tiles_slot, instance_slot, d.vertex_pulled);
}

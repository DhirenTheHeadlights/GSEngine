module gse.graphics:gi_probe_renderer_impl;

import std;

import :gi_probe_renderer;
import :atmosphere_renderer;
import :camera_system;
import :directional_light;
import :geometry_collector;
import :light_packing;
import :point_light;
import :rt_shadow_renderer;
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
	};

	struct [[= shaders::shader_constant_block]] gi_probe_limits {
		float surface_sky_response = 0.1f;
		float temporal_blend = 0.0625f;
	};

	struct [[
		= shaders::binding<0, 0>{},
		= shaders::tlas
	]] scene_tlas {};

	struct [[
		= shaders::binding<0, 1>{},
		= shaders::storage_image
	]] irradiance_atlas_out {
		using element = vec4f;
	};

	struct [[
		= shaders::binding<0, 2>{},
		= shaders::ssbo_readonly
	]] material_palette {
		using element = shaders::forward::material_data;
	};

	struct [[
		= shaders::binding<0, 3>{},
		= shaders::texture2d
	]] sky_view_in {
		using element = vec4f;
	};

	struct [[
		= shaders::binding<0, 4>{},
		= shaders::sampler_state
	]] sky_view_sampler {};

	struct [[
		= shaders::binding<0, 5>{},
		= shaders::ssbo_readonly
	]] lights {
		using element = shaders::forward::light;
	};

	using shader_binding_types = type_pack<scene_tlas, irradiance_atlas_out, material_palette, sky_view_in, sky_view_sampler, lights, atmosphere::atmosphere_ubo>;

	using entry = gpu::compute_entry<
		gpu::body_path<"Compute/gi_probe_update">,
		gpu::types<shaders::forward::shader_types, type_pack<gi_probe_limits>>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"Atmosphere/atmosphere_common", "Standard3D/light_eval">,
		gpu::threads<rays_per_probe>,
		gpu::push_constant<push_constants>,
		gpu::system_values<gpu::dispatch_thread_id, gpu::group_id, gpu::group_thread_id>
	>;

	auto atlas_extent(
		vec3u dim
	) -> vec2u;

	auto recreate_atlas(
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> void;

	auto rebind_tlas_views(
		shared_view<gpu::context::data> gpu_s,
		shared_view<rt_shadow::data> rt_state,
		data& d
	) -> void;
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
			.bindless = true,
		},
		"gi_irradiance_atlas"
	);
	gpu::transition_image_to(*gpu_s.device, d.irradiance_atlas);
}

auto gse::renderer::gi_probe::rebind_tlas_views(const shared_view<gpu::context::data> gpu_s, const shared_view<rt_shadow::data> rt_state, data& d) -> void {
	for (std::size_t i = 0; i < per_frame_resource<gpu::bindless_handle>::frames_in_flight; ++i) {
		const auto fi = static_cast<std::uint32_t>(i);
		if (!d.tlas_views[i].valid()) {
			d.tlas_views[i] = gpu_s.device->allocate_acceleration_structure_slot();
		}
		const auto tlas_address = (*rt_state.tlas_ptrs[fi]).device_address();
		d.tlas_addresses[i] = tlas_address;
		gpu_s.device->write_acceleration_structure(d.tlas_views[i].slot(), tlas_address);
	}
}

auto gse::renderer::gi_probe::init(context& ctx, const shared_view<gpu::context::data> gpu_s, const shared_view<rt_shadow::data> rt_state, const shared_view<geometry_collector::data> gc_state, data& d) -> async::task<> {
	d.update_pipeline = gpu::build_compute_program(*gpu_s.device, entry::pod);
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
	rebind_tlas_views(gpu_s, rt_state, d);

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		d.light_buffers[i] = gpu_s.device->create_buffer(
			{
				.size = sizeof(shaders::forward::light) * max_lights,
				.stride = sizeof(shaders::forward::light),
				.usage = gpu::buffer_flag::storage,
				.bindless = true
			}
		);
	}
	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		log::println(
			log::category::render,
			"gi_probe: material frame={} slot={} address=0x{:x} bytes={}",
			i,
			gc_state.material_palette_buffers[i].slot().index,
			gc_state.material_palette_buffers[i].device_address(),
			gc_state.material_palette_buffers[i].size()
		);
	}

	gpu::context::on_swap_chain_recreate(
		gpu_s,
		[gpu_s, rt_state, &d]() {
			rebind_tlas_views(gpu_s, rt_state, d);
		}
	);

	return {};
}

auto gse::renderer::gi_probe::frame(context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out, const channel_read<geometry_collector::render_data> geometry_in, shared_view<camera::data> cam_state, shared_view<atmosphere::data> atm_state, shared_view<geometry_collector::data> gc_r, read<directional_light_component> dir_lights, read<spot_light_component> spot_lights, read<point_light_component> point_lights, read<physics::transform_component> transforms) -> async::task<> {
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

	auto rec = co_await gpu::pass<^^frame>(pass_out).pipeline(d.update_pipeline).after<^^rt_shadow::frame, ^^atmosphere::sky_view_pass>();

	rec.sample_image(atm_state.sky_view_lut, gpu::pipeline_stage_flag::compute_shader);

	rec.dispatch<entry>(
		{
			.origin_world = d.origin_world,
			.spacing = d.spacing,
			.grid_dim_uniform = active_grid_dim,
			.frame_counter = d.frame_counter,
			.trace_t_max = d.trace_t_max,
			.light_count = static_cast<std::uint32_t>(light_count),
		},
		{
			.scene_tlas = gpu::make_acceleration_structure_arg(d.tlas_addresses[frame_index], d.tlas_views[frame_index].slot()),
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
}
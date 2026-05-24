module gse.graphics;

import std;

import :gi_probe_renderer;
import :atmosphere_renderer;
import :camera_system;
import :geometry_collector;
import :rt_shadow_renderer;
import :shared_shaders;

import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.meta;

namespace gse::renderer::gi_probe {
	struct [[= shaders::shader_struct]] push_constants {
		vec3<position> origin_world;
		length spacing;
		vec3u grid_dim_uniform;
		std::uint32_t frame_counter;
		vec3f sun_direction;
		irradiance sun_intensity;
		vec3f sun_color;
		length trace_t_max;
		vec3f sky_color;
		float intensity;
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

	using shader_binding_types = type_pack<scene_tlas, irradiance_atlas_out, material_palette>;

	using entry = gpu::compute_entry<
		gpu::body_path<"Compute/gi_probe_update">,
		gpu::layout<"gi_probe_update">,
		gpu::types<shaders::forward::shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::threads<rays_per_probe>,
		gpu::push_constant<push_constants>,
		gpu::system_values<gpu::dispatch_thread_id, gpu::group_id, gpu::group_thread_id>
	>;

	auto atlas_extent() -> vec2u;

	auto recreate_atlas(
		const gpu::context::data& gpu_s,
		system::data& d
	) -> void;

	auto rewrite_descriptors(
		const gpu::context::data& gpu_s,
		const rt_shadow::system::data& rt_state,
		const geometry_collector::system::data& gc_state,
		system::data& d
	) -> void;
}

auto gse::renderer::gi_probe::atlas_extent() -> vec2u {
	return {
		grid_dim.x() * probe_tile_size,
		grid_dim.y() * grid_dim.z() * probe_tile_size,
	};
}

auto gse::renderer::gi_probe::recreate_atlas(const gpu::context::data& gpu_s, system::data& d) -> void {
	const auto ext = atlas_extent();
	d.irradiance_atlas = gpu::image::create(
		gpu_s.device->allocator(),
		{
			.size = ext,
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.usage = gpu::image_flag::storage | gpu::image_flag::sampled,
		},
		"gi_irradiance_atlas"
	);
	gpu::transition_image_to(*gpu_s.device, d.irradiance_atlas);
}

auto gse::renderer::gi_probe::rewrite_descriptors(const gpu::context::data& gpu_s, const rt_shadow::system::data& rt_state, const geometry_collector::system::data& gc_state, system::data& d) -> void {
	constexpr std::size_t material_buffer_size =
		geometry_collector::system::data::max_materials * sizeof(shaders::forward::material_data);
	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		const auto fi = static_cast<std::uint32_t>(i);
		gpu::descriptor_writer writer(
			gpu_s.device->handle(),
			d.update_descriptors[i]
		);
		writer.acceleration_structure<scene_tlas>((*rt_state.tlas_ptrs[fi]).handle());
		writer.storage_image<irradiance_atlas_out>(d.irradiance_atlas);
		writer.buffer<material_palette>(gc_state.material_palette_buffers[fi], 0, material_buffer_size);
		writer.commit();
	}
}

auto gse::renderer::gi_probe::system::run(run_context& ctx, const gpu::context::data& gpu_s, const rt_shadow::system::data& rt_state, const geometry_collector::system::data& gc_state, data& d) -> async::task<> {
	d.update_pipeline =
		gpu::build_compute_program(
			*gpu_s.device,
			*gpu_s.shader_registry,
			*gpu_s.bindless_textures,
			entry::pod
		);

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		d.update_descriptors[i] =
			gpu::allocate_descriptors(
				*gpu_s.shader_registry,
				gpu_s.device->descriptor_heap(),
				entry::pod
			);
	}

	recreate_atlas(gpu_s, d);
	rewrite_descriptors(gpu_s, rt_state, gc_state, d);

	gpu::context::on_swap_chain_recreate(gpu_s, [&gpu_s, &rt_state, &gc_state, &d]() {
		rewrite_descriptors(gpu_s, rt_state, gc_state, d);
	});

	co_return;
}

auto gse::renderer::gi_probe::system::frame(frame_context& ctx, shared_view<gpu::context> gpu_s, data& d, shared_view<camera::system> cam_state, shared_view<atmosphere::system> atm_state, shared_view<geometry_collector::system> gc_r) -> async::task<> {
	if (d.quality == quality_level::off) {
		co_return;
	}

	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	if (!d.irradiance_atlas.handle()) {
		co_return;
	}

	const auto inv_view = cam_state.view_matrix.inverse();
	const auto cam_world = inv_view.transform_point(vec3<position>{});

	const length half_x = (static_cast<float>(grid_dim.x() - 1) * 0.5f) * d.spacing;
	const length half_z = (static_cast<float>(grid_dim.z() - 1) * 0.5f) * d.spacing;
	d.origin_world = vec3<position>(
		cam_world.x() - half_x,
		meters(1.0f),
		cam_world.z() - half_z
	);

	const auto frame_index = gpu_s.render_graph->current_frame();

	const gpu::typed_push_constants<push_constants> pc{
		.data = {
			.origin_world = d.origin_world,
			.spacing = d.spacing,
			.grid_dim_uniform = grid_dim,
			.frame_counter = d.frame_counter,
			.sun_direction = atm_state.sun_direction,
			.sun_intensity = atm_state.sun_intensity,
			.sun_color = atm_state.sun_color,
			.trace_t_max = d.trace_t_max,
			.sky_color = vec3f{ 0.5f, 0.7f, 1.0f },
			.intensity = d.intensity,
		},
		.stages = gpu::stage_flag::compute,
	};

	auto rec = co_await gpu::pass<system>(ctx)
		.pipeline(d.update_pipeline)
		.after<rt_shadow::system>();

	rec.bind_descriptors(d.update_pipeline, d.update_descriptors[frame_index]);
	rec.push(d.update_pipeline, pc);
	rec.dispatch(grid_dim.x(), grid_dim.y(), grid_dim.z());

	++d.frame_counter;
}

module gse.graphics;

import std;

import :atmosphere_renderer;
import :camera_system;
import :forward_renderer;
import :render_targets;

import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.meta;

namespace gse::renderer::atmosphere {
	using atmosphere_types = type_pack<atmosphere_data>;

	struct [[
		= shaders::binding<0, 0>{},
		= shaders::storage_image
	]] transmittance_out {
		using element = vec4f;
	};

	struct [[
		= shaders::binding<0, 0>{},
		= shaders::sampler2d
	]] transmittance_in {};

	struct [[
		= shaders::binding<0, 1>{},
		= shaders::storage_image
	]] multiscatter_out {
		using element = vec4f;
	};

	struct [[
		= shaders::binding<0, 1>{},
		= shaders::sampler2d
	]] multiscatter_in {};

	struct [[
		= shaders::binding<0, 2>{},
		= shaders::storage_image
	]] sky_view_out {
		using element = vec4f;
	};

	struct [[
		= shaders::binding<0, 1>{},
		= shaders::sampler2d
	]] sky_view_in {};

	struct [[
		= shaders::binding<0, 2>{},
		= shaders::storage_image_3d
	]] ap_volume_out {
		using element = vec4f;
	};

	struct [[= shaders::shader_struct]] sky_view_push_constants {
		vec3f sun_direction;
		atmosphere_length camera_altitude;
	};

	struct [[= shaders::shader_struct]] sky_raster_push_constants {
		mat4f inv_view_proj;
		vec3f sun_direction;
		atmosphere_length camera_altitude;
		vec3<irradiance> sun_irradiance;
		float sun_cos_radius;
	};

	struct [[= shaders::shader_struct]] ap_push_constants {
		mat4f inv_view_proj;
		vec3f sun_direction;
		atmosphere_length camera_altitude;
		vec3<irradiance> sun_irradiance;
		float _pad;
	};

	using transmittance_bindings = type_pack<atmosphere_ubo, transmittance_out>;
	using multiscatter_bindings = type_pack<atmosphere_ubo, transmittance_in, multiscatter_out>;
	using sky_view_bindings = type_pack<atmosphere_ubo, transmittance_in, multiscatter_in, sky_view_out>;
	using sky_raster_bindings = type_pack<atmosphere_ubo, transmittance_in, sky_view_in>;
	using ap_bindings = type_pack<atmosphere_ubo, transmittance_in, multiscatter_in, ap_volume_out>;

	using transmittance_entry = gpu::compute_entry<
		gpu::body_path<"Compute/atmosphere_transmittance">,
		gpu::layout<"atmosphere_transmittance">,
		gpu::types<atmosphere_types>,
		gpu::bindings<transmittance_bindings>,
		gpu::helpers<"Atmosphere/atmosphere_common">,
		gpu::threads<8, 8, 1>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using multiscatter_entry = gpu::compute_entry<
		gpu::body_path<"Compute/atmosphere_multiscatter">,
		gpu::layout<"atmosphere_multiscatter">,
		gpu::types<atmosphere_types>,
		gpu::bindings<multiscatter_bindings>,
		gpu::helpers<"Atmosphere/atmosphere_common">,
		gpu::threads<8, 8, 1>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using sky_view_entry = gpu::compute_entry<
		gpu::body_path<"Compute/atmosphere_sky_view">,
		gpu::layout<"atmosphere_sky_view">,
		gpu::types<atmosphere_types>,
		gpu::bindings<sky_view_bindings>,
		gpu::helpers<"Atmosphere/atmosphere_common">,
		gpu::push_constant<sky_view_push_constants>,
		gpu::threads<8, 8, 1>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using sky_raster_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/AtmosphereSky">,
		gpu::layout<"atmosphere_sky_raster">,
		gpu::types<atmosphere_types>,
		gpu::bindings<sky_raster_bindings>,
		gpu::helpers<"Atmosphere/atmosphere_common">,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<sky_raster_push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::color_target<gpu::color_format::hdr>,
		gpu::depth<true, false, gpu::compare_op::equal>
	>;

	using ap_entry = gpu::compute_entry<
		gpu::body_path<"Compute/atmosphere_aerial_perspective">,
		gpu::layout<"atmosphere_aerial_perspective">,
		gpu::types<atmosphere_types>,
		gpu::bindings<ap_bindings>,
		gpu::helpers<"Atmosphere/atmosphere_common">,
		gpu::push_constant<ap_push_constants>,
		gpu::threads<8, 8, 1>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	auto compute_ap_volume_extent(
		vec2u screen_extent
	) -> vec3u;

	auto recreate_ap_volume(
		const gpu::context::data& gpu_s,
		system::data& d
	) -> void;

	auto write_transmittance_descriptors(
		const gpu::context::data& gpu_s,
		system::data& d
	) -> void;

	auto write_multiscatter_descriptors(
		const gpu::context::data& gpu_s,
		system::data& d
	) -> void;

	auto write_sky_view_descriptors(
		const gpu::context::data& gpu_s,
		system::data& d
	) -> void;

	auto write_sky_raster_descriptors(
		const gpu::context::data& gpu_s,
		system::data& d
	) -> void;

	auto write_ap_descriptors(
		const gpu::context::data& gpu_s,
		system::data& d
	) -> void;

	auto compute_sun_direction(
		const system::data& d
	) -> vec3f;

	auto build_atmosphere_data(
		const system::data& d
	) -> atmosphere_data;
}

auto gse::renderer::atmosphere::compute_ap_volume_extent(const vec2u screen_extent) -> vec3u {
	constexpr std::uint32_t base = 32u;
	const auto w = std::max(screen_extent.x(), 1u);
	const auto h = std::max(screen_extent.y(), 1u);
	const auto x = base;
	const auto y = std::max(1u, (base * h + (w / 2u)) / w);
	return vec3u{ x, y, base };
}

auto gse::renderer::atmosphere::recreate_ap_volume(const gpu::context::data& gpu_s, system::data& d) -> void {
	d.ap_volume_extent = compute_ap_volume_extent(gpu_s.render_graph->extent());
	d.ap_volume = gpu::image::create(
		gpu_s.device->allocator(),
		{
			.size = vec2u{ d.ap_volume_extent.x(), d.ap_volume_extent.y() },
			.depth = d.ap_volume_extent.z(),
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.view = gpu::image_view_type::e3d,
			.usage = gpu::image_flag::storage | gpu::image_flag::sampled,
		},
		"atmosphere_ap_volume"
	);
	gpu::transition_image_to(*gpu_s.device, d.ap_volume);
}

auto gse::renderer::atmosphere::write_transmittance_descriptors(const gpu::context::data& gpu_s, system::data& d) -> void {
	gpu::descriptor_writer(gpu::context::device_handle(*gpu_s.device), d.transmittance_descriptors)
		.buffer<atmosphere_ubo>(d.atmosphere_ubo_buffer, 0, sizeof(atmosphere_data))
		.storage_image<transmittance_out>(d.transmittance_lut)
		.commit();
}

auto gse::renderer::atmosphere::write_multiscatter_descriptors(const gpu::context::data& gpu_s, system::data& d) -> void {
	gpu::descriptor_writer(gpu::context::device_handle(*gpu_s.device), d.multiscatter_descriptors)
		.buffer<atmosphere_ubo>(d.atmosphere_ubo_buffer, 0, sizeof(atmosphere_data))
		.combined_image_sampler<transmittance_in>(d.transmittance_lut, d.lut_sampler)
		.storage_image<multiscatter_out>(d.multiscatter_lut)
		.commit();
}

auto gse::renderer::atmosphere::write_sky_view_descriptors(const gpu::context::data& gpu_s, system::data& d) -> void {
	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		gpu::descriptor_writer(gpu::context::device_handle(*gpu_s.device), d.sky_view_descriptors[i])
			.buffer<atmosphere_ubo>(d.atmosphere_ubo_buffer, 0, sizeof(atmosphere_data))
			.combined_image_sampler<transmittance_in>(d.transmittance_lut, d.lut_sampler)
			.combined_image_sampler<multiscatter_in>(d.multiscatter_lut, d.lut_sampler)
			.storage_image<sky_view_out>(d.sky_view_lut)
			.commit();
	}
}

auto gse::renderer::atmosphere::write_sky_raster_descriptors(const gpu::context::data& gpu_s, system::data& d) -> void {
	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		gpu::descriptor_writer(gpu::context::device_handle(*gpu_s.device), d.sky_raster_descriptors[i])
			.buffer<atmosphere_ubo>(d.atmosphere_ubo_buffer, 0, sizeof(atmosphere_data))
			.combined_image_sampler<transmittance_in>(d.transmittance_lut, d.lut_sampler)
			.combined_image_sampler<sky_view_in>(d.sky_view_lut, d.sky_view_sampler)
			.commit();
	}
}

auto gse::renderer::atmosphere::write_ap_descriptors(const gpu::context::data& gpu_s, system::data& d) -> void {
	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		gpu::descriptor_writer(gpu::context::device_handle(*gpu_s.device), d.ap_descriptors[i])
			.buffer<atmosphere_ubo>(d.atmosphere_ubo_buffer, 0, sizeof(atmosphere_data))
			.combined_image_sampler<transmittance_in>(d.transmittance_lut, d.lut_sampler)
			.combined_image_sampler<multiscatter_in>(d.multiscatter_lut, d.lut_sampler)
			.storage_image<ap_volume_out>(d.ap_volume)
			.commit();
	}
}

auto gse::renderer::atmosphere::compute_sun_direction(const system::data& d) -> vec3f {
	const float ce = gse::cos(d.sun_elevation);
	const float se = gse::sin(d.sun_elevation);
	const float ca = gse::cos(d.sun_azimuth);
	const float sa = gse::sin(d.sun_azimuth);
	return normalize(vec3f{ ce * ca, se, ce * sa });
}

auto gse::renderer::atmosphere::build_atmosphere_data(const system::data& d) -> atmosphere_data {
	return atmosphere_data{
		.rayleigh_scattering = d.rayleigh_scattering,
		.bottom_radius = d.bottom_radius,
		.ozone_absorption = d.ozone_absorption,
		.top_radius = d.top_radius,
		.rayleigh_scale_height = d.rayleigh_scale_height,
		.mie_scattering = d.mie_scattering,
		.mie_absorption = d.mie_absorption,
		.mie_scale_height = d.mie_scale_height,
		.mie_phase_g = d.mie_phase_g,
		.ozone_peak_height = d.ozone_peak_height,
		.ozone_half_width = d.ozone_half_width,
		.max_view_distance = d.max_view_distance,
	};
}

auto gse::renderer::atmosphere::system::run(run_context& ctx, const gpu::context::data& gpu_s, data& d) -> async::task<> {
	d.transmittance_pipeline = gpu::build_compute_program(
		*gpu_s.device,
		*gpu_s.shader_registry,
		*gpu_s.bindless_textures,
		transmittance_entry::pod
	);
	d.multiscatter_pipeline = gpu::build_compute_program(
		*gpu_s.device,
		*gpu_s.shader_registry,
		*gpu_s.bindless_textures,
		multiscatter_entry::pod
	);
	d.sky_view_pipeline = gpu::build_compute_program(
		*gpu_s.device,
		*gpu_s.shader_registry,
		*gpu_s.bindless_textures,
		sky_view_entry::pod
	);
	d.sky_raster_pipeline = gpu::build_graphics_program(
		*gpu_s.device,
		*gpu_s.shader_registry,
		*gpu_s.bindless_textures,
		sky_raster_entry::pod
	);
	d.ap_pipeline =
		gpu::build_compute_program(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, ap_entry::pod);

	d.transmittance_lut = gpu::image::create(
		gpu_s.device->allocator(),
		{
			.size = transmittance_lut_size,
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.usage = gpu::image_flag::storage | gpu::image_flag::sampled,
		},
		"atmosphere_transmittance_lut"
	);
	d.multiscatter_lut = gpu::image::create(
		gpu_s.device->allocator(),
		{
			.size = multiscatter_lut_size,
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.usage = gpu::image_flag::storage | gpu::image_flag::sampled,
		},
		"atmosphere_multiscatter_lut"
	);
	d.sky_view_lut = gpu::image::create(
		gpu_s.device->allocator(),
		{
			.size = sky_view_lut_size,
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.usage = gpu::image_flag::storage | gpu::image_flag::sampled,
		},
		"atmosphere_sky_view_lut"
	);
	gpu::transition_image_to(*gpu_s.device, d.transmittance_lut);
	gpu::transition_image_to(*gpu_s.device, d.multiscatter_lut);
	gpu::transition_image_to(*gpu_s.device, d.sky_view_lut);

	recreate_ap_volume(gpu_s, d);

	d.atmosphere_ubo_buffer = gpu::buffer::create(
		gpu_s.device->allocator(),
		{
			.size = sizeof(atmosphere_data),
			.usage = gpu::buffer_flag::uniform,
		}
	);

	d.lut_sampler = gpu::sampler::create(
		gpu_s.device->allocator(),
		{
			.min = gpu::sampler_filter::linear,
			.mag = gpu::sampler_filter::linear,
			.address_u = gpu::sampler_address_mode::clamp_to_edge,
			.address_v = gpu::sampler_address_mode::clamp_to_edge,
			.address_w = gpu::sampler_address_mode::clamp_to_edge,
		}
	);
	d.sky_view_sampler = gpu::sampler::create(
		gpu_s.device->allocator(),
		{
			.min = gpu::sampler_filter::linear,
			.mag = gpu::sampler_filter::linear,
			.address_u = gpu::sampler_address_mode::repeat,
			.address_v = gpu::sampler_address_mode::clamp_to_edge,
			.address_w = gpu::sampler_address_mode::clamp_to_edge,
		}
	);

	d.transmittance_descriptors =
		gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), transmittance_entry::pod);
	d.multiscatter_descriptors =
		gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), multiscatter_entry::pod);
	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		d.sky_view_descriptors[i] =
			gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), sky_view_entry::pod);
		d.sky_raster_descriptors[i] =
			gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), sky_raster_entry::pod);
		d.ap_descriptors[i] =
			gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), ap_entry::pod);
	}

	write_transmittance_descriptors(gpu_s, d);
	write_multiscatter_descriptors(gpu_s, d);
	write_sky_view_descriptors(gpu_s, d);
	write_sky_raster_descriptors(gpu_s, d);
	write_ap_descriptors(gpu_s, d);

	gpu::context::on_swap_chain_recreate(gpu_s, [&gpu_s, &d]() {
		recreate_ap_volume(gpu_s, d);
		write_ap_descriptors(gpu_s, d);
	});

	co_return;
}

auto gse::renderer::atmosphere::system::frame(const frame_context& ctx, shared_view<gpu::context> gpu_s, data& d, shared_view<camera::system> cam_state) -> async::task<> {
	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();
	const auto ext = gpu_s.render_graph->extent();

	d.sun_direction = compute_sun_direction(d);
	const auto sun_irradiance = vec3<irradiance>{
		d.sun_intensity * d.sun_color.x(),
		d.sun_intensity * d.sun_color.y(),
		d.sun_intensity * d.sun_color.z(),
	};

	const atmosphere_data shader_payload = build_atmosphere_data(d);
	d.atmosphere_ubo_buffer.host_write(shader_payload);

	if (!d.luts_ready) {
		const auto transmittance_groups = vec2u{
			(transmittance_lut_size.x() + 7) / 8,
			(transmittance_lut_size.y() + 7) / 8,
		};
		const auto multiscatter_groups = vec2u{
			(multiscatter_lut_size.x() + 7) / 8,
			(multiscatter_lut_size.y() + 7) / 8,
		};

		auto rec = co_await gpu::pass<transmittance_pass>(ctx).pipeline(d.transmittance_pipeline);
		rec.bind_descriptors(d.transmittance_pipeline, d.transmittance_descriptors);
		rec.dispatch(transmittance_groups.x(), transmittance_groups.y(), 1);

		rec = co_await gpu::pass<multiscatter_pass>(ctx).pipeline(d.multiscatter_pipeline).after<transmittance_pass>();
		rec.bind_descriptors(d.multiscatter_pipeline, d.multiscatter_descriptors);
		rec.dispatch(multiscatter_groups.x(), multiscatter_groups.y(), 1);

		d.luts_ready = true;
	}

	const auto view = cam_state.view_matrix;
	const auto proj = cam_state.projection_matrix;
	const auto inv_view_proj = (proj * view).inverse();

	const auto sky_view_groups = vec2u{
		(sky_view_lut_size.x() + 7) / 8,
		(sky_view_lut_size.y() + 7) / 8,
	};
	const auto ap_groups = vec3u{
		(d.ap_volume_extent.x() + 7) / 8,
		(d.ap_volume_extent.y() + 7) / 8,
		d.ap_volume_extent.z(),
	};
	const float sun_cos_radius = gse::cos(d.sun_angular_radius);

	auto rec = co_await gpu::pass<sky_view_pass>(ctx).pipeline(d.sky_view_pipeline).after<multiscatter_pass>();
	rec.bind_descriptors(d.sky_view_pipeline, d.sky_view_descriptors[frame_index]);
	rec.push(
		d.sky_view_pipeline,
		gpu::typed_push_constants<sky_view_push_constants>{
			.data = {
				.sun_direction = d.sun_direction,
				.camera_altitude = d.camera_altitude,
			},
			.stages = gpu::stage_flag::compute,
		}
	);
	rec.dispatch(sky_view_groups.x(), sky_view_groups.y(), 1);

	rec = co_await gpu::pass<ap_compute_pass>(ctx).pipeline(d.ap_pipeline).after<multiscatter_pass>();
	rec.bind_descriptors(d.ap_pipeline, d.ap_descriptors[frame_index]);
	rec.push(
		d.ap_pipeline,
		gpu::typed_push_constants<ap_push_constants>{
			.data = {
				.inv_view_proj = inv_view_proj,
				.sun_direction = d.sun_direction,
				.camera_altitude = d.camera_altitude,
				.sun_irradiance = sun_irradiance,
				._pad = 0.0f,
			},
			.stages = gpu::stage_flag::compute,
		}
	);
	rec.dispatch(ap_groups.x(), ap_groups.y(), ap_groups.z());

	rec = co_await gpu::pass<sky_raster_pass>(ctx)
		.pipeline(d.sky_raster_pipeline)
		.color(gpu::load_color(gpu_s.render_graph->framebuffer_image<targets::hdr_color>()))
		.depth(gpu::load_depth())
		.after<forward::system, sky_view_pass>();
	rec.set_viewport(ext);
	rec.set_scissor(ext);
	rec.bind_descriptors(d.sky_raster_pipeline, d.sky_raster_descriptors[frame_index]);
	rec.push(
		d.sky_raster_pipeline,
		gpu::typed_push_constants<sky_raster_push_constants>{
			.data = {
				.inv_view_proj = inv_view_proj,
				.sun_direction = d.sun_direction,
				.camera_altitude = d.camera_altitude,
				.sun_irradiance = sun_irradiance,
				.sun_cos_radius = sun_cos_radius,
			},
			.stages = gpu::stage_flag::vertex | gpu::stage_flag::fragment,
		}
	);
	rec.draw(3);
}

module gse.graphics:atmosphere_renderer_impl;

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
		gpu::types<atmosphere_types>,
		gpu::bindings<transmittance_bindings>,
		gpu::helpers<"Atmosphere/atmosphere_common">,
		gpu::threads<8, 8, 1>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using multiscatter_entry = gpu::compute_entry<
		gpu::body_path<"Compute/atmosphere_multiscatter">,
		gpu::types<atmosphere_types>,
		gpu::bindings<multiscatter_bindings>,
		gpu::helpers<"Atmosphere/atmosphere_common">,
		gpu::threads<8, 8, 1>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using sky_view_entry = gpu::compute_entry<
		gpu::body_path<"Compute/atmosphere_sky_view">,
		gpu::types<atmosphere_types>,
		gpu::bindings<sky_view_bindings>,
		gpu::helpers<"Atmosphere/atmosphere_common">,
		gpu::push_constant<sky_view_push_constants>,
		gpu::threads<8, 8, 1>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using sky_raster_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/AtmosphereSky">,
		gpu::types<atmosphere_types>,
		gpu::bindings<sky_raster_bindings>,
		gpu::helpers<"Atmosphere/atmosphere_common">,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<sky_raster_push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::color_targets<gpu::color_format::hdr>,
		gpu::depth<true, false, gpu::compare_op::equal>
	>;

	using ap_entry = gpu::compute_entry<
		gpu::body_path<"Compute/atmosphere_aerial_perspective">,
		gpu::types<atmosphere_types>,
		gpu::bindings<ap_bindings>,
		gpu::helpers<"Atmosphere/atmosphere_common">,
		gpu::push_constant<ap_push_constants>,
		gpu::threads<8, 8, 1>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	constexpr gpu::sampler_desc lut_sampler_desc{
		.min = gpu::sampler_filter::linear,
		.mag = gpu::sampler_filter::linear,
		.address_u = gpu::sampler_address_mode::clamp_to_edge,
		.address_v = gpu::sampler_address_mode::clamp_to_edge,
		.address_w = gpu::sampler_address_mode::clamp_to_edge,
	};

	constexpr gpu::sampler_desc sky_view_sampler_desc{
		.min = gpu::sampler_filter::linear,
		.mag = gpu::sampler_filter::linear,
		.address_u = gpu::sampler_address_mode::repeat,
		.address_v = gpu::sampler_address_mode::clamp_to_edge,
		.address_w = gpu::sampler_address_mode::clamp_to_edge,
	};

	auto compute_ap_volume_extent(
		vec2u screen_extent
	) -> vec3u;

	auto recreate_ap_volume(
		shared_view<gpu::context> gpu_s,
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

auto gse::renderer::atmosphere::recreate_ap_volume(const shared_view<gpu::context> gpu_s, system::data& d) -> void {
	d.ap_volume_extent = compute_ap_volume_extent(gpu_s.render_graph->extent());
	d.ap_volume = gpu_s.device->create_image(
		{
			.size = vec2u{ d.ap_volume_extent.x(), d.ap_volume_extent.y() },
			.depth = d.ap_volume_extent.z(),
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.view = gpu::image_view_type::e3d,
			.usage = gpu::image_flag::storage | gpu::image_flag::sampled,
			.bindless = true,
		},
		"atmosphere_ap_volume"
	);
	gpu::transition_image_to(*gpu_s.device, d.ap_volume);
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

auto gse::renderer::atmosphere::system::init(context& ctx, const shared_view<gpu::context> gpu_s, data& d) -> async::task<> {
	d.transmittance_pipeline = gpu::build_compute_program(*gpu_s.device,
														  transmittance_entry::pod);
	d.multiscatter_pipeline = gpu::build_compute_program(*gpu_s.device, multiscatter_entry::pod);
	d.sky_view_pipeline = gpu::build_compute_program(*gpu_s.device, sky_view_entry::pod);
	d.sky_raster_pipeline = gpu::build_graphics_program(*gpu_s.device, sky_raster_entry::pod);
	d.ap_pipeline = gpu::build_compute_program(*gpu_s.device, ap_entry::pod);

	d.transmittance_lut = gpu_s.device->create_image(
		{
			.size = transmittance_lut_size,
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.usage = gpu::image_flag::storage | gpu::image_flag::sampled,
			.bindless = true,
		},
		"atmosphere_transmittance_lut"
	);
	d.multiscatter_lut = gpu_s.device->create_image(
		{
			.size = multiscatter_lut_size,
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.usage = gpu::image_flag::storage | gpu::image_flag::sampled,
			.bindless = true,
		},
		"atmosphere_multiscatter_lut"
	);
	d.sky_view_lut = gpu_s.device->create_image(
		{
			.size = sky_view_lut_size,
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.usage = gpu::image_flag::storage | gpu::image_flag::sampled,
			.bindless = true,
		},
		"atmosphere_sky_view_lut"
	);
	gpu::transition_image_to(*gpu_s.device, d.transmittance_lut);
	gpu::transition_image_to(*gpu_s.device, d.multiscatter_lut);
	gpu::transition_image_to(*gpu_s.device, d.sky_view_lut);

	recreate_ap_volume(
		gpu_s,
		d
	);

	d.atmosphere_ubo_buffer = gpu_s.device->create_buffer(
		{
			.size = sizeof(atmosphere_data),
			.usage = gpu::buffer_flag::uniform,
			.bindless = true,
		},
		"atmosphere_ubo"
	);

	d.lut_sampler = gpu_s.device->create_sampler(lut_sampler_desc);
	d.lut_sampler_bindless = gpu_s.device->register_sampler(lut_sampler_desc);
	d.sky_view_sampler_bindless = gpu_s.device->register_sampler(sky_view_sampler_desc);

	gpu::context::on_swap_chain_recreate(
		gpu_s,
		[gpu_s, &d]() -> void {
			recreate_ap_volume(
				gpu_s,
				d
			);
		}
	);

	return {};
}

auto gse::renderer::atmosphere::system::frame(const context& ctx, shared_view<gpu::context> gpu_s, data& d, shared_view<camera::system> cam_state) -> async::task<> {
	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

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
		rec.dispatch<transmittance_entry>(
			{
				.transmittance_out = d.transmittance_lut.storage_slot(),
				.atmosphere_ubo = d.atmosphere_ubo_buffer.slot(),
			},
			vec3u{ transmittance_groups.x(), transmittance_groups.y(), 1 }
		);

		rec = co_await gpu::pass<multiscatter_pass>(ctx).pipeline(d.multiscatter_pipeline).after<transmittance_pass>();
		rec.dispatch<multiscatter_entry>(
			{
				.transmittance_in = { d.transmittance_lut.sampled_slot(), d.lut_sampler_bindless.slot() },
				.multiscatter_out = d.multiscatter_lut.storage_slot(),
				.atmosphere_ubo = d.atmosphere_ubo_buffer.slot(),
			},
			vec3u{ multiscatter_groups.x(), multiscatter_groups.y(), 1 }
		);

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
	rec.dispatch<sky_view_entry>(
		{
			.sun_direction = d.sun_direction,
			.camera_altitude = d.camera_altitude,
		},
		{
			.transmittance_in = {
				.image = d.transmittance_lut.sampled_slot(),
				.sampler = d.lut_sampler_bindless.slot()
			},
			.multiscatter_in = {
				.image = d.multiscatter_lut.sampled_slot(),
				.sampler = d.lut_sampler_bindless.slot()
			},
			.sky_view_out = d.sky_view_lut.storage_slot(),
			.atmosphere_ubo = d.atmosphere_ubo_buffer.slot(),
		},
		vec3u{ sky_view_groups.x(), sky_view_groups.y(), 1 }
	);

	rec = co_await gpu::pass<ap_compute_pass>(ctx).pipeline(d.ap_pipeline).after<multiscatter_pass>();
	rec.dispatch<ap_entry>(
		{
			.inv_view_proj = inv_view_proj,
			.sun_direction = d.sun_direction,
			.camera_altitude = d.camera_altitude,
			.sun_irradiance = sun_irradiance,
			._pad = 0.0f,
		},
		{
			.transmittance_in = {
				.image = d.transmittance_lut.sampled_slot(),
				.sampler = d.lut_sampler_bindless.slot()
			},
			.multiscatter_in = {
				.image = d.multiscatter_lut.sampled_slot(),
				.sampler = d.lut_sampler_bindless.slot()
			},
			.ap_volume_out = d.ap_volume.storage_slot(),
			.atmosphere_ubo = d.atmosphere_ubo_buffer.slot(),
		},
		ap_groups
	);

	rec = co_await gpu::pass<sky_raster_pass>(ctx)
		.pipeline(d.sky_raster_pipeline)
		.color(gpu::load_color(gpu_s.render_graph->framebuffer_image<targets::hdr_color>()))
		.depth(gpu::load_depth())
		.after<forward::system, sky_view_pass>();
	rec.set_viewport(ext);
	rec.set_scissor(ext);
	rec.push_bindings<sky_raster_entry>(
		{
			.inv_view_proj = inv_view_proj,
			.sun_direction = d.sun_direction,
			.camera_altitude = d.camera_altitude,
			.sun_irradiance = sun_irradiance,
			.sun_cos_radius = sun_cos_radius,
		},
		{
			.transmittance_in = {
				.image = d.transmittance_lut.sampled_slot(),
				.sampler = d.lut_sampler_bindless.slot()
			},
			.sky_view_in = {
				.image = d.sky_view_lut.sampled_slot(),
				.sampler = d.sky_view_sampler_bindless.slot()
			},
			.atmosphere_ubo = d.atmosphere_ubo_buffer.slot(),
		}
	);
	rec.draw(3);
}

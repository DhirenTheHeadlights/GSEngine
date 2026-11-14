module gse.graphics:atmosphere_renderer_impl;

import std;

import :atmosphere_renderer;
import :camera_system;


import gse.gpu;
import gse.gpu_record;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.meta;

namespace gse::renderer::atmosphere {
	struct transmittance_bindings {
		shaders::storage_image<vec4f> transmittance_out;
	};

	struct multiscatter_bindings {
		shaders::texture2d<vec4f> transmittance_in;
		shaders::storage_image<vec4f> multiscatter_out;
		shaders::sampler_state lut_sampler;
	};

	struct sky_view_bindings {
		shaders::texture2d<vec4f> transmittance_in;
		shaders::texture2d<vec4f> multiscatter_in;
		shaders::storage_image<vec4f> sky_view_out;
		shaders::sampler_state lut_sampler;
	};

	struct ap_bindings {
		shaders::texture2d<vec4f> transmittance_in;
		shaders::texture2d<vec4f> multiscatter_in;
		shaders::storage_image_3d<vec4f> ap_volume_out;
		shaders::sampler_state lut_sampler;
	};

	struct [[= shaders::shader_struct]] sky_view_push_constants {
		vec3f sun_direction;
		atmosphere_length camera_altitude;
	};

	struct [[= shaders::shader_struct]] ap_push_constants {
		mat4f inv_view_proj;
		vec3f sun_direction;
		atmosphere_length camera_altitude;
	};

	using lut_pass = gpu::specs<
		gpu::helpers<"Atmosphere/atmosphere_common">,
		gpu::threads<8, 8, 1>
	>;

	using transmittance_entry = gpu::compute_entry<
		gpu::body_path<"Compute/atmosphere_transmittance">,
		gpu::bindings<transmittance_bindings, bindings>,
		lut_pass
	>;

	using multiscatter_entry = gpu::compute_entry<
		gpu::body_path<"Compute/atmosphere_multiscatter">,
		gpu::bindings<multiscatter_bindings, bindings>,
		lut_pass
	>;

	using sky_view_entry = gpu::compute_entry<
		gpu::body_path<"Compute/atmosphere_sky_view">,
		gpu::bindings<sky_view_bindings, bindings>,
		gpu::push_constant<sky_view_push_constants>,
		lut_pass
	>;

	using ap_entry = gpu::compute_entry<
		gpu::body_path<"Compute/atmosphere_aerial_perspective">,
		gpu::bindings<ap_bindings, bindings>,
		gpu::push_constant<ap_push_constants>,
		lut_pass
	>;

	constexpr gpu::sampler_desc sky_view_sampler_desc{
		.address_v = gpu::sampler_address_mode::clamp_to_edge,
		.address_w = gpu::sampler_address_mode::clamp_to_edge,
	};

	auto compute_ap_volume_extent(
		vec2u screen_extent
	) -> vec3u;

	auto recreate_ap_volume(
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> void;

	auto compute_sun_direction(
		angle elevation,
		angle azimuth
	) -> vec3f;

	auto compute_sun_transmittance(
		const data& d
	) -> vec3f;

	auto build_atmosphere_data(
		const data& d
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

auto gse::renderer::atmosphere::recreate_ap_volume(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
	d.ap_volume_extent = compute_ap_volume_extent(gpu_s.render_graph->extent());
	d.ap_volume = gpu_s.device->create_image(
		{
			.size = vec2u{ d.ap_volume_extent.x(), d.ap_volume_extent.y() },
			.depth = d.ap_volume_extent.z(),
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.view = gpu::image_view_type::e3d,
			.usage = { gpu::image_flag::storage, gpu::image_flag::sampled },
		},
		"atmosphere_ap_volume"
	);
	gpu::transition_image_to(*gpu_s.device, d.ap_volume);
}

auto gse::renderer::atmosphere::compute_sun_direction(const angle elevation, const angle azimuth) -> vec3f {
	const float ce = cos(elevation);
	const float se = sin(elevation);
	const float ca = cos(azimuth);
	const float sa = sin(azimuth);
	return normalize(vec3f{ ce * ca, se, ce * sa });
}

auto gse::renderer::atmosphere::compute_sun_transmittance(const data& d) -> vec3f {
	const auto radius = quantity_cast<atmosphere_length>(d.bottom_radius + d.camera_altitude);
	const float mu = d.sun_direction.y();

	const float bottom_ratio = d.bottom_radius / radius;
	if (mu < 0.f && mu * mu >= 1.f - bottom_ratio * bottom_ratio) {
		return vec3f{};
	}

	const float top_ratio = d.top_radius / radius;
	const auto t_max = radius * (std::sqrt(top_ratio * top_ratio - 1.f + mu * mu) - mu);

	constexpr int steps = 40;
	const auto dt = t_max / static_cast<float>(steps);
	const float dt_km = dt / kilometers(1.f);

	float rayleigh_depth_km = 0.f;
	float mie_depth_km = 0.f;
	float ozone_depth_km = 0.f;

	for (int i = 0; i < steps; ++i) {
		const auto t = dt * (static_cast<float>(i) + 0.5f);
		const float s = t / radius;
		const auto r_t = radius * std::sqrt(s * s + 2.f * s * mu + 1.f);
		const auto altitude = r_t > d.bottom_radius ? quantity_cast<atmosphere_length>(r_t - d.bottom_radius) : atmosphere_length{};

		rayleigh_depth_km += std::exp(-(altitude / d.rayleigh_scale_height)) * dt_km;
		mie_depth_km += std::exp(-(altitude / d.mie_scale_height)) * dt_km;
		const float ozone_offset = abs(quantity_cast<atmosphere_length>(altitude - d.ozone_peak_height)) / d.ozone_half_width;
		ozone_depth_km += std::clamp(1.f - ozone_offset, 0.f, 1.f) * dt_km;
	}

	const float mie_extinction_km = quantity_cast<atmosphere_inverse_length>(d.mie_scattering + d.mie_absorption) / per_kilometer(1.f);
	const auto tau = [&](const atmosphere_inverse_length rayleigh, const atmosphere_inverse_length ozone) -> float {
		return rayleigh / per_kilometer(1.f) * rayleigh_depth_km + mie_extinction_km * mie_depth_km + ozone / per_kilometer(1.f) * ozone_depth_km;
	};

	return vec3f{
		std::exp(-tau(d.rayleigh_scattering.x(), d.ozone_absorption.x())),
		std::exp(-tau(d.rayleigh_scattering.y(), d.ozone_absorption.y())),
		std::exp(-tau(d.rayleigh_scattering.z(), d.ozone_absorption.z())),
	};
}

auto gse::renderer::atmosphere::build_atmosphere_data(const data& d) -> atmosphere_data {
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
		.ground_albedo = d.ground_albedo,
		.sun_irradiance = {
			d.sun_intensity * d.sun_color.x(),
			d.sun_intensity * d.sun_color.y(),
			d.sun_intensity * d.sun_color.z(),
		},
	};
}

auto gse::renderer::atmosphere::init(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	{
		gpu::program_batch batch(*gpu_s.device);
		batch.add(d.transmittance_pipeline, transmittance_entry::pod);
		batch.add(d.multiscatter_pipeline, multiscatter_entry::pod);
		batch.add(d.sky_view_pipeline, sky_view_entry::pod);
		batch.add(d.ap_pipeline, ap_entry::pod);
	}

	d.transmittance_lut = gpu_s.device->create_image(
		{
			.size = transmittance_lut_size,
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.usage = { gpu::image_flag::storage, gpu::image_flag::sampled },
		},
		"atmosphere_transmittance_lut"
	);
	d.multiscatter_lut = gpu_s.device->create_image(
		{
			.size = multiscatter_lut_size,
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.usage = { gpu::image_flag::storage, gpu::image_flag::sampled },
		},
		"atmosphere_multiscatter_lut"
	);
	d.sky_view_lut = gpu_s.device->create_image(
		{
			.size = sky_view_lut_size,
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.usage = { gpu::image_flag::storage, gpu::image_flag::sampled },
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

	d.atmosphere_ubo_buffer = gpu_s.device->create_buffer<atmosphere_data>(
		1,
		{
			.usage = gpu::buffer_flag::storage,
		},
		"atmosphere_ubo"
	);

	d.lut_sampler_bindless = gpu_s.device->register_sampler(gpu::linear_clamp_sampler);
	d.sky_view_sampler_bindless = gpu_s.device->register_sampler(sky_view_sampler_desc);

	gpu_s.render_graph->on_resize(
		[gpu_s, &d]() -> void {
			recreate_ap_volume(
				gpu_s,
				d
			);
		}
	);

	return {};
}

auto gse::renderer::atmosphere::frame(const context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out, const channel_read<sun_request> sun_in, shared_view<camera::data> cam_state) -> async::task<> {
	angle sun_elevation = d.sun_elevation;
	angle sun_azimuth = d.sun_azimuth;
	for (const auto& [elevation, azimuth] : sun_in.of<sun_request>()) {
		sun_elevation = elevation;
		sun_azimuth = azimuth;
	}

	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	d.sun_direction = compute_sun_direction(sun_elevation, sun_azimuth);
	d.sun_transmittance = compute_sun_transmittance(d);

	const atmosphere_data shader_payload = build_atmosphere_data(d);
	d.atmosphere_ubo_buffer.host_write(shader_payload);

	if (!d.enabled || !cam_state.world_visible) {
		co_return;
	}

	if (!d.luts_ready) {
		auto rec = co_await gpu::pass<^^transmittance_pass>(pass_out).pipeline(d.transmittance_pipeline);
		rec.dispatch<transmittance_entry>(
			{
				.transmittance_out = d.transmittance_lut,
				.atmosphere_ubo = d.atmosphere_ubo_buffer,
			},
			gpu::group_count<transmittance_entry>(transmittance_lut_size)
		);

		rec = co_await gpu::pass<^^multiscatter_pass>(pass_out).pipeline(d.multiscatter_pipeline).after<^^transmittance_pass>();
		rec.dispatch<multiscatter_entry>(
			{
				.transmittance_in = d.transmittance_lut,
				.multiscatter_out = d.multiscatter_lut,
				.lut_sampler = d.lut_sampler_bindless,
				.atmosphere_ubo = d.atmosphere_ubo_buffer,
			},
			gpu::group_count<multiscatter_entry>(multiscatter_lut_size)
		);

		d.luts_ready = true;
	}

	const auto view = cam_state.view_matrix;
	const auto proj = cam_state.projection_matrix;
	const auto inv_view_proj = (proj * view).inverse();

	auto rec = co_await gpu::pass<^^sky_view_pass>(pass_out).pipeline(d.sky_view_pipeline).after<^^multiscatter_pass>();
	rec.dispatch<sky_view_entry>(
		{
			.sun_direction = d.sun_direction,
			.camera_altitude = d.camera_altitude,
		},
		{
			.transmittance_in = d.transmittance_lut,
			.multiscatter_in = d.multiscatter_lut,
			.sky_view_out = d.sky_view_lut,
			.lut_sampler = d.lut_sampler_bindless,
			.atmosphere_ubo = d.atmosphere_ubo_buffer,
		},
		gpu::group_count<sky_view_entry>(sky_view_lut_size)
	);

	rec = co_await gpu::pass<^^ap_compute_pass>(pass_out).pipeline(d.ap_pipeline).after<^^multiscatter_pass>();
	rec.dispatch<ap_entry>(
		{
			.inv_view_proj = inv_view_proj,
			.sun_direction = d.sun_direction,
			.camera_altitude = d.camera_altitude,
		},
		{
			.transmittance_in = d.transmittance_lut,
			.multiscatter_in = d.multiscatter_lut,
			.ap_volume_out = d.ap_volume,
			.lut_sampler = d.lut_sampler_bindless,
			.atmosphere_ubo = d.atmosphere_ubo_buffer,
		},
		gpu::group_count<ap_entry>(d.ap_volume_extent)
	);
}
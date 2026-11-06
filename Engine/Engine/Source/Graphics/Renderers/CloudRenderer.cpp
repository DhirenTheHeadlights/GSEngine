module gse.graphics:cloud_renderer_impl;

import std;

import :cloud_renderer;
import :atmosphere_renderer;
import :camera_system;
import :depth_prepass_renderer;
import :sdf_grid_renderer;
import :world_text_renderer;
import :render_targets;


import gse.gpu;
import gse.gpu_record;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.meta;

namespace gse::renderer::cloud {
	struct shape_bake_pass {};
	struct detail_bake_pass {};
	struct weather_bake_pass {};

	struct [[= shaders::shader_constant_block]] cloud_limits {
		int max_iterations = 128;
		int empty_steps_to_coarse = 6;
		int light_cone_samples = 5;
		int light_far_cone_samples = 2;
		int multi_scatter_octaves = 6;
		int shadow_march_steps = 16;

		atmosphere_length coarse_step = kilometers(0.4f);
		atmosphere_length fine_step = kilometers(0.1f);
		atmosphere_length step_growth_distance = kilometers(8.f);
		atmosphere_length ambient_depth = kilometers(0.15f);
		atmosphere_length powder_depth = kilometers(0.1f);
		atmosphere_length detail_fade_start = kilometers(15.f);
		atmosphere_length detail_fade_end = kilometers(40.f);
		atmosphere_length light_lod_distance = kilometers(20.f);
		angle shadow_min_sun_elevation = degrees(1.15f);

		float light_cone_spread = 0.15f;
		float light_long_sample_scale = 3.f;
		float light_march_min_transmittance = 0.1f;
		float view_march_min_transmittance = 0.02f;
		float thin_step_scale = 2.f;
		float thin_density = 0.25f;
		float scene_background_depth = 1e-5f;
	};

	struct [[= shaders::shader_struct]] cloud_view_data {
		mat4f inv_view_proj;
		mat4f prev_view_proj;
		vec2f history_subpixel_offset;
	};

	struct [[= shaders::uniform_block]] cloud_view_ubo {
		using element = cloud_view_data;
	};

	using cloud_types = type_pack<cloud_limits, cloud_data, atmosphere::atmosphere_data, cloud_view_data>;
	using shadow_types = type_pack<cloud_limits, cloud_data, cloud_shadow_data, atmosphere::atmosphere_data>;

	struct [[= shaders::storage_image_3d]] cloud_shape_out {
		using element = vec4f;
	};

	struct [[= shaders::storage_image_3d]] cloud_detail_out {
		using element = vec4f;
	};

	struct [[= shaders::texture2d]] transmittance_in {
		using element = vec4f;
	};

	struct [[= shaders::texture2d]] sky_view_in {
		using element = vec4f;
	};

	struct [[= shaders::texture3d]] cloud_shape_in {
		using element = vec4f;
	};

	struct [[= shaders::texture3d]] cloud_detail_in {
		using element = vec4f;
	};

	struct [[= shaders::storage_image]] cloud_out {
		using element = vec4f;
	};

	struct [[= shaders::sampler_state]] transmittance_sampler {};

	struct [[= shaders::sampler_state]] sky_view_sampler_binding {};

	struct [[= shaders::sampler_state]] noise_sampler_binding {};

	struct [[= shaders::storage_image]] cloud_shadow_out {
		using element = vec4f;
	};

	struct [[= shaders::storage_image_3d]] cloud_weather_out {
		using element = vec4f;
	};

	struct [[= shaders::texture3d]] cloud_weather_in {
		using element = vec4f;
	};

	struct [[= shaders::texture2d]] scene_depth_in {
		using element = vec4f;
	};

	struct [[= shaders::sampler_state]] scene_depth_sampler {};

	struct [[= shaders::texture2d]] cloud_in {
		using element = vec4f;
	};

	struct [[= shaders::texture2d]] cloud_history_in {
		using element = vec4f;
	};

	struct [[= shaders::sampler_state]] cloud_history_sampler {};

	struct [[= shaders::sampler_state]] cloud_composite_sampler {};

	struct [[= shaders::texture2d]] resolve_low_in {
		using element = vec4f;
	};

	struct [[= shaders::texture2d]] resolve_history_in {
		using element = vec4f;
	};

	struct [[= shaders::sampler_state]] resolve_sampler_binding {};

	struct [[= shaders::shader_struct]] cloud_push_constants {
		vec3f sun_direction;
		atmosphere_length camera_altitude;
		vec3<irradiance> sun_irradiance;
		float frame_index;
		vec2f subpixel_offset;
		vec2f full_extent;
		vec3<atmosphere_length> wind_offset;
		std::uint32_t target_divisor;
		vec2f depth_uv_scale;
		vec2f depth_uv_max;
		vec2u interleave_offset;
		vec2f low_uv_scale;
		vec2f low_uv_max;
		std::uint32_t interleave_stride;
	};

	struct [[= shaders::shader_struct]] cloud_resolve_push_constants {
		mat4f inv_view_proj;
		mat4f prev_view_proj;
		vec2u full_extent;
		vec2u block_offset;
		vec2u interleave_offset;
		std::int32_t divisor;
		std::int32_t history_valid;
		vec2f low_uv_scale;
		vec2f low_uv_max;
		vec2f history_uv_scale;
		vec2f history_uv_max;
		std::uint32_t interleave_stride;
	};

	struct [[= shaders::shader_struct]] cloud_shadow_push_constants {
		vec2u block_offset;
		std::uint32_t stride;
	};

	constexpr std::uint32_t shadow_interleave_divisor = 2;
	constexpr std::uint32_t raymarch_interleave_divisor = 2;

	struct [[= shaders::shader_struct]] cloud_composite_push_constants {
		vec2f screen_uv_scale;
		vec2f screen_uv_max;
	};

	using shape_bake_bindings = type_pack<cloud_shape_out>;
	using detail_bake_bindings = type_pack<cloud_detail_out>;
	using weather_bake_bindings = type_pack<cloud_weather_out>;
	using raymarch_bindings = type_pack<transmittance_in, sky_view_in, cloud_shape_in, cloud_detail_in, cloud_out, cloud_ubo, sky_view_sampler_binding, atmosphere::atmosphere_ubo, transmittance_sampler, noise_sampler_binding, cloud_weather_in, scene_depth_in, scene_depth_sampler, cloud_history_in, cloud_history_sampler, cloud_view_ubo>;
	using composite_bindings = type_pack<cloud_in, cloud_composite_sampler>;
	using resolve_bindings = type_pack<resolve_low_in, resolve_history_in, resolve_sampler_binding>;
	using shadow_bindings = type_pack<cloud_shape_in, cloud_ubo, atmosphere::atmosphere_ubo, noise_sampler_binding, cloud_shadow_out, cloud_shadow_ubo, cloud_weather_in>;

	using shape_bake_entry = gpu::compute_entry<gpu::body_path<"Compute/cloud_shape_bake">, gpu::bindings<shape_bake_bindings>, gpu::helpers<"Clouds/cloud_common">, gpu::threads<8, 8, 1>, gpu::system_values<gpu::dispatch_thread_id>>;

	using detail_bake_entry = gpu::compute_entry<gpu::body_path<"Compute/cloud_detail_bake">, gpu::bindings<detail_bake_bindings>, gpu::helpers<"Clouds/cloud_common">, gpu::threads<8, 8, 1>, gpu::system_values<gpu::dispatch_thread_id>>;

	using weather_bake_entry = gpu::compute_entry<gpu::body_path<"Compute/cloud_weather_bake">, gpu::bindings<weather_bake_bindings>, gpu::helpers<"Clouds/cloud_common">, gpu::threads<8, 8, 1>, gpu::system_values<gpu::dispatch_thread_id>>;

	using raymarch_entry = gpu::compute_entry<
		gpu::body_path<"Compute/cloud_raymarch">,
		gpu::types<cloud_types>,
		gpu::bindings<raymarch_bindings>,
		gpu::helpers<"Atmosphere/atmosphere_common", "Clouds/cloud_common", "Clouds/cloud_density", "Screen/screen_target">,
		gpu::push_constant<cloud_push_constants>,
		gpu::threads<8, 8, 1>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using resolve_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/CloudResolve">,
		gpu::bindings<resolve_bindings>,
		gpu::helpers<"Screen/screen_pass", "Screen/screen_target">,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<cloud_resolve_push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::depth<false, false>,
		gpu::depth_target<gpu::depth_format::none>,
		gpu::color_targets<gpu::color_format::hdr>
	>;

	using shadow_entry = gpu::compute_entry<
		gpu::body_path<"Compute/cloud_shadow_map">,
		gpu::types<shadow_types>,
		gpu::bindings<shadow_bindings>,
		gpu::helpers<"Clouds/cloud_common", "Clouds/cloud_density">,
		gpu::push_constant<cloud_shadow_push_constants>,
		gpu::threads<8, 8, 1>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using composite_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/CloudComposite">,
		gpu::bindings<composite_bindings>,
		gpu::helpers<"Screen/screen_pass", "Screen/screen_target">,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<cloud_composite_push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::color_targets<gpu::color_format::hdr>,
		gpu::blend<gpu::blend_preset::alpha>,
		gpu::depth<true, false, gpu::compare_op::greater_or_equal>
	>;

	constexpr gpu::sampler_desc noise_sampler_desc{
		.min = gpu::sampler_filter::linear,
		.mag = gpu::sampler_filter::linear,
		.address_u = gpu::sampler_address_mode::repeat,
		.address_v = gpu::sampler_address_mode::repeat,
		.address_w = gpu::sampler_address_mode::repeat,
	};

	constexpr gpu::sampler_desc atmosphere_lut_sampler_desc{
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

	constexpr gpu::sampler_desc composite_sampler_desc{
		.min = gpu::sampler_filter::linear,
		.mag = gpu::sampler_filter::linear,
		.address_u = gpu::sampler_address_mode::clamp_to_edge,
		.address_v = gpu::sampler_address_mode::clamp_to_edge,
		.address_w = gpu::sampler_address_mode::clamp_to_edge,
	};

	constexpr gpu::sampler_desc depth_sampler_desc{
		.min = gpu::sampler_filter::nearest,
		.mag = gpu::sampler_filter::nearest,
		.address_u = gpu::sampler_address_mode::clamp_to_edge,
		.address_v = gpu::sampler_address_mode::clamp_to_edge,
		.address_w = gpu::sampler_address_mode::clamp_to_edge,
		.border = gpu::border_color::float_opaque_white,
		.max_lod = 1.0f,
	};

	auto update_depth_descriptor(
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> void;

	auto compute_cloud_target_extent(
		vec2u screen_extent,
		int divisor
	) -> vec2u;

	auto effective_resolve_divisor(
		const data& d
	) -> int;

	auto recreate_cloud_target(
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> void;

	auto recreate_shadow_map(
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> void;

	auto build_cloud_data(
		const data& d
	) -> cloud_data;

	auto build_cloud_shadow_data(
		const data& d,
		const vec3f& sun_direction,
		atmosphere_length camera_altitude,
		bool active
	) -> cloud_shadow_data;
}

auto gse::renderer::cloud::update_depth_descriptor(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
	if (!d.depth_view.valid()) {
		d.depth_view = gpu_s.device->allocate_image_slot();
	}
	gpu_s.device->write_sampled_image(d.depth_view.slot(), gpu_s.render_graph->depth_image());
}

auto gse::renderer::cloud::compute_cloud_target_extent(const vec2u screen_extent, const int divisor) -> vec2u {
	const auto d = static_cast<std::uint32_t>(std::max(divisor, 1));
	const auto w = std::max(screen_extent.x() / d, 1u);
	const auto h = std::max(screen_extent.y() / d, 1u);
	return vec2u{ w, h };
}

auto gse::renderer::cloud::effective_resolve_divisor(const data& d) -> int {
	return std::gcd(std::max(d.target_divisor, 1), std::max(d.resolve_divisor, 1));
}

auto gse::renderer::cloud::recreate_cloud_target(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
	const auto previous_target_allocated = d.cloud_target_allocated_extent;
	const auto previous_resolve_allocated = d.cloud_resolve_allocated_extent;
	const auto previous_resolve_active = d.cloud_resolve_extent;
	d.applied_target_divisor = std::max(d.target_divisor, 1);
	d.applied_resolve_divisor = effective_resolve_divisor(d);
	d.cloud_target_extent = compute_cloud_target_extent(gpu_s.render_graph->extent(), d.applied_target_divisor);
	d.cloud_target_allocated_extent = compute_cloud_target_extent(gpu_s.render_graph->allocated_extent(), d.applied_target_divisor);
	d.cloud_resolve_extent = compute_cloud_target_extent(gpu_s.render_graph->extent(), d.applied_resolve_divisor);
	d.cloud_resolve_allocated_extent = compute_cloud_target_extent(gpu_s.render_graph->allocated_extent(), d.applied_resolve_divisor);

	if (d.cloud_resolve_extent != previous_resolve_active) {
		d.frames_since_history_invalid = 0;
	}

	if (d.cloud_target[0].handle() && d.cloud_target_allocated_extent == previous_target_allocated && d.cloud_resolve_allocated_extent == previous_resolve_allocated) {
		update_depth_descriptor(gpu_s, d);
		return;
	}

	d.frames_since_history_invalid = 0;
	for (std::size_t i = 0; i < d.cloud_resolve.frames_in_flight; ++i) {
		d.cloud_target[i] = gpu_s.device->create_image(
			{
				.size = d.cloud_target_allocated_extent,
				.format = gpu::image_format::r16g16b16a16_sfloat,
				.usage = { gpu::image_flag::storage, gpu::image_flag::sampled },
			},
			std::format("cloud_target_{}", i)
		);
		gpu::transition_image_to(*gpu_s.device, d.cloud_target[i]);

		d.cloud_resolve[i] = gpu_s.device->create_image(
			{
				.size = d.cloud_resolve_allocated_extent,
				.format = gpu::image_format::r16g16b16a16_sfloat,
				.usage = { gpu::image_flag::color_attachment, gpu::image_flag::sampled },
			},
			std::format("cloud_resolve_{}", i)
		);
		gpu::transition_image_to(*gpu_s.device, d.cloud_resolve[i]);
	}

	update_depth_descriptor(gpu_s, d);
}

auto gse::renderer::cloud::recreate_shadow_map(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
	d.applied_shadow_resolution = std::clamp(d.shadow_map_resolution, 128, 2048);
	const auto edge = static_cast<std::uint32_t>(d.applied_shadow_resolution);
	d.shadow_map = gpu_s.device->create_image(
		{
			.size = vec2u{ edge, edge },
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.usage = { gpu::image_flag::storage, gpu::image_flag::sampled },
		},
		"cloud_shadow_map"
	);
	gpu::transition_image_to(*gpu_s.device, d.shadow_map);
	d.shadow_phases_pending = shadow_interleave_divisor * shadow_interleave_divisor;
	d.shadow_map_filled = false;
}

auto gse::renderer::cloud::build_cloud_shadow_data(const data& d, const vec3f& sun_direction, const atmosphere_length camera_altitude, const bool active) -> cloud_shadow_data {
	const auto half_extent = d.shadow_extent * 0.5f;

	return cloud_shadow_data{
		.sun_direction = sun_direction,
		.strength = active ? std::clamp(d.shadow_strength, 0.f, 1.f) : 0.f,
		.wind_offset = d.wind_offset,
		.extent_km = d.shadow_extent,
		.origin_km = vec2<atmosphere_length>(-half_extent, -half_extent),
		.plane_altitude = camera_altitude,
	};
}

auto gse::renderer::cloud::build_cloud_data(const data& d) -> cloud_data {
	return cloud_data{
		.cloud_bottom = d.cloud_bottom,
		.cloud_top = d.cloud_top,
		.cloud_coverage = d.cloud_coverage,
		.cloud_type = d.cloud_type,
		.extinction = d.extinction,
		.albedo = d.albedo,
		.multi_scatter_strength = d.multi_scatter_strength,
		.powder_strength = d.powder_strength,
		.shape_scale = d.shape_scale,
		.detail_scale = d.detail_scale,
		.detail_erosion = d.detail_erosion,
		.phase_g_forward = d.phase_g_forward,
		.phase_g_back = d.phase_g_back,
		.phase_blend = d.phase_blend,
		.ambient_strength = d.ambient_strength,
		.max_distance = d.max_distance,
		.weather_scale = d.weather_scale,
		.weather_phase = d.weather_phase,
		.weather_contrast = d.weather_contrast,
		.weather_type_influence = d.weather_type_influence,
		.multi_scatter_extinction = d.multi_scatter_extinction,
	};
}

auto gse::renderer::cloud::init(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	d.shape_bake_pipeline = gpu::build_compute_program(*gpu_s.device, shape_bake_entry::pod);
	d.detail_bake_pipeline = gpu::build_compute_program(*gpu_s.device, detail_bake_entry::pod);
	d.raymarch_pipeline = gpu::build_compute_program(*gpu_s.device, raymarch_entry::pod);
	d.resolve_pipeline = gpu::build_graphics_program(*gpu_s.device, resolve_entry::pod);
	d.composite_pipeline = gpu::build_graphics_program(*gpu_s.device, composite_entry::pod);
	d.shadow_pipeline = gpu::build_compute_program(*gpu_s.device, shadow_entry::pod);
	d.weather_bake_pipeline = gpu::build_compute_program(*gpu_s.device, weather_bake_entry::pod);

	d.shape_noise = gpu_s.device->create_image(
		{
			.size = vec2u{ shape_noise_size.x(), shape_noise_size.y() },
			.depth = shape_noise_size.z(),
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.view = gpu::image_view_type::e3d,
			.usage = { gpu::image_flag::storage, gpu::image_flag::sampled },
		},
		"cloud_shape_noise"
	);
	d.detail_noise = gpu_s.device->create_image(
		{
			.size = vec2u{ detail_noise_size.x(), detail_noise_size.y() },
			.depth = detail_noise_size.z(),
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.view = gpu::image_view_type::e3d,
			.usage = { gpu::image_flag::storage, gpu::image_flag::sampled },
		},
		"cloud_detail_noise"
	);
	d.weather_map = gpu_s.device->create_image(
		{
			.size = vec2u{ weather_map_size.x(), weather_map_size.y() },
			.depth = weather_map_size.z(),
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.view = gpu::image_view_type::e3d,
			.usage = { gpu::image_flag::storage, gpu::image_flag::sampled },
		},
		"cloud_weather_map"
	);

	gpu::transition_image_to(*gpu_s.device, d.shape_noise);
	gpu::transition_image_to(*gpu_s.device, d.detail_noise);
	gpu::transition_image_to(*gpu_s.device, d.weather_map);

	recreate_cloud_target(gpu_s, d);
	recreate_shadow_map(gpu_s, d);

	d.cloud_ubo_buffer = gpu_s.device->create_buffer(
		{
			.size = sizeof(cloud_data),
			.stride = sizeof(cloud_data),
			.usage = gpu::buffer_flag::storage,
		},
		"cloud_ubo"
	);

	d.shadow_ubo_buffer = gpu_s.device->create_buffer(
		{
			.size = sizeof(cloud_shadow_data),
			.stride = sizeof(cloud_shadow_data),
			.usage = gpu::buffer_flag::storage,
		},
		"cloud_shadow_ubo"
	);

	for (std::size_t i = 0; i < d.view_ubo_buffer.frames_in_flight; ++i) {
		d.view_ubo_buffer[i] = gpu_s.device->create_buffer(
			{
				.size = sizeof(cloud_view_data),
				.stride = sizeof(cloud_view_data),
				.usage = gpu::buffer_flag::storage,
			},
			std::format("cloud_view_ubo_{}", i)
		);
	}

	d.shadow_sampler = gpu_s.device->register_sampler(atmosphere_lut_sampler_desc);
	d.noise_sampler = gpu_s.device->register_sampler(noise_sampler_desc);
	d.atmosphere_lut_sampler = gpu_s.device->register_sampler(atmosphere_lut_sampler_desc);
	d.sky_view_sampler = gpu_s.device->register_sampler(sky_view_sampler_desc);
	d.composite_sampler = gpu_s.device->register_sampler(composite_sampler_desc);
	d.resolve_sampler = gpu_s.device->register_sampler(composite_sampler_desc);
	d.depth_sampler = gpu_s.device->register_sampler(depth_sampler_desc);

	gpu_s.render_graph->on_resize(
		[gpu_s, &d]() {
			recreate_cloud_target(gpu_s, d);
		}
	);

	return {};
}

auto gse::renderer::cloud::frame(const context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out, const channel_read<weather_request> weather_in, shared_view<atmosphere::data> atm_state, shared_view<camera::data> cam_state, const channel_read<camera::temporal_reset_request> reset_in) -> async::task<> {
	if (!reset_in.of<camera::temporal_reset_request>().empty()) {
		d.frame_counter = 0;
		d.frames_since_history_invalid = 0;
	}

	for (const auto& [phase] : weather_in.of<weather_request>()) {
		d.weather_phase = phase;
	}

	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	if (std::max(d.target_divisor, 1) != d.applied_target_divisor || effective_resolve_divisor(d) != d.applied_resolve_divisor) {
		gpu_s.device->wait_idle();
		recreate_cloud_target(gpu_s, d);
	}

	if (std::clamp(d.shadow_map_resolution, 128, 2048) != d.applied_shadow_resolution) {
		gpu_s.device->wait_idle();
		recreate_shadow_map(gpu_s, d);
	}

	const bool shadows_active = d.enabled
		&& cam_state.world_visible
		&& d.shadow_strength > 0.f
		&& d.shadow_map.valid()
		&& d.shadow_ubo_buffer.valid()
		&& atm_state.atmosphere_ubo_buffer.valid();

	const cloud_shadow_data shadow_payload = build_cloud_shadow_data(d, atm_state.sun_direction, atm_state.camera_altitude, shadows_active);

	if (d.shadow_ubo_buffer.valid()) {
		d.shadow_ubo_buffer.host_write(shadow_payload);
	}

	if (!d.cloud_target[0].valid()) {
		co_return;
	}

	if (!atm_state.atmosphere_ubo_buffer.valid()) {
		co_return;
	}

	if (!d.enabled || !cam_state.world_visible) {
		co_return;
	}

	const cloud_data shader_payload = build_cloud_data(d);
	d.cloud_ubo_buffer.host_write(shader_payload);

	if (!d.noises_ready) {
		const auto shape_groups = vec3u{
			(shape_noise_size.x() + 7) / 8,
			(shape_noise_size.y() + 7) / 8,
			shape_noise_size.z(),
		};
		const auto detail_groups = vec3u{
			(detail_noise_size.x() + 7) / 8,
			(detail_noise_size.y() + 7) / 8,
			detail_noise_size.z(),
		};

		auto rec = co_await gpu::pass<^^shape_bake_pass>(pass_out).pipeline(d.shape_bake_pipeline);
		rec.dispatch<shape_bake_entry>(
			{
				.cloud_shape_out = d.shape_noise.storage_slot(),
			},
			shape_groups
		);

		rec = co_await gpu::pass<^^detail_bake_pass>(pass_out).pipeline(d.detail_bake_pipeline).after<^^shape_bake_pass>();
		rec.dispatch<detail_bake_entry>(
			{
				.cloud_detail_out = d.detail_noise.storage_slot(),
			},
			detail_groups
		);

		const auto weather_groups = vec3u{
			(weather_map_size.x() + 7) / 8,
			(weather_map_size.y() + 7) / 8,
			weather_map_size.z(),
		};

		rec = co_await gpu::pass<^^weather_bake_pass>(pass_out).pipeline(d.weather_bake_pipeline).after<^^detail_bake_pass>();
		rec.dispatch<weather_bake_entry>(
			{
				.cloud_weather_out = d.weather_map.storage_slot(),
			},
			weather_groups
		);

		d.noises_ready = true;
	}

	constexpr auto shadow_cycle = shadow_interleave_divisor * shadow_interleave_divisor;

	if (d.shadow_plane_inputs != shadow_payload || d.shadow_cloud_inputs != shader_payload) {
		d.shadow_plane_inputs = shadow_payload;
		d.shadow_cloud_inputs = shader_payload;
		d.shadow_phases_pending = shadow_cycle;
	}

	if (shadows_active && d.shadow_phases_pending > 0) {
		const auto stride = d.shadow_map_filled ? shadow_interleave_divisor : 1u;
		const auto shadow_slot = (d.shadow_bake_counter * 7u) % shadow_cycle;

		if (d.shadow_map_filled) {
			--d.shadow_phases_pending;
			++d.shadow_bake_counter;
		}
		else {
			d.shadow_phases_pending = 0;
			d.shadow_map_filled = true;
		}

		const auto edge = static_cast<std::uint32_t>(d.applied_shadow_resolution);
		const auto shadow_groups = ((edge + stride - 1) / stride + 7) / 8;

		auto shadow_rec = co_await gpu::pass<^^cloud_shadow_pass>(pass_out)
			.pipeline(d.shadow_pipeline)
			.after<^^weather_bake_pass>();

		shadow_rec.dispatch<shadow_entry>(
			{
				.block_offset = stride == 1u ? vec2u{ 0, 0 } : vec2u{ shadow_slot % stride, shadow_slot / stride },
				.stride = stride,
			},
			{
				.cloud_shape_in = d.shape_noise.sampled_slot(),
				.cloud_ubo = d.cloud_ubo_buffer.slot(),
				.atmosphere_ubo = atm_state.atmosphere_ubo_buffer.slot(),
				.noise_sampler_binding = d.noise_sampler.slot(),
				.cloud_shadow_out = d.shadow_map.storage_slot(),
				.cloud_shadow_ubo = d.shadow_ubo_buffer.slot(),
				.cloud_weather_in = d.weather_map.sampled_slot(),
			},
			vec3u{ shadow_groups, shadow_groups, 1 }
		);
	}

	const auto view = cam_state.view_matrix;
	const auto proj = cam_state.projection_matrix;
	const auto inv_view_proj = (proj * view).inverse();

	const auto sun_irradiance = vec3<irradiance>{
		atm_state.sun_intensity * atm_state.sun_color.x(),
		atm_state.sun_intensity * atm_state.sun_color.y(),
		atm_state.sun_intensity * atm_state.sun_color.z(),
	};

	d.frame_counter += 1;

	const auto history_index = gpu_s.render_graph->current_frame();
	const auto previous_index = 1u - history_index;
	const bool history_ready = d.frames_since_history_invalid >= 2;
	++d.frames_since_history_invalid;

	constexpr auto interleave_cycle = raymarch_interleave_divisor * raymarch_interleave_divisor;
	const auto interleave_stride = history_ready ? raymarch_interleave_divisor : 1u;
	const auto interleave_slot = (d.frame_counter * 7u) % interleave_cycle;
	const auto interleave_offset = vec2u{ interleave_slot % interleave_stride, interleave_slot / interleave_stride % interleave_stride };
	const auto raymarch_groups = vec3u{
		((d.cloud_target_extent.x() + interleave_stride - 1) / interleave_stride + 7) / 8,
		((d.cloud_target_extent.y() + interleave_stride - 1) / interleave_stride + 7) / 8,
		interleave_stride * interleave_stride,
	};

	const auto divisor = static_cast<std::uint32_t>(d.applied_target_divisor);
	const auto jitter = divisor / static_cast<std::uint32_t>(d.applied_resolve_divisor);
	const auto cycle = jitter * jitter;
	const auto slot = (d.frame_counter / interleave_cycle * 7u) % cycle;
	const auto block_offset = vec2u{ slot % jitter, slot / jitter };
	const auto resolve_extent = d.cloud_resolve_extent;
	const auto screen_extent = gpu_s.render_graph->extent();
	const auto low_uv_scale = gpu::screen_uv_scale_for(d.cloud_target_extent, d.cloud_target_allocated_extent);
	const auto low_uv_max = gpu::screen_uv_max_for(d.cloud_target_extent, d.cloud_target_allocated_extent);

	const auto subpixel_offset = vec2f{
		(static_cast<float>(block_offset.x()) + 0.5f) / static_cast<float>(jitter),
		(static_cast<float>(block_offset.y()) + 0.5f) / static_cast<float>(jitter),
	};

	d.view_ubo_buffer[history_index].host_write(cloud_view_data{
		.inv_view_proj = inv_view_proj,
		.prev_view_proj = history_ready ? d.prev_view_proj : proj * view,
		.history_subpixel_offset = history_ready ? d.prev_subpixel_offset : subpixel_offset,
	});
	d.prev_subpixel_offset = subpixel_offset;

	auto rec = co_await gpu::pass<^^cloud_raymarch_pass>(pass_out)
		.pipeline(d.raymarch_pipeline)
		.after<^^atmosphere::sky_view_pass, ^^weather_bake_pass, ^^depth_prepass::frame>();

	rec.dispatch<raymarch_entry>(
		{
			.sun_direction = atm_state.sun_direction,
			.camera_altitude = atm_state.camera_altitude,
			.sun_irradiance = sun_irradiance,
			.frame_index = static_cast<float>(d.frame_counter),
			.subpixel_offset = subpixel_offset,
			.full_extent = vec2f{
				static_cast<float>(screen_extent.x()),
				static_cast<float>(screen_extent.y()),
			},
			.wind_offset = d.wind_offset,
			.target_divisor = divisor,
			.depth_uv_scale = gpu_s.render_graph->screen_uv_scale(),
			.depth_uv_max = gpu_s.render_graph->screen_uv_max(),
			.interleave_offset = interleave_offset,
			.low_uv_scale = low_uv_scale,
			.low_uv_max = low_uv_max,
			.interleave_stride = interleave_stride,
		},
		{
			.transmittance_in = atm_state.transmittance_lut.sampled_slot(),
			.sky_view_in = atm_state.sky_view_lut.sampled_slot(),
			.cloud_shape_in = d.shape_noise.sampled_slot(),
			.cloud_detail_in = d.detail_noise.sampled_slot(),
			.cloud_out = d.cloud_target[history_index].storage_slot(),
			.cloud_ubo = d.cloud_ubo_buffer.slot(),
			.sky_view_sampler_binding = d.sky_view_sampler.slot(),
			.atmosphere_ubo = atm_state.atmosphere_ubo_buffer.slot(),
			.transmittance_sampler = d.atmosphere_lut_sampler.slot(),
			.noise_sampler_binding = d.noise_sampler.slot(),
			.cloud_weather_in = d.weather_map.sampled_slot(),
			.scene_depth_in = d.depth_view.slot(),
			.scene_depth_sampler = d.depth_sampler.slot(),
			.cloud_history_in = d.cloud_target[previous_index].sampled_slot(),
			.cloud_history_sampler = d.resolve_sampler.slot(),
			.cloud_view_ubo = d.view_ubo_buffer[history_index].slot(),
		},
		raymarch_groups
	);

	auto resolve_rec = co_await gpu::pass<^^cloud_resolve_pass>(pass_out)
		.pipeline(d.resolve_pipeline)
		.color(gpu::clear_color(
			gpu::color_clear{ 0.0f, 0.0f, 0.0f, 1.0f },
			d.cloud_resolve[history_index]
		))
		.after<^^cloud_raymarch_pass>();

	resolve_rec.set_viewport(resolve_extent);
	resolve_rec.set_scissor(resolve_extent);
	resolve_rec.push_bindings<resolve_entry>(
		{
			.inv_view_proj = inv_view_proj,
			.prev_view_proj = history_ready ? d.prev_view_proj : proj * view,
			.full_extent = resolve_extent,
			.block_offset = block_offset,
			.interleave_offset = interleave_offset,
			.divisor = static_cast<std::int32_t>(jitter),
			.history_valid = history_ready ? 1 : 0,
			.low_uv_scale = low_uv_scale,
			.low_uv_max = low_uv_max,
			.history_uv_scale = gpu::screen_uv_scale_for(d.cloud_resolve_extent, d.cloud_resolve_allocated_extent),
			.history_uv_max = gpu::screen_uv_max_for(d.cloud_resolve_extent, d.cloud_resolve_allocated_extent),
			.interleave_stride = interleave_stride,
		},
		{
			.resolve_low_in = d.cloud_target[history_index].sampled_slot(),
			.resolve_history_in = d.cloud_resolve[previous_index].sampled_slot(),
			.resolve_sampler_binding = d.resolve_sampler.slot(),
		}
	);
	resolve_rec.draw(3);

	d.prev_view_proj = proj * view;

	auto composite_rec = co_await gpu::pass<^^cloud_composite_pass>(pass_out)
		.pipeline(d.composite_pipeline)
		.color(gpu::load_color(gpu_s.render_graph->framebuffer_image<targets::hdr_color>()))
		.depth(gpu::load_depth())
		.after<^^atmosphere::sky_raster_pass, ^^cloud_resolve_pass, ^^sdf_grid::frame, ^^world_text::frame>();

	composite_rec.set_viewport(screen_extent);
	composite_rec.set_scissor(screen_extent);
	composite_rec.push_bindings<composite_entry>(
		{
			.screen_uv_scale = gpu::screen_uv_scale_for(d.cloud_resolve_extent, d.cloud_resolve_allocated_extent),
			.screen_uv_max = gpu::screen_uv_max_for(d.cloud_resolve_extent, d.cloud_resolve_allocated_extent),
		},
		{
			.cloud_in = d.cloud_resolve[history_index].sampled_slot(),
			.cloud_composite_sampler = d.composite_sampler.slot(),
		}
	);
	composite_rec.draw(3);
}

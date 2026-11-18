module gse.graphics:cloud_renderer_impl;

import std;

import :cloud_renderer;
import :atmosphere_renderer;
import :camera_latch_renderer;
import :camera_system;
import :depth_prepass_renderer;
import :shared_shaders;


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
	struct cloud_delta_pass {};

	constexpr gpu::image_format shape_noise_format = gpu::image_format::r16g16_sfloat;

	static_assert(
		std::is_lvalue_reference_v<decltype(shared_view<data>::layer)>,
		"cloud::data::layer must be published by reference: the sky draw reads `rendered` and the images as cloud::frame left them this frame, which a value snapshot taken after update would not show"
	);

	struct [[= shaders::shader_constant_block]] cloud_limits {
		int max_iterations = 128;
		int light_cone_samples = 5;
		int multi_scatter_octaves = 6;
		int shadow_march_steps = 16;
		int reference_max_iterations = 4096;

		atmosphere_length fine_step = kilometers(0.05f);
		atmosphere_length skip_step = fine_step * 4.f;
		atmosphere_length step_growth_distance = kilometers(8.f);
		atmosphere_length ambient_depth = kilometers(0.15f);
		atmosphere_length powder_depth = kilometers(0.1f);
		atmosphere_length detail_fade_start = kilometers(15.f);
		atmosphere_length detail_fade_end = kilometers(40.f);
		angle shadow_min_sun_elevation = degrees(1.15f);

		float light_cone_spread = 0.15f;
		float light_long_sample_scale = 3.f;
		float light_march_min_transmittance = 0.1f;
		float view_march_min_transmittance = 0.02f;
		float scene_background_depth = 1e-5f;
	};

	struct shape_bake_bindings {
		shaders::storage_image_3d<vec4f, shape_noise_format> cloud_shape_out;
	};

	struct detail_bake_bindings {
		shaders::storage_image_3d<vec4f> cloud_detail_out;
	};

	struct weather_bake_bindings {
		shaders::storage_image_3d<vec4f> cloud_weather_out;
	};

	struct raymarch_bindings {
		shaders::texture2d<vec4f> transmittance_in;
		shaders::texture2d<vec4f> sky_view_in;
		shaders::texture3d<vec4f> cloud_shape_in;
		shaders::texture3d<vec4f> cloud_detail_in;
		shaders::storage_image<vec4f> cloud_out;
		shaders::uniform_block<cloud_data> cloud_ubo;
		shaders::sampler_state sky_view_sampler_binding;
		shaders::sampler_state transmittance_sampler;
		shaders::sampler_state noise_sampler_binding;
		shaders::texture3d<vec4f> cloud_weather_in;
		shaders::texture2d<vec4f> scene_depth_in;
		shaders::sampler_state scene_depth_sampler;
	};

	struct resolve_bindings {
		shaders::texture2d<vec4f> resolve_trace_in;
		shaders::texture2d<vec4f> resolve_history_in;
		shaders::sampler_state resolve_sampler_binding;
	};

	struct resolve_delta_bindings {
		shaders::texture2d<vec4f> resolve_delta_in;
	};

	struct shadow_map_bindings {
		shaders::texture3d<vec4f> cloud_shape_in;
		shaders::uniform_block<cloud_data> cloud_ubo;
		shaders::sampler_state noise_sampler_binding;
		shaders::storage_image<vec4f> cloud_shadow_out;
		shaders::texture3d<vec4f> cloud_weather_in;
	};

	struct [[= shaders::shader_struct]] cloud_push_constants {
		vec3f sun_direction;
		atmosphere_length camera_altitude;
		vec3<irradiance> sun_irradiance;
		float frame_index;
		vec3<atmosphere_length> wind_offset;
		std::uint32_t block_size;
		vec2f jitter_ndc;
		vec2f screen_texel;
		vec2f depth_uv_scale;
		vec2f depth_uv_max;
		vec2u resolve_extent;
		vec2u trace_extent;
		vec2u sample_offset;
		std::int32_t march_quality;
		std::uint32_t footprint_taps;
	};

	struct [[= shaders::shader_struct]] cloud_resolve_push_constants {
		vec2f jitter_ndc;
		vec2f prev_jitter_ndc;
		vec2f history_uv_scale;
		vec2f history_uv_max;
		vec2f delta_uv_scale;
		vec2f delta_uv_max;
		vec2u resolve_extent;
		vec2u trace_extent;
		vec2u sample_offset;
		std::uint32_t block_size;
		std::int32_t history_valid;
	};

	struct [[= shaders::shader_struct]] cloud_shadow_push_constants {
		vec2u block_offset;
		std::uint32_t stride;
	};

	constexpr std::uint32_t shadow_interleave_divisor = 2;
	constexpr std::uint32_t raymarch_interleave_divisor = 2;
	constexpr std::uint32_t upsample_reach_resolve_pixels = 3;

	using bake_pass = gpu::specs<gpu::helpers<"Clouds/cloud_common">, gpu::threads<8, 8, 1>>;

	using shape_bake_entry = gpu::compute_entry<gpu::body_path<"Compute/cloud_shape_bake">, gpu::bindings<shape_bake_bindings>, bake_pass>;

	using detail_bake_entry = gpu::compute_entry<gpu::body_path<"Compute/cloud_detail_bake">, gpu::bindings<detail_bake_bindings>, bake_pass>;

	using weather_bake_entry = gpu::compute_entry<gpu::body_path<"Compute/cloud_weather_bake">, gpu::bindings<weather_bake_bindings>, bake_pass>;

	using raymarch_entry = gpu::compute_entry<
		gpu::body_path<"Compute/cloud_raymarch">,
		gpu::types<cloud_limits, shaders::common::shader_types>,
		gpu::bindings<raymarch_bindings, atmosphere::bindings, shaders::standard_3d::camera>,
		gpu::helpers<"Atmosphere/atmosphere_common", "Clouds/cloud_common", "Clouds/cloud_density", "Screen/screen_target">,
		gpu::push_constant<cloud_push_constants>,
		gpu::threads<8, 8, 1>
	>;

	using delta_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/CloudDelta">,
		gpu::types<shaders::common::shader_types>,
		gpu::bindings<resolve_bindings, shaders::standard_3d::camera>,
		gpu::fullscreen_pass,
		gpu::helpers<"Clouds/cloud_common", "Screen/screen_target", "Clouds/cloud_history">,
		gpu::push_constant<cloud_resolve_push_constants>,
		gpu::color_targets<gpu::color_format::hdr>
	>;

	using resolve_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/CloudResolve">,
		gpu::types<shaders::common::shader_types>,
		gpu::bindings<resolve_bindings, resolve_delta_bindings, shaders::standard_3d::camera>,
		gpu::fullscreen_pass,
		gpu::helpers<"Clouds/cloud_common", "Screen/screen_target", "Clouds/cloud_history">,
		gpu::push_constant<cloud_resolve_push_constants>,
		gpu::color_targets<gpu::color_format::hdr>
	>;

	using shadow_entry = gpu::compute_entry<
		gpu::body_path<"Compute/cloud_shadow_map">,
		gpu::types<cloud_limits>,
		gpu::bindings<shadow_map_bindings, atmosphere::bindings, shadow_bindings>,
		gpu::helpers<"Clouds/cloud_common", "Clouds/cloud_density">,
		gpu::push_constant<cloud_shadow_push_constants>,
		gpu::threads<8, 8, 1>
	>;

	constexpr gpu::sampler_desc sky_view_sampler_desc{
		.address_v = gpu::sampler_address_mode::clamp_to_edge,
		.address_w = gpu::sampler_address_mode::clamp_to_edge,
	};

	constexpr gpu::sampler_desc depth_sampler_desc{
		.min = gpu::sampler_filter::nearest,
		.mag = gpu::sampler_filter::nearest,
		.address_u = gpu::sampler_address_mode::clamp_to_edge,
		.address_v = gpu::sampler_address_mode::clamp_to_edge,
		.address_w = gpu::sampler_address_mode::clamp_to_edge,
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

	auto trace_extent_for(
		vec2u resolve_extent,
		std::uint32_t block_size
	) -> vec2u;

	auto sample_slot(
		std::uint32_t frame,
		std::uint32_t cycle
	) -> std::uint32_t;

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

auto gse::renderer::cloud::trace_extent_for(const vec2u resolve_extent, const std::uint32_t block_size) -> vec2u {
	return vec2u{
		(resolve_extent.x() + block_size - 1) / block_size,
		(resolve_extent.y() + block_size - 1) / block_size,
	};
}

auto gse::renderer::cloud::sample_slot(const std::uint32_t frame, const std::uint32_t cycle) -> std::uint32_t {
	auto step = (cycle * 7u + 8u) / 16u;
	while (std::gcd(step, cycle) != 1u) {
		++step;
	}
	return frame % cycle * step % cycle;
}

auto gse::renderer::cloud::recreate_cloud_target(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
	const auto previous_target_allocated = d.cloud_target_allocated_extent;
	const auto previous_resolve_allocated = d.layer.allocated_extent;
	const auto previous_resolve_active = d.layer.extent;
	d.applied_target_divisor = std::max(d.target_divisor, 1);
	d.applied_resolve_divisor = effective_resolve_divisor(d);
	d.layer.extent = compute_cloud_target_extent(gpu_s.render_graph->extent(), d.applied_resolve_divisor);
	d.layer.allocated_extent = compute_cloud_target_extent(gpu_s.render_graph->allocated_extent(), d.applied_resolve_divisor);
	d.cloud_target_allocated_extent = trace_extent_for(d.layer.allocated_extent, static_cast<std::uint32_t>(d.applied_target_divisor / d.applied_resolve_divisor));

	if (d.layer.extent != previous_resolve_active) {
		d.frames_since_history_invalid = 0;
	}

	if (d.cloud_target.handle() && d.cloud_target_allocated_extent == previous_target_allocated && d.layer.allocated_extent == previous_resolve_allocated) {
		update_depth_descriptor(gpu_s, d);
		return;
	}

	d.frames_since_history_invalid = 0;
	d.cloud_target = gpu_s.device->create_image(
		{
			.size = d.cloud_target_allocated_extent,
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.usage = { gpu::image_flag::storage, gpu::image_flag::sampled },
		},
		"cloud_target"
	);
	gpu::transition_image_to(*gpu_s.device, d.cloud_target);

	d.cloud_delta = gpu_s.device->create_image(
		{
			.size = d.cloud_target_allocated_extent,
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.usage = { gpu::image_flag::color_attachment, gpu::image_flag::sampled },
		},
		"cloud_delta"
	);
	gpu::transition_image_to(*gpu_s.device, d.cloud_delta);

	for (std::size_t i = 0; i < d.layer.images.frames_in_flight; ++i) {
		d.layer.images[i] = gpu_s.device->create_image(
			{
				.size = d.layer.allocated_extent,
				.format = gpu::image_format::r16g16b16a16_sfloat,
				.usage = { gpu::image_flag::color_attachment, gpu::image_flag::sampled },
			},
			std::format("cloud_resolve_{}", i)
		);
		gpu::transition_image_to(*gpu_s.device, d.layer.images[i]);
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
	{
		gpu::program_batch batch(*gpu_s.device);
		batch.add(d.shape_bake_pipeline, shape_bake_entry::pod);
		batch.add(d.detail_bake_pipeline, detail_bake_entry::pod);
		batch.add(d.raymarch_pipeline, raymarch_entry::pod);
		batch.add(d.delta_pipeline, delta_entry::pod);
		batch.add(d.resolve_pipeline, resolve_entry::pod);
		batch.add(d.shadow_pipeline, shadow_entry::pod);
		batch.add(d.weather_bake_pipeline, weather_bake_entry::pod);
	}

	d.shape_noise = gpu_s.device->create_image(
		{
			.size = vec2u{ shape_noise_size.x(), shape_noise_size.y() },
			.depth = shape_noise_size.z(),
			.format = shape_noise_format,
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

	d.cloud_ubo_buffer = gpu_s.device->create_buffer<cloud_data>(
		1,
		{
			.usage = gpu::buffer_flag::storage,
		},
		"cloud_ubo"
	);

	d.shadow_ubo_buffer = gpu_s.device->create_buffer<cloud_shadow_data>(
		1,
		{
			.usage = gpu::buffer_flag::storage,
		},
		"cloud_shadow_ubo"
	);

	d.shadow_sampler = gpu_s.device->register_sampler(gpu::linear_clamp_sampler);
	d.noise_sampler = gpu_s.device->register_sampler({});
	d.atmosphere_lut_sampler = gpu_s.device->register_sampler(gpu::linear_clamp_sampler);
	d.sky_view_sampler = gpu_s.device->register_sampler(sky_view_sampler_desc);
	d.resolve_sampler = gpu_s.device->register_sampler(gpu::linear_clamp_sampler);
	d.depth_sampler = gpu_s.device->register_sampler(depth_sampler_desc);

	gpu_s.render_graph->on_resize(
		[gpu_s, &d]() {
			recreate_cloud_target(gpu_s, d);
		}
	);

	return {};
}

auto gse::renderer::cloud::frame(const context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out, const channel_read<weather_request> weather_in, shared_view<atmosphere::data> atm_state, shared_view<camera::data> cam_state, shared_view<camera_latch::data> latch_s, const channel_read<camera::temporal_reset_request> reset_in) -> async::task<> {
	d.layer.rendered = false;

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

	if (!atm_state.atmosphere_ubo_buffer.valid()) {
		co_return;
	}

	if (!d.enabled || !cam_state.world_visible) {
		d.frames_since_history_invalid = 0;
		co_return;
	}

	const cloud_data shader_payload = build_cloud_data(d);
	d.cloud_ubo_buffer.host_write(shader_payload);

	if (!d.noises_ready) {
		auto rec = co_await gpu::pass<^^shape_bake_pass>(pass_out).pipeline(d.shape_bake_pipeline);
		rec.dispatch<shape_bake_entry>(
			{
				.cloud_shape_out = d.shape_noise,
			},
			gpu::group_count<shape_bake_entry>(shape_noise_size)
		);

		rec = co_await gpu::pass<^^detail_bake_pass>(pass_out).pipeline(d.detail_bake_pipeline).after<^^shape_bake_pass>();
		rec.dispatch<detail_bake_entry>(
			{
				.cloud_detail_out = d.detail_noise,
			},
			gpu::group_count<detail_bake_entry>(detail_noise_size)
		);

		rec = co_await gpu::pass<^^weather_bake_pass>(pass_out).pipeline(d.weather_bake_pipeline).after<^^detail_bake_pass>();
		rec.dispatch<weather_bake_entry>(
			{
				.cloud_weather_out = d.weather_map,
			},
			gpu::group_count<weather_bake_entry>(weather_map_size)
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
		const auto block_edge = (edge + stride - 1) / stride;

		auto shadow_rec = co_await gpu::pass<^^cloud_shadow_pass>(pass_out)
			.pipeline(d.shadow_pipeline)
			.after<^^weather_bake_pass>();

		shadow_rec.dispatch<shadow_entry>(
			{
				.block_offset = stride == 1u ? vec2u{ 0, 0 } : vec2u{ shadow_slot % stride, shadow_slot / stride },
				.stride = stride,
			},
			{
				.cloud_shape_in = d.shape_noise,
				.cloud_ubo = d.cloud_ubo_buffer,
				.noise_sampler_binding = d.noise_sampler,
				.cloud_shadow_out = d.shadow_map,
				.cloud_weather_in = d.weather_map,
				.atmosphere_ubo = atm_state.atmosphere_ubo_buffer,
				.cloud_shadow_ubo = d.shadow_ubo_buffer,
			},
			gpu::group_count<shadow_entry>(vec2u{ block_edge, block_edge })
		);
	}

	if (!atm_state.enabled) {
		d.frames_since_history_invalid = 0;
		co_return;
	}

	const view_snapshot current_view{
		.jitter_ndc = cam_state.jitter_ndc,
	};

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
	const auto previous_view = history_ready ? d.previous_view : current_view;
	d.previous_view = current_view;

	const auto trace_ratio = static_cast<std::uint32_t>(d.applied_target_divisor / d.applied_resolve_divisor);
	const auto block_size = trace_ratio * (history_ready ? raymarch_interleave_divisor : 1u);
	const auto slot = sample_slot(d.frame_counter, block_size * block_size);
	const auto sample_offset = vec2u{ slot % block_size, slot / block_size };
	const auto resolve_extent = d.layer.extent;
	const auto trace_extent = trace_extent_for(resolve_extent, block_size);
	const auto screen_extent = gpu_s.render_graph->extent();
	const auto camera_slot = latch_s.camera_buffers[history_index].slot();

	auto rec = co_await gpu::pass<^^cloud_raymarch_pass>(pass_out)
		.pipeline(d.raymarch_pipeline)
		.after<^^camera_latch::frame, ^^atmosphere::sky_view_pass, ^^weather_bake_pass, ^^depth_prepass::frame>();

	rec.dispatch<raymarch_entry>(
		{
			.sun_direction = atm_state.sun_direction,
			.camera_altitude = atm_state.camera_altitude,
			.sun_irradiance = sun_irradiance,
			.frame_index = static_cast<float>(d.frame_counter),
			.wind_offset = d.wind_offset,
			.block_size = block_size,
			.jitter_ndc = current_view.jitter_ndc,
			.screen_texel = vec2f{
				1.f / static_cast<float>(screen_extent.x()),
				1.f / static_cast<float>(screen_extent.y()),
			},
			.depth_uv_scale = gpu_s.render_graph->screen_uv_scale(),
			.depth_uv_max = gpu_s.render_graph->screen_uv_max(),
			.resolve_extent = resolve_extent,
			.trace_extent = trace_extent,
			.sample_offset = sample_offset,
			.march_quality = static_cast<std::int32_t>(d.march_quality),
			.footprint_taps = upsample_reach_resolve_pixels * static_cast<std::uint32_t>(d.applied_resolve_divisor),
		},
		{
			.transmittance_in = atm_state.transmittance_lut,
			.sky_view_in = atm_state.sky_view_lut,
			.cloud_shape_in = d.shape_noise,
			.cloud_detail_in = d.detail_noise,
			.cloud_out = d.cloud_target,
			.cloud_ubo = d.cloud_ubo_buffer,
			.sky_view_sampler_binding = d.sky_view_sampler,
			.transmittance_sampler = d.atmosphere_lut_sampler,
			.noise_sampler_binding = d.noise_sampler,
			.cloud_weather_in = d.weather_map,
			.scene_depth_in = d.depth_view,
			.scene_depth_sampler = d.depth_sampler,
			.atmosphere_ubo = atm_state.atmosphere_ubo_buffer,
			.camera_ubo = camera_slot,
		},
		gpu::group_count<raymarch_entry>(trace_extent)
	);

	const cloud_resolve_push_constants resolve_constants{
		.jitter_ndc = current_view.jitter_ndc,
		.prev_jitter_ndc = previous_view.jitter_ndc,
		.history_uv_scale = gpu::screen_uv_scale_for(d.layer.extent, d.layer.allocated_extent),
		.history_uv_max = gpu::screen_uv_max_for(d.layer.extent, d.layer.allocated_extent),
		.delta_uv_scale = gpu::screen_uv_scale_for(trace_extent, d.cloud_target_allocated_extent),
		.delta_uv_max = gpu::screen_uv_max_for(trace_extent, d.cloud_target_allocated_extent),
		.resolve_extent = resolve_extent,
		.trace_extent = trace_extent,
		.sample_offset = sample_offset,
		.block_size = block_size,
		.history_valid = history_ready ? 1 : 0,
	};

	auto delta_rec = co_await gpu::pass<^^cloud_delta_pass>(pass_out)
		.pipeline(d.delta_pipeline)
		.color(gpu::clear_color(
			gpu::color_clear{ 0.0f, 0.0f, 0.0f, 0.0f },
			d.cloud_delta
		))
		.after<^^cloud_raymarch_pass>();

	delta_rec.set_viewport(trace_extent);
	delta_rec.set_scissor(trace_extent);
	delta_rec.push_bindings<delta_entry>(
		resolve_constants,
		{
			.resolve_trace_in = d.cloud_target,
			.resolve_history_in = d.layer.images[previous_index],
			.resolve_sampler_binding = d.resolve_sampler,
			.camera_ubo = camera_slot,
		}
	);
	delta_rec.draw(3);

	auto resolve_rec = co_await gpu::pass<^^cloud_resolve_pass>(pass_out)
		.pipeline(d.resolve_pipeline)
		.color(gpu::clear_color(
			gpu::color_clear{ 0.0f, 0.0f, 0.0f, 1.0f },
			d.layer.images[history_index]
		))
		.after<^^cloud_delta_pass>();

	resolve_rec.set_viewport(resolve_extent);
	resolve_rec.set_scissor(resolve_extent);
	resolve_rec.push_bindings<resolve_entry>(
		resolve_constants,
		{
			.resolve_trace_in = d.cloud_target,
			.resolve_history_in = d.layer.images[previous_index],
			.resolve_sampler_binding = d.resolve_sampler,
			.resolve_delta_in = d.cloud_delta,
			.camera_ubo = camera_slot,
		}
	);
	resolve_rec.draw(3);

	d.layer.rendered = true;
}

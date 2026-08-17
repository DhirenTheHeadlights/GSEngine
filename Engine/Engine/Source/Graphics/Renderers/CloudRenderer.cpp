module gse.graphics:cloud_renderer_impl;

import std;

import :cloud_renderer;
import :atmosphere_renderer;
import :camera_system;
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

	using cloud_types = type_pack<cloud_data, atmosphere::atmosphere_data>;

	struct [[
		= shaders::binding<0, 0>{},
		= shaders::storage_image_3d
	]] cloud_shape_out {
		using element = vec4f;
	};

	struct [[
		= shaders::binding<0, 0>{},
		= shaders::storage_image_3d
	]] cloud_detail_out {
		using element = vec4f;
	};

	struct [[
		= shaders::binding<0, 0>{},
		= shaders::texture2d
	]] transmittance_in {
		using element = vec4f;
	};

	struct [[
		= shaders::binding<0, 1>{},
		= shaders::texture2d
	]] sky_view_in {
		using element = vec4f;
	};

	struct [[
		= shaders::binding<0, 2>{},
		= shaders::texture3d
	]] cloud_shape_in {
		using element = vec4f;
	};

	struct [[
		= shaders::binding<0, 3>{},
		= shaders::texture3d
	]] cloud_detail_in {
		using element = vec4f;
	};

	struct [[
		= shaders::binding<0, 4>{},
		= shaders::storage_image
	]] cloud_out {
		using element = vec4f;
	};

	struct [[
		= shaders::binding<0, 8>{},
		= shaders::sampler_state
	]] transmittance_sampler {};

	struct [[
		= shaders::binding<0, 6>{},
		= shaders::sampler_state
	]] sky_view_sampler_binding {};

	struct [[
		= shaders::binding<0, 9>{},
		= shaders::sampler_state
	]] noise_sampler_binding {};

	struct [[
		= shaders::binding<0, 0>{},
		= shaders::texture2d
	]] cloud_in {
		using element = vec4f;
	};

	struct [[
		= shaders::binding<0, 1>{},
		= shaders::sampler_state
	]] cloud_composite_sampler {};

	struct [[= shaders::shader_struct]] cloud_push_constants {
		mat4f inv_view_proj;
		vec3f sun_direction;
		atmosphere_length camera_altitude;
		vec3<irradiance> sun_irradiance;
		float frame_index;
		vec3<atmosphere_length> wind_offset;
	};

	using shape_bake_bindings = type_pack<cloud_shape_out>;
	using detail_bake_bindings = type_pack<cloud_detail_out>;
	using raymarch_bindings = type_pack<atmosphere::atmosphere_ubo, cloud_ubo, transmittance_in, sky_view_in, cloud_shape_in, cloud_detail_in, cloud_out, transmittance_sampler, sky_view_sampler_binding, noise_sampler_binding>;
	using composite_bindings = type_pack<cloud_in, cloud_composite_sampler>;

	using shape_bake_entry = gpu::compute_entry<gpu::body_path<"Compute/cloud_shape_bake">, gpu::bindings<shape_bake_bindings>, gpu::helpers<"Clouds/cloud_common">, gpu::threads<8, 8, 1>, gpu::system_values<gpu::dispatch_thread_id>>;

	using detail_bake_entry = gpu::compute_entry<gpu::body_path<"Compute/cloud_detail_bake">, gpu::bindings<detail_bake_bindings>, gpu::helpers<"Clouds/cloud_common">, gpu::threads<8, 8, 1>, gpu::system_values<gpu::dispatch_thread_id>>;

	using raymarch_entry = gpu::compute_entry<
		gpu::body_path<"Compute/cloud_raymarch">,
		gpu::types<cloud_types>,
		gpu::bindings<raymarch_bindings>,
		gpu::helpers<"Atmosphere/atmosphere_common", "Clouds/cloud_common">,
		gpu::push_constant<cloud_push_constants>,
		gpu::threads<8, 8, 1>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	using composite_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/CloudComposite">,
		gpu::bindings<composite_bindings>,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::color_targets<gpu::color_format::hdr>,
		gpu::blend<gpu::blend_preset::alpha>,
		gpu::depth<true, false, gpu::compare_op::less_or_equal>
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

	auto compute_cloud_target_extent(
		vec2u screen_extent,
		int divisor
	) -> vec2u;

	auto recreate_cloud_target(
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> void;

	auto build_cloud_data(
		const data& d
	) -> cloud_data;
}

auto gse::renderer::cloud::compute_cloud_target_extent(const vec2u screen_extent, const int divisor) -> vec2u {
	const auto d = static_cast<std::uint32_t>(std::max(divisor, 1));
	const auto w = std::max(screen_extent.x() / d, 1u);
	const auto h = std::max(screen_extent.y() / d, 1u);
	return vec2u{ w, h };
}

auto gse::renderer::cloud::recreate_cloud_target(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
	d.applied_target_divisor = std::max(d.target_divisor, 1);
	d.cloud_target_extent = compute_cloud_target_extent(gpu_s.render_graph->extent(), d.applied_target_divisor);
	d.cloud_target = gpu_s.device->create_image(
		{
			.size = d.cloud_target_extent,
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.usage = { gpu::image_flag::storage, gpu::image_flag::sampled },
			.bindless = true,
		},
		"cloud_target"
	);
	gpu::transition_image_to(*gpu_s.device, d.cloud_target);
}

auto gse::renderer::cloud::build_cloud_data(const data& d) -> cloud_data {
	return cloud_data{
		.cloud_bottom = d.cloud_bottom,
		.cloud_top = d.cloud_top,
		.cloud_coverage = d.cloud_coverage,
		.cloud_type = d.cloud_type,
		.density_multiplier = d.density_multiplier,
		.view_extinction = d.view_extinction,
		.light_extinction = d.light_extinction,
		.shape_scale = d.shape_scale,
		.detail_scale = d.detail_scale,
		.detail_strength = d.detail_strength,
		.phase_g_forward = d.phase_g_forward,
		.phase_g_back = d.phase_g_back,
		.phase_blend = d.phase_blend,
		.ambient_strength = d.ambient_strength,
		.max_distance = d.max_distance,
	};
}

auto gse::renderer::cloud::init(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	d.shape_bake_pipeline = gpu::build_compute_program(*gpu_s.device, shape_bake_entry::pod);
	d.detail_bake_pipeline = gpu::build_compute_program(*gpu_s.device, detail_bake_entry::pod);
	d.raymarch_pipeline = gpu::build_compute_program(*gpu_s.device, raymarch_entry::pod);
	d.composite_pipeline = gpu::build_graphics_program(*gpu_s.device, composite_entry::pod);

	d.shape_noise = gpu_s.device->create_image(
		{
			.size = vec2u{ shape_noise_size.x(), shape_noise_size.y() },
			.depth = shape_noise_size.z(),
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.view = gpu::image_view_type::e3d,
			.usage = { gpu::image_flag::storage, gpu::image_flag::sampled },
			.bindless = true,
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
			.bindless = true,
		},
		"cloud_detail_noise"
	);
	gpu::transition_image_to(*gpu_s.device, d.shape_noise);
	gpu::transition_image_to(*gpu_s.device, d.detail_noise);

	recreate_cloud_target(gpu_s, d);

	d.cloud_ubo_buffer = gpu_s.device->create_buffer(
		{
			.size = sizeof(cloud_data),
			.stride = sizeof(cloud_data),
			.usage = gpu::buffer_flag::storage,
			.bindless = true,
		},
		"cloud_ubo"
	);

	d.noise_sampler = gpu_s.device->register_sampler(noise_sampler_desc);
	d.atmosphere_lut_sampler = gpu_s.device->register_sampler(atmosphere_lut_sampler_desc);
	d.sky_view_sampler = gpu_s.device->register_sampler(sky_view_sampler_desc);
	d.composite_sampler = gpu_s.device->register_sampler(composite_sampler_desc);

	gpu::context::on_swap_chain_recreate(
		gpu_s,
		[gpu_s, &d]() {
			recreate_cloud_target(gpu_s, d);
		}
	);

	return {};
}

auto gse::renderer::cloud::frame(const context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out, shared_view<atmosphere::data> atm_state, shared_view<camera::data> cam_state) -> async::task<> {
	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	if (std::max(d.target_divisor, 1) != d.applied_target_divisor) {
		gpu_s.device->wait_idle();
		recreate_cloud_target(gpu_s, d);
	}

	if (!d.cloud_target.valid()) {
		co_return;
	}

	if (!atm_state.atmosphere_ubo_buffer.valid()) {
		co_return;
	}

	if (!d.enabled) {
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

		d.noises_ready = true;
	}

	const auto view = cam_state.view_matrix;
	const auto proj = cam_state.projection_matrix;
	const auto inv_view_proj = (proj * view).inverse();

	const auto sun_irradiance = vec3<irradiance>{
		atm_state.sun_intensity * atm_state.sun_color.x(),
		atm_state.sun_intensity * atm_state.sun_color.y(),
		atm_state.sun_intensity * atm_state.sun_color.z(),
	};

	const auto raymarch_groups = vec2u{
		(d.cloud_target_extent.x() + 7) / 8,
		(d.cloud_target_extent.y() + 7) / 8,
	};

	d.frame_counter += 1;

	auto rec = co_await gpu::pass<^^cloud_raymarch_pass>(pass_out)
		.pipeline(d.raymarch_pipeline)
		.after<^^atmosphere::sky_view_pass, ^^detail_bake_pass>();

	rec.dispatch<raymarch_entry>(
		{
			.inv_view_proj = inv_view_proj,
			.sun_direction = atm_state.sun_direction,
			.camera_altitude = atm_state.camera_altitude,
			.sun_irradiance = sun_irradiance,
			.frame_index = static_cast<float>(d.frame_counter),
			.wind_offset = d.wind_offset,
		},
		{
			.transmittance_in = atm_state.transmittance_lut.sampled_slot(),
			.sky_view_in = atm_state.sky_view_lut.sampled_slot(),
			.cloud_shape_in = d.shape_noise.sampled_slot(),
			.cloud_detail_in = d.detail_noise.sampled_slot(),
			.cloud_out = d.cloud_target.storage_slot(),
			.cloud_ubo = d.cloud_ubo_buffer.slot(),
			.sky_view_sampler_binding = d.sky_view_sampler.slot(),
			.atmosphere_ubo = atm_state.atmosphere_ubo_buffer.slot(),
			.transmittance_sampler = d.atmosphere_lut_sampler.slot(),
			.noise_sampler_binding = d.noise_sampler.slot(),
		},
		vec3u{ raymarch_groups.x(), raymarch_groups.y(), 1 }
	);

	const auto ext = gpu_s.render_graph->extent();
	auto composite_rec = co_await gpu::pass<^^cloud_composite_pass>(pass_out)
		.pipeline(d.composite_pipeline)
		.color(gpu::load_color(gpu_s.render_graph->framebuffer_image<targets::hdr_color>()))
		.depth(gpu::load_depth())
		.after<^^atmosphere::sky_raster_pass, ^^cloud_raymarch_pass, ^^sdf_grid::frame, ^^world_text::frame>();

	composite_rec.sample_image(d.cloud_target, gpu::pipeline_stage_flag::fragment_shader);
	composite_rec.set_viewport(ext);
	composite_rec.set_scissor(ext);
	composite_rec.push_bindings<composite_entry>({
		.cloud_in = d.cloud_target.sampled_slot(),
		.cloud_composite_sampler = d.composite_sampler.slot(),
	});
	composite_rec.draw(3);
}

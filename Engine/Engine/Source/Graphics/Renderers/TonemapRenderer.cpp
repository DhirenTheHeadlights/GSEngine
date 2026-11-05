module gse.graphics:tonemap_renderer_impl;

import std;

import :tonemap_renderer;
import :atmosphere_renderer;
import :bloom_renderer;
import :depth_prepass_renderer;
import :forward_renderer;
import :physics_debug_renderer;
import :render_targets;
import :sdf_grid_renderer;
import :taa_renderer;
import :world_text_renderer;


import gse.gpu;
import gse.gpu_record;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.meta;
import gse.time;

namespace gse::renderer::tonemap {
	constexpr std::uint32_t histogram_tile = 4;

	struct [[= shaders::shader_constant_block]] exposure_limits {
		std::uint32_t histogram_bins = tonemap::histogram_bins;
		std::uint32_t histogram_tile = tonemap::histogram_tile;
		std::uint32_t histogram_sample_step = 2;
		irradiance histogram_min_luminance = watts_per_square_meter(0.001f);
		irradiance histogram_max_luminance = watts_per_square_meter(65536.f);
		vec3f luminance_weights = { 0.2126f, 0.7152f, 0.0722f };
		float middle_grey = 0.18f;
	};

	struct [[= shaders::texture2d]] hdr_color {
		using element = vec4f;
	};

	struct [[= shaders::texture2d]] bloom_color {
		using element = vec4f;
	};

	struct [[= shaders::texture2d]] velocity_color {
		using element = vec4f;
	};

	struct [[= shaders::sampler_state]] color_sampler {};

	struct [[= shaders::ssbo_readonly]] exposure_in {
		using element = float;
	};

	struct [[= shaders::texture2d]] histogram_source {
		using element = vec4f;
	};

	struct [[= shaders::ssbo_readwrite]] histogram_out {
		using element = std::uint32_t;
	};

	struct [[= shaders::ssbo_readwrite]] histogram_in {
		using element = std::uint32_t;
	};

	struct [[= shaders::ssbo_readwrite]] exposure_out {
		using element = float;
	};

	struct [[= shaders::shader_struct]] push_constants {
		float exposure;
		float bloom_intensity;
		std::uint32_t show_velocity;
		std::uint32_t auto_exposure;
		vec2f hdr_uv_scale;
		vec2f hdr_uv_max;
		vec2f bloom_uv_scale;
		vec2f bloom_uv_max;
	};

	struct [[= shaders::shader_struct]] exposure_push_constants {
		float low_percentile;
		float high_percentile;
		float compensation;
		float blend_bright;
		float blend_dark;
		irradiance min_luminance;
		irradiance max_luminance;
		irradiance incident_luminance;
	};

	using exposure_types = type_pack<exposure_limits>;

	using shader_binding_types = type_pack<hdr_color, bloom_color, velocity_color, color_sampler, exposure_in>;
	using histogram_binding_types = type_pack<histogram_source, histogram_out>;
	using exposure_binding_types = type_pack<histogram_in, exposure_out>;

	using entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/Tonemap">,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"Screen/screen_pass", "Screen/screen_target">,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::depth<false, false>,
		gpu::depth_target<gpu::depth_format::none>
	>;

	struct [[= shaders::shader_struct]] histogram_push_constants {
		vec2u active_extent;
	};

	using histogram_entry = gpu::compute_entry<
		gpu::body_path<"Compute/exposure_histogram">,
		gpu::types<exposure_types>,
		gpu::bindings<histogram_binding_types>,
		gpu::push_constant<histogram_push_constants>,
		gpu::threads<16, 16, 1>,
		gpu::system_values<gpu::dispatch_thread_id, gpu::group_index>
	>;

	using exposure_entry = gpu::compute_entry<
		gpu::body_path<"Compute/exposure_update">,
		gpu::types<exposure_types>,
		gpu::bindings<exposure_binding_types>,
		gpu::push_constant<exposure_push_constants>,
		gpu::threads<histogram_bins>,
		gpu::system_values<gpu::group_index>
	>;

	namespace atomic {
		struct [[= shaders::ssbo_readwrite]] histogram_out {
			using element = shaders::atomic_u32;
		};

		struct [[= shaders::ssbo_readwrite]] histogram_in {
			using element = shaders::atomic_u32;
		};

		using histogram_entry = gpu::compute_entry<
			gpu::body_path<"Compute/exposure_histogram_atomic">,
			gpu::types<exposure_types>,
			gpu::bindings<type_pack<histogram_source, histogram_out>>,
			gpu::push_constant<histogram_push_constants>,
			gpu::threads<16, 16, 1>,
			gpu::system_values<gpu::dispatch_thread_id, gpu::group_index>
		>;

		using exposure_entry = gpu::compute_entry<
			gpu::body_path<"Compute/exposure_update_atomic">,
			gpu::types<exposure_types>,
			gpu::bindings<type_pack<histogram_in, exposure_out>>,
			gpu::push_constant<exposure_push_constants>,
			gpu::threads<histogram_bins>,
			gpu::system_values<gpu::group_index>
		>;
	}

	template <typename HistogramEntry, typename ExposureEntry>
	auto record_exposure(
		channel_write<gpu::render_pass_request> pass_out,
		data& d,
		vec2u ext,
		std::uint32_t frame_index,
		const exposure_push_constants& exposure_push
	) -> async::task<>;

	auto rebind_views(
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> void;

	auto adaptation_blend(
		time dt,
		time constant
	) -> float;
}

auto gse::renderer::tonemap::rebind_views(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
	auto& hdr = gpu_s.render_graph->framebuffer_image<targets::post_taa_color>();
	if (hdr.handle()) {
		if (!d.hdr_view.valid()) {
			d.hdr_view = gpu_s.device->allocate_image_slot();
		}
		gpu_s.device->write_sampled_image(d.hdr_view.slot(), hdr);
	}
	auto& velocity = gpu_s.render_graph->framebuffer_image<targets::velocity>();
	if (velocity.handle()) {
		if (!d.velocity_view.valid()) {
			d.velocity_view = gpu_s.device->allocate_image_slot();
		}
		gpu_s.device->write_sampled_image(d.velocity_view.slot(), velocity);
	}
}

auto gse::renderer::tonemap::adaptation_blend(const time dt, const time constant) -> float {
	if (!(constant > time{})) {
		return 1.f;
	}
	return 1.f - std::exp(-(dt / constant));
}

template <typename HistogramEntry, typename ExposureEntry>
auto gse::renderer::tonemap::record_exposure(const channel_write<gpu::render_pass_request> pass_out, data& d, const vec2u ext, const std::uint32_t frame_index, const exposure_push_constants& exposure_push) -> async::task<> {
	auto histogram_rec = co_await gpu::pass<^^histogram_pass>(pass_out)
		.pipeline(d.histogram_pipeline)
		.after<^^forward::frame, ^^atmosphere::sky_raster_pass, ^^physics_debug::frame, ^^sdf_grid::frame, ^^world_text::frame, ^^taa::frame>();
	histogram_rec.template dispatch<HistogramEntry>(
		{
			.active_extent = ext,
		},
		{
			.histogram_source = d.hdr_view.slot(),
			.histogram_out = d.histogram_buffers[frame_index].slot(),
		},
		vec3u{ (ext.x() + 16u * histogram_tile - 1u) / (16u * histogram_tile), (ext.y() + 16u * histogram_tile - 1u) / (16u * histogram_tile), 1u }
	);

	auto exposure_rec = co_await gpu::pass<^^exposure_pass>(pass_out)
		.pipeline(d.exposure_pipeline)
		.after<^^histogram_pass>();
	exposure_rec.template dispatch<ExposureEntry>(
		exposure_push,
		{
			.histogram_in = d.histogram_buffers[frame_index].slot(),
			.exposure_out = d.exposure_buffer.slot(),
		},
		vec3u{ 1u, 1u, 1u }
	);
}

auto gse::renderer::tonemap::init(context& ctx, const shared_view<gpu::context::data> gpu_s, const shared_view<bloom::data> bloom_state, data& d) -> async::task<> {
	d.pipeline = gpu::build_graphics_program(*gpu_s.device, entry::pod);
	d.typed_atomics = !gpu_s.device->capabilities().untyped_atomics;
	if (d.typed_atomics) {
		d.histogram_pipeline = gpu::build_compute_program(*gpu_s.device, atomic::histogram_entry::pod);
		d.exposure_pipeline = gpu::build_compute_program(*gpu_s.device, atomic::exposure_entry::pod);
	}
	else {
		d.histogram_pipeline = gpu::build_compute_program(*gpu_s.device, histogram_entry::pod);
		d.exposure_pipeline = gpu::build_compute_program(*gpu_s.device, exposure_entry::pod);
	}

	d.sampler = gpu_s.device->register_sampler(
		{
			.min = gpu::sampler_filter::linear,
			.mag = gpu::sampler_filter::linear,
			.address_u = gpu::sampler_address_mode::clamp_to_edge,
			.address_v = gpu::sampler_address_mode::clamp_to_edge,
			.address_w = gpu::sampler_address_mode::clamp_to_edge,
		}
	);

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		d.histogram_buffers[i] = gpu_s.device->create_buffer(
			{
				.size = sizeof(std::uint32_t) * histogram_bins,
				.stride = sizeof(std::uint32_t),
				.usage = gpu::buffer_flag::storage,
				.writable = true,
			},
			"tonemap.histogram"
		);
		d.histogram_buffers[i].host_zero();
	}

	d.exposure_buffer = gpu_s.device->create_buffer(
		{
			.size = sizeof(float),
			.stride = sizeof(float),
			.usage = gpu::buffer_flag::storage,
			.writable = true,
		},
		"tonemap.exposure"
	);
	d.exposure_buffer.host_write(d.exposure);

	rebind_views(gpu_s, d);

	gpu_s.render_graph->on_resize(
		[gpu_s, &d]() {
			rebind_views(gpu_s, d);
		}
	);

	return {};
}

auto gse::renderer::tonemap::frame(const context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out, shared_view<bloom::data> bloom_state, shared_view<atmosphere::data> atm_state, const channel_read<camera::temporal_reset_request> reset_in) -> async::task<> {
	if (!gpu_s.render_graph->frame_in_progress() || !gpu_s.render_graph->target_live()) {
		co_return;
	}

	const auto& hdr = gpu_s.render_graph->framebuffer_image<targets::post_taa_color>();
	if (!hdr.handle()) {
		co_return;
	}

	const auto ext = gpu_s.render_graph->extent();
	const auto frame_index = gpu_s.render_graph->current_frame();

	const bool snap_exposure = !reset_in.of<camera::temporal_reset_request>().empty();

	if (d.auto_exposure) {
		const auto dt = system_clock::dt<time>();
		const exposure_limits limits{};
		const float daylight = dot(atm_state.sun_color * atm_state.sun_transmittance, limits.luminance_weights);
		const irradiance incident = d.incident_metering
			? atm_state.sun_intensity * daylight * (std::max(atm_state.sun_direction.y(), 0.f) + atm_state.sun_ambient_strength)
			: irradiance{};
		const irradiance card_luminance = limits.middle_grey * incident / std::numbers::pi_v<float>;

		const exposure_push_constants exposure_push{
			.low_percentile = d.low_percentile,
			.high_percentile = d.high_percentile,
			.compensation = std::exp2(d.exposure_compensation),
			.blend_bright = snap_exposure ? 1.f : adaptation_blend(dt, d.adaptation_time_bright),
			.blend_dark = snap_exposure ? 1.f : adaptation_blend(dt, d.adaptation_time_dark),
			.min_luminance = d.min_luminance,
			.max_luminance = d.max_luminance,
			.incident_luminance = card_luminance,
		};
		if (d.typed_atomics) {
			co_await record_exposure<atomic::histogram_entry, atomic::exposure_entry>(pass_out, d, ext, frame_index, exposure_push);
		}
		else {
			co_await record_exposure<histogram_entry, exposure_entry>(pass_out, d, ext, frame_index, exposure_push);
		}
	}

	const bool bloom_active = bloom_state.bloom_quality != bloom::quality_level::off && bloom_state.active_mip_count > 0;

	const auto bloom_slot = bloom_active ? bloom_state.mips_up[0].sampled_slot() : d.hdr_view.slot();
	const auto bloom_active_extent = bloom_active ? bloom_state.mip_extents[0] : gpu_s.render_graph->extent();
	const auto bloom_allocated_extent = bloom_active ? bloom_state.mip_allocated_extents[0] : gpu_s.render_graph->allocated_extent();

	auto rec = co_await gpu::pass<^^frame>(pass_out)
		.pipeline(d.pipeline)
		.color(gpu::load_color())
		.after<^^forward::frame, ^^physics_debug::frame, ^^sdf_grid::frame, ^^world_text::frame, ^^bloom::downsample_pass, ^^bloom::upsample_pass, ^^depth_prepass::frame, ^^taa::frame, ^^exposure_pass>();

	rec.set_viewport(ext);
	rec.set_scissor(ext);
	rec.push_bindings<entry>(
		{
			.exposure = d.exposure,
			.bloom_intensity = bloom_active ? bloom_state.bloom_intensity : 0.0f,
			.show_velocity = d.show_velocity ? 1u : 0u,
			.auto_exposure = d.auto_exposure ? 1u : 0u,
			.hdr_uv_scale = gpu_s.render_graph->screen_uv_scale(),
			.hdr_uv_max = gpu_s.render_graph->screen_uv_max(),
			.bloom_uv_scale = gpu::screen_uv_scale_for(bloom_active_extent, bloom_allocated_extent),
			.bloom_uv_max = gpu::screen_uv_max_for(bloom_active_extent, bloom_allocated_extent),
		},
		{
			.hdr_color = d.hdr_view.slot(),
			.bloom_color = bloom_slot,
			.velocity_color = d.velocity_view.slot(),
			.color_sampler = d.sampler.slot(),
			.exposure_in = d.exposure_buffer.slot(),
		}
	);
	rec.draw(3);
}

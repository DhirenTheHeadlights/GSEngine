module gse.graphics:ssr_renderer_impl;

import std;

import :camera_system;
import :depth_prepass_renderer;
import :forward_renderer;
import :lighting_source;
import :shared_shaders;
import :ssr_renderer;
import :taa_renderer;

import gse.concurrency;
import gse.core;
import gse.ecs;
import gse.gpu;
import gse.gpu_record;
import gse.math;

namespace gse::renderer::ssr {
	struct [[= shaders::uniform_block]] camera_ubo {
		using element = shaders::common::camera_data;
	};

	struct [[= shaders::texture2d]] depth_in {
		using element = vec4f;
	};

	struct [[= shaders::texture2d]] history_in {
		using element = vec4f;
	};

	struct [[= shaders::sampler_state]] history_sampler {};

	struct [[= shaders::storage_image]] radiance_out {
		using element = vec4f;
	};

	struct [[= shaders::shader_struct]]
	push_constants {
		vec2u extent;
		std::int32_t reflection_quality;
		length max_distance;
		vec2f screen_uv_scale;
		vec2f screen_uv_max;
		length thickness;
	};

	using entry = gpu::compute_entry<
		gpu::body_path<"Compute/ssr_trace">,
		gpu::types<shaders::common::shader_types, shaders::forward::shader_types>,
		gpu::bindings<type_pack<camera_ubo, depth_in, history_in, history_sampler, radiance_out>>,
		gpu::helpers<"Screen/depth_reconstruct", "Screen/screen_target", "Standard3D/lighting_shared">,
		gpu::threads<8, 8, 1>,
		gpu::push_constant<push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;

	auto recreate_targets(
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> void;
}

auto gse::renderer::ssr::recreate_targets(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
	d.radiance = gpu_s.device->create_image(
		{
			.size = gpu_s.render_graph->allocated_extent(),
			.format = gpu::image_format::r16g16b16a16_sfloat,
			.usage = { gpu::image_flag::storage, gpu::image_flag::sampled },
		},
		"ssr.radiance"
	);
	gpu::transition_image_to(*gpu_s.device, d.radiance);

	if (!d.depth_view.valid()) {
		d.depth_view = gpu_s.device->allocate_image_slot();
	}
	gpu_s.device->write_sampled_image(d.depth_view.slot(), gpu_s.render_graph->depth_image());
}

auto gse::renderer::ssr::init(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	d.pipeline = gpu::build_compute_program(*gpu_s.device, entry::pod);

	d.history_sampler = gpu_s.device->register_sampler(
		{
			.min = gpu::sampler_filter::linear,
			.mag = gpu::sampler_filter::linear,
			.address_u = gpu::sampler_address_mode::clamp_to_edge,
			.address_v = gpu::sampler_address_mode::clamp_to_edge,
			.address_w = gpu::sampler_address_mode::clamp_to_edge,
		}
	);

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		d.camera_ubo_buffers[i] = gpu_s.device->create_buffer(
			{
				.size = sizeof(shaders::common::camera_data),
				.stride = sizeof(shaders::common::camera_data),
				.usage = gpu::buffer_flag::storage,
			},
			"ssr.camera_ubo"
		);
	}

	recreate_targets(gpu_s, d);

	gpu_s.render_graph->on_resize(
		[gpu_s, &d]() {
			recreate_targets(gpu_s, d);
		}
	);

	return {};
}

auto gse::renderer::ssr::frame(context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out, const channel_write<lighting_source::screen_reflections> reflections_out, shared_view<camera::data> cam_state, shared_view<forward::data> forward_s, shared_view<taa::data> taa_s) -> async::task<> {
	reflections_out.push<lighting_source::screen_reflections>({
		.radiance = d.radiance.sampled_slot(),
		.pass = gpu::pass_id<^^frame>(),
	});

	if (!gpu_s.render_graph->frame_in_progress() || forward_s.reflection_quality == forward::reflection_quality_level::off) {
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();
	const auto previous_index = 1u - frame_index;
	const auto& history = taa_s.history[previous_index];
	if (!history.handle()) {
		co_return;
	}

	const auto view = cam_state.view_matrix;
	const auto proj = cam_state.projection_matrix;
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

	const auto ext = gpu_s.render_graph->extent();
	const length max_distance = meters(30.f);
	const length thickness = meters(0.5f);

	auto rec = co_await gpu::pass<^^frame>(pass_out)
		.pipeline(d.pipeline)
		.after<^^depth_prepass::frame>();
	rec.dispatch<entry>(
		{
			.extent = ext,
			.reflection_quality = static_cast<std::int32_t>(forward_s.reflection_quality),
			.max_distance = max_distance,
			.screen_uv_scale = gpu_s.render_graph->screen_uv_scale(),
			.screen_uv_max = gpu_s.render_graph->screen_uv_max(),
			.thickness = thickness,
		},
		{
			.camera_ubo = d.camera_ubo_buffers[frame_index].slot(),
			.depth_in = d.depth_view.slot(),
			.history_in = history.sampled_slot(),
			.history_sampler = d.history_sampler.slot(),
			.radiance_out = d.radiance.storage_slot(),
		},
		vec3u{ (ext.x() + 7) / 8, (ext.y() + 7) / 8, 1u }
	);
}

module gse.graphics:camera_latch_renderer_impl;

import std;

import :camera_latch_renderer;
import :camera_data;
import :camera_system;
import :shared_shaders;

import gse.assert;
import gse.os;
import gse.gpu;
import gse.gpu_record;
import gse.containers;
import gse.core;
import gse.concurrency;
import gse.ecs;
import gse.math;

namespace gse::renderer::camera_latch {
	struct [[= shaders::shader_struct]] push_constants {
		std::uint32_t model;
		std::uint32_t consumed_x;
		std::uint32_t consumed_y;
		angle yaw;
		angle pitch;
		angle min_pitch;
		angle max_pitch;
		length distance;
		vec2<angle> per_count;
	};

	struct [[= shaders::shader_struct]] latch_history {
		view_matrix cpu_view;
		view_matrix latched_view;
	};

	struct latch_bindings {
		shaders::uniform_block<shaders::common::camera_data> camera_in;
		shaders::ssbo_readonly<std::uint32_t> mouse_totals;
		shaders::ssbo_readwrite<shaders::common::camera_data> camera_out;
		shaders::ssbo_readwrite<latch_history> history;
	};

	using entry = gpu::compute_entry<
		gpu::body_path<"Compute/camera_latch">,
		gpu::types<shaders::common::shader_types, type_pack<latch_history>>,
		gpu::bindings<latch_bindings>,
		gpu::threads<1>,
		gpu::push_constant<push_constants>
	>;
}

auto gse::renderer::camera_latch::init(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	d.pipeline = gpu::build_compute_program(*gpu_s.device, entry::pod);

	d.mouse_totals = gpu_s.device->create_buffer<std::uint32_t>(
		2,
		{
			.usage = gpu::buffer_flag::storage,
		},
		"camera_latch.mouse_totals"
	);
	assert(d.mouse_totals.mapped<std::uint64_t>() != nullptr, "camera latch mouse totals must be persistently mapped");
	input::set_mouse_latch_sink(d.mouse_totals.mapped<std::uint64_t>());

	d.history = gpu_s.device->create_buffer<latch_history>(
		1,
		{
			.usage = gpu::buffer_flag::storage,
			.writable = true,
			.device_local = true,
		},
		"camera_latch.history"
	);

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		d.camera_in_buffers[i] = gpu_s.device->create_buffer<shaders::common::camera_data>(
			1,
			{
				.usage = gpu::buffer_flag::storage,
			},
			"camera_latch.camera_in"
		);
		d.camera_buffers[i] = gpu_s.device->create_buffer<shaders::common::camera_data>(
			1,
			{
				.usage = gpu::buffer_flag::storage,
				.writable = true,
				.device_local = true,
			},
			"camera_latch.camera"
		);
	}

	return {};
}

auto gse::renderer::camera_latch::frame(context& ctx, const shared_view<gpu::context::data> gpu_s, const data& d, const channel_write<gpu::render_pass_request> pass_out, const shared_view<camera::data> cam_state) -> async::task<> {
	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();
	const auto view = cam_state.view_matrix;
	const auto proj = cam_state.projection_matrix;

	d.camera_in_buffers[frame_index].host_write(shaders::common::camera_data{
		.view = view,
		.proj = proj,
		.inv_view = view.inverse(),
		.inv_view_proj = (proj * view).inverse(),
		.prev_view = cam_state.prev_view_matrix,
		.prev_proj = cam_state.prev_projection_matrix,
		.jitter_ndc = cam_state.jitter_ndc,
		.prev_jitter_ndc = cam_state.prev_jitter_ndc,
	});

	const camera::latch& latch = cam_state.latch;
	const input::mouse_totals consumed = cam_state.latch_consumed;

	auto rec = co_await gpu::pass<^^frame>(pass_out).pipeline(d.pipeline);

	rec.dispatch<entry>(
		{
			.model = static_cast<std::uint32_t>(latch.model),
			.consumed_x = consumed.x,
			.consumed_y = consumed.y,
			.yaw = latch.yaw,
			.pitch = latch.pitch,
			.min_pitch = latch.min_pitch,
			.max_pitch = latch.max_pitch,
			.distance = latch.distance,
			.per_count = latch.per_count,
		},
		{
			.camera_in = d.camera_in_buffers[frame_index],
			.mouse_totals = d.mouse_totals,
			.camera_out = d.camera_buffers[frame_index],
			.history = d.history,
		},
		vec3u{ 1u, 1u, 1u }
	);
}

auto gse::renderer::camera_latch::shutdown(data& d) -> void {
	input::set_mouse_latch_sink(nullptr);
}

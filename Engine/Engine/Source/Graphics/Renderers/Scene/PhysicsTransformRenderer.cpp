module gse.graphics:physics_transform_renderer_impl;

import std;

import :physics_transform_renderer;
import :geometry_collector;


import gse.os;
import gse.assets;
import gse.gpu;
import gse.gpu_record;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.physics;

namespace gse::renderer::physics_transform {
	struct [[= shaders::shader_struct]] push_constants {
		std::uint32_t mapping_count;
		std::uint32_t body_count;
		time_t<float, seconds> interpolation_lag;
		time_t<float, seconds> frame_delta;
	};

	struct transform_bindings {
		shaders::ssbo_readonly<vbd::body_state> body_data;
		shaders::ssbo_readonly<geometry_collector::physics_mapping> mapping_data;
		shaders::ssbo_readwrite<shaders::common::instance_data> instance_data_buffer;
	};

	using entry = gpu::compute_entry<
		gpu::body_path<"Compute/physics_instance_transform">,
		gpu::types<vbd::shader_types>,
		gpu::bindings<transform_bindings>,
		gpu::helpers<"VBDPhysics/vbd_shared">,
		gpu::threads<64>,
		gpu::push_constant<push_constants>
	>;
}

auto gse::renderer::physics_transform::init(context& ctx, const shared_view<gpu::context::data> gpu_s, const shared_view<asset::data> assets_s, data& d) -> async::task<> {
	d.pipeline = gpu::build_compute_program(*gpu_s.device, entry::pod);

	d.initialized = true;

	return {};
}

auto gse::renderer::physics_transform::frame(context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out, const channel_read<physics::gpu_solver_frame_info, geometry_collector::render_data, physics::interpolation_state> frame_in, shared_view<geometry_collector::data> gc_r) -> async::task<> {
	const auto& solver_infos = frame_in.of<physics::gpu_solver_frame_info>();

	if (solver_infos.empty()) {
		co_return;
	}

	if (!solver_infos[0].snapshot || solver_infos[0].body_count == 0) {
		co_return;
	}

	const auto& info = solver_infos[0];

	const auto frame_index = gpu_s.render_graph->current_frame();

	const auto& render_items = frame_in.of<geometry_collector::render_data>();

	if (!render_items.empty() && render_items[0].physics_mapping_count > 0) {
		const auto& data = render_items[0];
		const auto required = data.physics_mapping_count * sizeof(geometry_collector::physics_mapping);

		d.cached_mapping_count = data.physics_mapping_count;

		if (d.mapping_buffer_size < required) {
			for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
				d.mapping_buffers[i] = gpu_s.device->create_buffer(
					{
						.size = required,
						.stride = sizeof(geometry_collector::physics_mapping),
						.usage = gpu::buffer_flag::storage,
					}
				);
			}
			d.mapping_buffer_size = required;
		}
		d.mapping_buffers[frame_index].host_write(data.physics_mappings.data(), required);
	}

	if (d.cached_mapping_count == 0) {
		co_return;
	}

	physics::interpolation_state interpolation;
	if (const auto& published = frame_in.of<physics::interpolation_state>(); !published.empty()) {
		interpolation = published[0];
	}
	const auto interpolation_lag = physics::render_lag(interpolation);

	if (!d.body_views[frame_index].valid()) {
		d.body_views[frame_index] = gpu_s.device->allocate_buffer_slot();
	}
	gpu_s.device->write_storage_buffer(d.body_views[frame_index].slot(), info.snapshot, info.body_count * info.body_stride);

	auto rec = co_await gpu::pass<^^frame>(pass_out)
		.pipeline(d.pipeline)
		.after<^^geometry_collector::frame, ^^vbd::vbd_render_mirror_stage>();

	rec.dispatch<entry>(
		{
			.mapping_count = d.cached_mapping_count,
			.body_count = info.body_count,
			.interpolation_lag = interpolation_lag,
			.frame_delta = system_clock::dt(),
		},
		{
			.body_data = d.body_views[frame_index],
			.mapping_data = d.mapping_buffers[frame_index],
			.instance_data_buffer = gc_r.instance_buffer[frame_index],
		},
		gpu::group_count<entry>(d.cached_mapping_count)
	);
}
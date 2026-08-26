export module gse.graphics:physics_transform_renderer;

import std;

import :geometry_collector;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.physics;
import gse.gpu_record;

export namespace gse::renderer::physics_transform {
	struct [[= system_state<"PhysicsTransform">{}]] data {
		gpu::shader_program pipeline;
		bool initialized = false;

		per_frame_resource<gpu::buffer> mapping_buffers;
		std::size_t mapping_buffer_size = 0;
		std::uint32_t cached_mapping_count = 0;

		per_frame_resource<gpu::bindless_handle> body_views;
	};

	[[= system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		shared_view<asset::data> assets_s,
		data& d
	) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		channel_write<gpu::render_pass_request> pass_out,
		channel_read<physics::gpu_solver_frame_info, geometry_collector::render_data, physics::interpolation_state> frame_in,
		shared_view<geometry_collector::data> gc_r
	) -> async::task<>;
}
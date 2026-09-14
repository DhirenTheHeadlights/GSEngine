export module gse.graphics:cull_compute_renderer;

import gse.assets;
import gse.concurrency;
import gse.ecs;
import gse.gpu;
import gse.gpu_record;

import :geometry_collector;

export namespace gse::renderer::cull_compute {
	struct [[= system_state<"CullCompute">{}]] data {
		bool enabled = true;

		gpu::shader_program pipeline;
		per_frame_resource<gpu::buffer> frustum_buffer;
		per_frame_resource<gpu::buffer> batch_info_buffer;
	};

	[[= system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		shared_view<asset::data> assets_s,
		shared_view<geometry_collector::data> gc_r,
		data& d
	) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		shared_view<geometry_collector::data> gc_r,
		const data& d,
		channel_write<gpu::render_pass_request> pass_out,
		channel_read<geometry_collector::render_data> geometry_in
	) -> async::task<>;
}
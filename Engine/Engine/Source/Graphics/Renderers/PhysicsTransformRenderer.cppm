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

export namespace gse::renderer::physics_transform {
	struct system {
		struct data {
			gpu::pipeline pipeline;
			bool initialized = false;

			per_frame_resource<gpu::descriptor_region> descriptors;

			per_frame_resource<gpu::buffer> mapping_buffers;
			std::size_t mapping_buffer_size = 0;
			std::uint32_t cached_mapping_count = 0;
		};

		static auto run(run_context& ctx, const gpu::context::data& gpu_s, const asset::data& assets_s, data& d)
			-> async::task<>;

		static auto frame(
			frame_context& ctx,
			shared_view<gpu::context> gpu_s,
			data& d,
			shared_view<geometry_collector::system> gc_r
		) -> async::task<>;
	};
}

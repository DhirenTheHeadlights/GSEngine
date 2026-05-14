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
		struct resources {
			gpu::pipeline pipeline;
			bool initialized = false;
		};

		struct frame_data {
			per_frame_resource<gpu::descriptor_region> descriptors;

			per_frame_resource<gpu::buffer> mapping_buffers;
			std::size_t mapping_buffer_size = 0;
			std::uint32_t cached_mapping_count = 0;
		};

		static auto run(
			run_context& ctx,
			const gpu::context::state& gpu_s,
			const asset::state& assets_s,
			resources& r,
			frame_data& fd
		) -> async::task<>;

		static auto frame(
			frame_context& ctx,
			const gpu::context::state& gpu_s,
			const resources& r,
			frame_data& fd,
			const geometry_collector::system::resources& gc_r
		) -> async::task<>;
	};
}

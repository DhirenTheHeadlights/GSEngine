export module gse.graphics:rt_shadow_renderer;

import std;

import :geometry_collector;
import :mesh;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.log;

export namespace gse::renderer::rt_shadow {
	struct system {
		struct data {
			per_frame_resource<const gpu::tlas*> tlas_ptrs{};

			std::unordered_map<const mesh*, gpu::blas> blas_cache;
			per_frame_resource<gpu::tlas> tlas_per_frame;
			per_frame_resource<linear_vector<gpu::tlas_instance_desc>> instances;

			gpu::shader_program tlas_update_pipeline;
			per_frame_resource<gpu::descriptor_region> tlas_update_descriptors;
			per_frame_resource<gpu::buffer> mapping_buffers;
			std::size_t mapping_buffer_capacity = 0;
		};

		static auto run(
			run_context& ctx,
			const gpu::context::data& gpu_s,
			const asset::data& assets_s,
			data& d
		) -> async::task<>;

		static auto frame(
			frame_context& ctx,
			shared_view<gpu::context> gpu_s,
			data& d,
			shared_view<geometry_collector::system> gc_r
		) -> async::task<>;
	};
}

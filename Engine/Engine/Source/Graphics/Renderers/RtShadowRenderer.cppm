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
	using blas_key = std::pair<id, std::uint32_t>;

	struct blas_entry {
		const mesh* source = nullptr;
		gpu::blas blas;
	};

	struct [[= gse::system_state<"RtShadow">{}]] data {
		[[= gse::shared]] per_frame_resource<const gpu::tlas*> tlas_ptrs{};

		std::flat_map<blas_key, blas_entry> blas_cache;
		per_frame_resource<gpu::buffer> blas_scratch;
		per_frame_resource<gpu::tlas> tlas_per_frame;
		per_frame_resource<linear_vector<gpu::tlas_instance_desc>> instances;

		gpu::shader_program tlas_update_pipeline;
		per_frame_resource<gpu::buffer> mapping_buffers;
		std::size_t mapping_buffer_capacity = 0;

		per_frame_resource<gpu::bindless_handle> tlas_instance_views;
	};

	[[= gse::system_init{}]]
	auto init(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		shared_view<asset::data> assets_s,
		data& d
	) -> async::task<>;

	[[= gse::system_frame{}]]
	auto frame(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		shared_view<geometry_collector::data> gc_r
	) -> async::task<>;
}

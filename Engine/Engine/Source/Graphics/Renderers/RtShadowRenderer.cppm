export module gse.graphics:rt_shadow_renderer;

import std;

import :geometry_collector;
import :physics_transform_renderer;
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
	constexpr std::uint32_t max_instances = 4096;

	struct state {
		per_frame_resource<const gpu::tlas*> tlas_ptrs{};

		[[nodiscard]] auto tlas(
			const std::uint32_t frame_index
		) const -> const gpu::tlas& {
			return *tlas_ptrs[frame_index];
		}
	};

	struct system {
		struct frame_data {
			std::unordered_map<const mesh*, gpu::blas> blas_cache;
			per_frame_resource<gpu::tlas> tlas_per_frame;
			per_frame_resource<linear_vector<gpu::tlas_instance_desc>> instances;

			resource::handle<shader> tlas_update_shader;
			gpu::pipeline tlas_update_pipeline;
			per_frame_resource<gpu::descriptor_region> tlas_update_descriptors;
			per_frame_resource<gpu::buffer> mapping_buffers;
			std::size_t mapping_buffer_capacity = 0;
		};

		static auto initialize(
			const init_context& phase,
			frame_data& fd,
			state& s
		) -> void;

		static auto frame(
			frame_context& ctx,
			frame_data& fd,
			const state& s,
			const geometry_collector::state& gc_s
		) -> async::task<>;
	};
}

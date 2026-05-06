export module gse.graphics:cull_compute_renderer;

import std;

import :geometry_collector;
import :skin_compute_renderer;
import :camera_system;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.math;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;

export namespace gse::renderer::cull_compute {
	struct system {
		struct state {
			bool enabled = true;
		};

		struct resources {
			resource::handle<shader> shader_handle;
			gpu::pipeline pipeline;
			per_frame_resource<gpu::descriptor_region> normal_descriptors;
			per_frame_resource<gpu::descriptor_region> skinned_descriptors;
			per_frame_resource<gpu::buffer> frustum_buffer;
			per_frame_resource<gpu::buffer> batch_info_buffer;
			std::uint32_t batch_stride = 0;
			std::unordered_map<std::string, std::uint32_t> batch_offsets;
		};

		static auto initialize(
			const init_context& phase,
			const gpu::context::state& gpu_s,
			const geometry_collector::system::resources& gc_r,
			resources& r,
			state& s
		) -> void;

		static auto frame(
			frame_context& ctx,
			const resources& r
		) -> async::task<>;
	};
}

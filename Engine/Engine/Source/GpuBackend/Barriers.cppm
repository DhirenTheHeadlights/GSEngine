export module gse.gpu_backend:barriers;

import std;

import :core;
import :enums;
import :sync;
import :image;

export namespace gse::gpu {
	struct image_discard {
		pipeline_stage_flags src_stages;
		access_flags src_access;
		pipeline_stage_flags dst_stages;
		access_flags dst_access;
		handle<gpu::image> image;
		image_aspect_flags aspects;
	};

	struct dependency_info {
		std::span<const memory_barrier> memory_barriers;
		std::span<const image_discard> image_discards;
	};
}

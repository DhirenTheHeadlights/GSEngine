export module gse.gpu_backend:barriers;

import std;

import :core;
import :enums;
import :sync;
import :buffer;
import :image;

export namespace gse::gpu {
	struct buffer_barrier {
		pipeline_stage_flags src_stages;
		access_flags src_access;
		pipeline_stage_flags dst_stages;
		access_flags dst_access;
		gpu::handle<gpu::buffer> buffer;
		device_size offset = 0;
		device_size size = 0;
	};

	struct image_barrier {
		pipeline_stage_flags src_stages;
		access_flags src_access;
		pipeline_stage_flags dst_stages;
		access_flags dst_access;
		bool discard_contents = false;
		gpu::handle<gpu::image> image;
		image_aspect_flags aspects;
		std::uint32_t base_mip_level = 0;
		std::uint32_t level_count = 1;
		std::uint32_t base_array_layer = 0;
		std::uint32_t layer_count = 1;
	};

	struct dependency_info {
		std::span<const memory_barrier> memory_barriers;
		std::span<const buffer_barrier> buffer_barriers;
		std::span<const image_barrier> image_barriers;
	};
}

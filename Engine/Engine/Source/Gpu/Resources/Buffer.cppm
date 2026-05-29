export module gse.gpu:buffer;

import std;

import :aliases;
import :gpu_task;
import :sync_token;
import :transient_api;
import :device;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;

export namespace gse::gpu {
	struct buffer_upload {
		const buffer* dst = nullptr;
		const void* data = nullptr;
		std::size_t size = 0;
		std::size_t dst_offset = 0;
	};

	auto upload_to_buffers(
		gpu::device& dev,
		std::span<const buffer_upload> uploads
	) -> sync_token;
}

namespace gse {
	struct upload_entry {
		gpu::buffer_handle dst;
		const void* data;
		gpu::device_size size;
		gpu::device_size offset;
	};

	auto upload_to_buffers_async(
		gpu::device& dev,
		std::vector<upload_entry> entries
	) -> async::task<gpu::sync_token>;
}

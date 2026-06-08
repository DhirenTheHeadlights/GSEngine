export module gse.gpu:buffer;

import std;

import :aliases;
import :gpu_task;
import :sync_token;
import :device;

import gse.core;
import gse.containers;
import gse.time;
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

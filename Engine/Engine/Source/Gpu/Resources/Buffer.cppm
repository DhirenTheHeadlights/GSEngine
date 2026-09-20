export module gse.gpu:buffer;

import gse.containers;
import gse.core;
import gse.diag;
import gse.gpu_backend;
import gse.time;
import std;

import :device;
import :sync_token;

export namespace gse::gpu {
	struct buffer_upload {
		const buffer* dst = nullptr;
		const void* data = nullptr;
		std::size_t size = 0;
		std::size_t dst_offset = 0;
	};

	auto upload_to_buffers(
		device& dev,
		std::span<const buffer_upload> uploads
	) -> sync_token;
}
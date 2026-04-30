export module gse.gpu:buffer;

import std;

import :handles;
import :gpu_task;
import :transient_api;
import :types;
import :vulkan_device;
import :vulkan_buffer;
import :vulkan_commands;
import :device;

import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;

export namespace gse::gpu {
	struct descriptor_buffer_binding {
		std::uint32_t binding = 0;
		const vulkan::buffer* buf = nullptr;
		std::size_t offset = 0;
		std::size_t range = 0;
	};

	struct buffer_upload {
		const vulkan::buffer* dst = nullptr;
		const void* data = nullptr;
		std::size_t size = 0;
		std::size_t dst_offset = 0;
	};

	auto upload_to_buffers(
		gpu::device& dev,
		std::span<const buffer_upload> uploads
	) -> void;
}

namespace gse {
	struct upload_entry {
		gpu::handle<vulkan::buffer> dst;
		const void* data;
		gpu::device_size size;
		gpu::device_size offset;
	};

	auto upload_to_buffers_async(
		gpu::device& dev,
		std::vector<upload_entry> entries
	) -> async::task<>;
}

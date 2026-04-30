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

auto gse::upload_to_buffers_async(gpu::device& dev, std::vector<upload_entry> entries) -> async::task<> {
	std::vector<vulkan::buffer> stagings;
	stagings.reserve(entries.size());
	for (const auto& e : entries) {
		stagings.push_back(dev.allocator().create_buffer(
			gpu::buffer_create_info{
				.size = e.size,
				.usage = gpu::buffer_flag::transfer_src,
			},
			e.data
		));
	}

	auto cmd = co_await begin_transient(dev, gpu::queue_id::graphics);

	for (std::size_t i = 0; i < entries.size(); ++i) {
		vulkan::commands(cmd.handle()).copy_buffer(
			stagings[i].handle(),
			entries[i].dst,
			gpu::buffer_copy_region{
				.src_offset = 0,
				.dst_offset = entries[i].offset,
				.size = entries[i].size,
			}
		);
	}

	co_await submit(dev, std::move(cmd), gpu::queue_id::graphics)
		.retain(std::move(stagings));
}

auto gse::gpu::upload_to_buffers(gpu::device& dev, const std::span<const buffer_upload> uploads) -> void {
	if (uploads.empty()) {
		return;
	}

	std::vector<upload_entry> entries;
	entries.reserve(uploads.size());
	for (const auto& [dst, data, size, dst_offset] : uploads) {
		entries.push_back({
			.dst = dst->handle(),
			.data = data,
			.size = size,
			.offset = dst_offset,
		});
	}

	dispatch(dev, upload_to_buffers_async(dev, std::move(entries)));
}

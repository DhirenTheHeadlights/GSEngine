module gse.gpu:buffer_impl;

import std;

import :buffer;
import :gpu_task;
import :sync_token;
import :device;
import :pass_recorder;

import gse.vulkan;

auto gse::gpu::upload_to_buffers(gpu::device& dev, const std::span<const buffer_upload> uploads) -> sync_token {
	if (uploads.empty()) {
		return {};
	}

	std::vector<gpu::buffer> stagings;
	stagings.reserve(uploads.size());
	for (const auto& u : uploads) {
		stagings.push_back(dev.create_buffer(
			buffer_desc{
				.size = u.size,
				.usage = buffer_flag::transfer_src,
				.data = u.data,
			}
		));
	}

	auto cmd_awaiter = begin_transient(dev, queue_id::graphics, "transient.buffer_upload");
	auto cmd = cmd_awaiter.await_resume();

	for (std::size_t i = 0; i < uploads.size(); ++i) {
		pass_recorder(cmd.handle())
			.copy_buffer(
				stagings[i].handle(),
				uploads[i].dst->handle(),
				buffer_copy_region{
					.src_offset = 0,
					.dst_offset = uploads[i].dst_offset,
					.size = uploads[i].size,
				}
			);
	}

	return submit(dev, std::move(cmd), queue_id::graphics).retain(std::move(stagings)).submit_sync();
}

module gse.gpu;

import std;

import gse.concurrency;

auto gse::upload_to_buffers_async(gpu::device& dev, std::vector<upload_entry> entries) -> async::task<gpu::sync_token> {
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

	auto cmd = co_await begin_transient(dev, gpu::queue_id::graphics, "transient.buffer_upload");

	for (std::size_t i = 0; i < entries.size(); ++i) {
		vulkan::commands(cmd.handle()).copy_buffer(stagings[i].handle(), entries[i].dst, gpu::buffer_copy_region{
																							 .src_offset = 0,
																							 .dst_offset = entries[i].offset,
																							 .size = entries[i].size,
																						 });
	}

	co_return co_await submit(dev, std::move(cmd), gpu::queue_id::graphics)
		.retain(std::move(stagings));
}

auto gse::gpu::upload_to_buffers(gpu::device& dev, const std::span<const buffer_upload> uploads) -> sync_token {
	if (uploads.empty()) {
		return {};
	}

	std::vector<vulkan::buffer> stagings;
	stagings.reserve(uploads.size());
	for (const auto& u : uploads) {
		stagings.push_back(dev.allocator().create_buffer(
			buffer_create_info{
				.size = u.size,
				.usage = buffer_flag::transfer_src,
			},
			u.data
		));
	}

	auto cmd_awaiter = begin_transient(dev, queue_id::graphics, "transient.buffer_upload");
	auto cmd = cmd_awaiter.await_resume();

	for (std::size_t i = 0; i < uploads.size(); ++i) {
		vulkan::commands(cmd.handle()).copy_buffer(stagings[i].handle(), uploads[i].dst->handle(), buffer_copy_region{
																									   .src_offset = 0,
																									   .dst_offset = uploads[i].dst_offset,
																									   .size = uploads[i].size,
																								   });
	}

	return submit(dev, std::move(cmd), queue_id::graphics)
		.retain(std::move(stagings))
		.submit_sync();
}

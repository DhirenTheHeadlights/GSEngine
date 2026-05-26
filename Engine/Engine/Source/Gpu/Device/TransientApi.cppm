export module gse.gpu:transient_api;

import std;

import :aliases;
import :device;
import :gpu_task;

import gse.concurrency;
import gse.core;

export namespace gse::gpu {
	[[nodiscard]]
	auto begin_transient(
		gpu::device& dev,
		queue_id id,
		std::string_view tag = "transient.untagged"
	) -> begin_transient_awaiter;

	[[nodiscard]]
	auto submit(
		gpu::device& dev,
		transient_command_buffer&& cmd,
		queue_id id
	) -> submission;

	auto dispatch(
		gpu::device& dev,
		async::task<> task
	) -> void;
}

auto gse::gpu::begin_transient(gpu::device& dev, const queue_id id, const std::string_view tag) -> begin_transient_awaiter {
	return begin_transient_awaiter{
		.m_gpu_device = &dev,
		.m_queue = &dev.transient().queue(id),
		.m_pass_marker =
			device::pass_marker{
				.pass_type = find_or_generate_id(tag)
			},
	};
}

auto gse::gpu::submit(gpu::device& dev, transient_command_buffer&& cmd, const queue_id id) -> submission {
	return submission(
		dev,
		dev.transient().queue(id),
		dev.transient().bin(),
		std::move(cmd)
	);
}

auto gse::gpu::dispatch(gpu::device& dev, async::task<> task) -> void {
	task.start();
	dev.transient().detach(std::move(task));
}

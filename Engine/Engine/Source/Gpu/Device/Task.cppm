export module gse.gpu:gpu_task;

import std;

import :device;
import :frame_resource_bin;
import :handles;
import :sync_token;
import :transient_queue;
import :types;
import :vulkan_device;
import :vulkan_queues;
import :vulkan_transient_command_buffer;
import :vulkan_transient_command_pool;

import gse.assert;
import gse.concurrency;
import gse.core;

export namespace gse::gpu {
	template <typename T = void>
	using gpu_task = async::task<T>;

	struct begin_transient_awaiter {
		gpu::device* m_gpu_device;
		transient_queue* m_queue;
		device::pass_marker m_pass_marker;

		auto await_ready() const noexcept -> bool;

		auto await_suspend(std::coroutine_handle<>) noexcept -> bool;

		auto await_resume() -> vulkan::transient_command_buffer;
	};

	class submission {
	public:
		submission(
			gpu::device& gpu_dev,
			vulkan::queue& queues,
			transient_queue& queue,
			frame_resource_bin& bin,
			vulkan::transient_command_buffer&& cmd
		);

		template <typename T>
		auto retain(T&& resource) && -> submission&&;

		auto submit_sync() -> sync_token;

		auto await_ready() noexcept -> bool;

		auto await_suspend(std::coroutine_handle<> caller) -> bool;

		auto await_resume() noexcept -> sync_token;

	private:
		gpu::device* m_gpu_device;
		vulkan::queue* m_queues;
		transient_queue* m_queue;
		frame_resource_bin* m_bin;
		vulkan::transient_command_buffer m_cmd;
		std::vector<move_only_function<void()>> m_pending_retains;
		std::uint64_t m_value = 0;
		bool m_submitted = false;
	};
}

auto gse::gpu::begin_transient_awaiter::await_ready() const noexcept -> bool {
	return true;
}

auto gse::gpu::begin_transient_awaiter::await_suspend(std::coroutine_handle<>) noexcept -> bool {
	return false;
}

auto gse::gpu::begin_transient_awaiter::await_resume() -> vulkan::transient_command_buffer {
	const auto worker = task::current_worker();
	assert(worker.has_value(), "begin_transient must be co_awaited from a task worker thread");
	auto cmd = m_queue->allocate_primary(m_gpu_device->vulkan_device(), *worker);
	cmd.begin_one_time();
	const auto marker =
		m_gpu_device->begin_pass_marker(cmd.handle(), device::pass_marker_domain::transient, m_pass_marker);
	m_gpu_device->checkpoint_pass_marker(cmd.handle(), marker);
	m_gpu_device->post_renderpass_pass_marker(cmd.handle(), marker);
	cmd.set_marker_seq(marker.seq);
	return cmd;
}

gse::gpu::submission::submission(
	gpu::device& gpu_dev,
	vulkan::queue& queues,
	transient_queue& queue,
	frame_resource_bin& bin,
	vulkan::transient_command_buffer&& cmd
)
	: m_gpu_device(&gpu_dev),
	  m_queues(&queues),
	  m_queue(&queue),
	  m_bin(&bin),
	  m_cmd(std::move(cmd)) {
}

template <typename T>
auto gse::gpu::submission::retain(T&& resource) && -> submission&& {
	m_pending_retains.push_back(
		[bin = m_bin, qid = m_queue->id(), value = std::ref(m_value), payload = std::forward<T>(resource)]() mutable {
			bin->retain(qid, value.get(), std::move(payload));
		}
	);
	return std::move(*this);
}

auto gse::gpu::submission::submit_sync() -> sync_token {
	if (m_submitted) {
		return sync_token{ m_queue, m_value };
	}

	if (m_cmd.marker_seq() != std::numeric_limits<std::uint64_t>::max()) {
		m_gpu_device->end_pass_marker(
			m_cmd.handle(),
			device::pass_marker_handle{ .seq = m_cmd.marker_seq(), .domain = device::pass_marker_domain::transient }
		);
	}

	m_cmd.end();

	{
		auto ticket = m_queue->reserve_for_submit();
		m_value = ticket.value;

		const semaphore_submit_info signal{
			.semaphore = m_queue->timeline_handle(),
			.value = m_value,
			.stages = pipeline_stage_flag::all_commands,
		};
		const command_buffer_submit_info cmd_info{
			.command_buffer = m_cmd.handle(),
		};
		const submit_info info{
			.command_buffers = std::span(&cmd_info, 1),
			.signal_semaphores = std::span(&signal, 1),
		};

		if (m_queue->id() == queue_id::graphics) {
			m_queues->submit_graphics(info);
		}
		else {
			m_queues->submit_compute(info);
		}
	}
	m_submitted = true;

	if (auto* pool = m_cmd.origin_pool()) {
		pool->mark_in_use_until(m_value);
	}

	for (auto& fn : m_pending_retains) {
		fn();
	}
	m_pending_retains.clear();

	return sync_token{ m_queue, m_value };
}

auto gse::gpu::submission::await_ready() noexcept -> bool {
	return false;
}

auto gse::gpu::submission::await_suspend(std::coroutine_handle<> caller) -> bool {
	submit_sync();

	if (m_queue->reached(m_value)) {
		return false;
	}

	m_queue->park(m_value, caller);
	return true;
}

auto gse::gpu::submission::await_resume() noexcept -> sync_token {
	return sync_token{ m_queue, m_value };
}

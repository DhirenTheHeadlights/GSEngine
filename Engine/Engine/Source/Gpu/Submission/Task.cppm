export module gse.gpu:gpu_task;

import std;

import gse.gpu_backend;
import :device;
import :sync_token;

import gse.assert;
import gse.concurrency;
import gse.core;

export namespace gse::gpu {
	class submission {
	public:
		submission(
			gpu::device& gpu_dev,
			transient_queue<device>& queue,
			frame_resource_bin& bin,
			gpu::transient_command_buffer&& cmd
		);

		template <typename T>
		auto retain(
			T&& resource
		) && -> submission&&;

		auto submit_sync() -> sync_token;

		auto retain(
			buffer&& resource
		) && -> submission&&;

		auto retain(
			std::vector<buffer>&& resources
		) && -> submission&&;

	private:
		gpu::device* m_gpu_device;
		transient_queue<device>* m_queue;
		frame_resource_bin* m_bin;
		gpu::transient_command_buffer m_cmd;
		std::vector<std::unique_ptr<frame_resource_bin::retained_base>> m_pending_retains;
		std::uint64_t m_value = 0;
		bool m_submitted = false;
	};

	[[nodiscard]]
	auto begin_transient(
		gpu::device& dev,
		queue_id id,
		std::string_view tag = "transient.untagged"
	) -> gpu::transient_command_buffer;

	[[nodiscard]]
	auto submit(
		gpu::device& dev,
		gpu::transient_command_buffer&& cmd,
		queue_id id
	) -> submission;
}

gse::gpu::submission::submission(gpu::device& gpu_dev, transient_queue<device>& queue, frame_resource_bin& bin, gpu::transient_command_buffer&& cmd)
	: m_gpu_device(&gpu_dev), m_queue(&queue), m_bin(&bin), m_cmd(std::move(cmd)) {
}

auto gse::gpu::submission::retain(buffer&& resource) && -> submission&& {
	m_pending_retains.push_back(std::make_unique<frame_resource_bin::retained_holder<buffer>>(std::move(resource)));
	return std::move(*this);
}

auto gse::gpu::submission::retain(std::vector<buffer>&& resources) && -> submission&& {
	m_pending_retains.push_back(std::make_unique<frame_resource_bin::retained_holder<std::vector<buffer>>>(std::move(resources)));
	return std::move(*this);
}

template <typename T>
auto gse::gpu::submission::retain(T&& resource) && -> submission&& {
	using resource_type = std::decay_t<T>;
	m_pending_retains.push_back(std::make_unique<frame_resource_bin::retained_holder<resource_type>>(std::forward<T>(resource)));
	return std::move(*this);
}

auto gse::gpu::submission::submit_sync() -> sync_token {
	if (m_submitted) {
		return sync_token{ &m_queue->station(), m_value };
	}

	if (m_cmd.marker_seq() != std::numeric_limits<std::uint64_t>::max()) {
		m_gpu_device->end_pass_marker(
			m_cmd.handle(),
			device::pass_marker_handle{
				.seq = m_cmd.marker_seq(),
				.domain = device::pass_marker_domain::transient
			}
		);
	}

	m_gpu_device->end_commands(m_cmd.handle());

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

		m_gpu_device->submit(m_queue->id() == queue_id::graphics ? queue_type::graphics : queue_type::compute, info);
	}
	m_submitted = true;

	m_queue->mark_in_use(m_cmd.worker_index(), m_value);

	for (auto& pending : m_pending_retains) {
		m_bin->retain(m_queue->id(), m_value, std::move(pending));
	}
	m_pending_retains.clear();
	
	return sync_token{ &m_queue->station(), m_value };
}

auto gse::gpu::begin_transient(gpu::device& dev, const queue_id id, const std::string_view tag) -> gpu::transient_command_buffer {
	const auto worker = task::current_worker();
	assert(worker.has_value(), "begin_transient must be called from a task worker thread");
	auto& queue = dev.transient().queue(id);
	auto cmd = queue.allocate_primary(*worker);
	dev.begin_one_time_commands(cmd.handle());
	const auto marker = dev.begin_pass_marker(
		cmd.handle(),
		device::pass_marker_domain::transient,
		device::pass_marker{
			.pass_type = find_or_generate_id(tag)
		},
		tag
	);
	dev.checkpoint_pass_marker(cmd.handle(), marker);
	dev.post_renderpass_pass_marker(cmd.handle(), marker);
	cmd.set_marker_seq(marker.seq);
	return cmd;
}

auto gse::gpu::submit(gpu::device& dev, gpu::transient_command_buffer&& cmd, const queue_id id) -> submission {
	return submission(dev, dev.transient().queue(id), dev.transient().bin(), std::move(cmd));
}
